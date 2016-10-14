/*
 * sfiniteintegrate.c: Buffered Finite Integration (Moving average)
 *
 * Takes buffered input stream, "x", averages over a number
 * of samples (INTEGRATION_TIME), and outputs "y".  The initial
 * value can be specified, but "prehistory" inputs are assumed
 * to be zero.
 *
 * The input vector has width n.
 *
 * The RWork vector holds the current y values.  Simulink
 * state elements carry over the final INTEGRATION_TIME inputs.
 * The states are organized as a circular buffer.
 *
 * Parameters are:
 *		Size of buffer
 *      Initial value
 *      Integration time
 *      Normalize
 *      Number of channels
 *      Multichannel order (1= column major, 0 = row major)
 *
 * Author:
 *		Steve Mitchell
 *		Dept. of Electrical Engineering
 *		Rhodes Hall
 *		Cornell University
 *		Ithaca, NY  14853
 *
 * Revision:
 *    2/10/98     SGM      Initial version
 *    4/24/98     SGM      Added column-major multichannel storage
 *                         Converted to Level 2 S-function
 *    1/26/99     SGM      Added normalization feature
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME sfiniteintegrate
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>



/* Simulink block parameters */
enum{
	kBUFFER_SIZE,			/* Input width													*/
	kINITIAL_VALUE,			/* Initial integrator value										*/
	kINTEGRATION_TIME,		/* Length to average											*/
	kNORMALIZE,				/* Whether to divide output by number of integration samples	*/
	kNUM_CHANNELS,			/* Number of channels											*/
	kCOL_MAJOR,				/* Nonzero if multi channels are stored as columns				*/
	kNUM_PARAMETERS
};

#define BUFFER_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define INITIAL_VALUE		(                  *mxGetPr(ssGetSFcnParam (S, kINITIAL_VALUE)))
#define INTEGRATION_TIME	((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kINTEGRATION_TIME))))
#define NORMALIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNORMALIZE))))
#define NUM_CHANNELS		((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNUM_CHANNELS))))
#define COL_MAJOR			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kCOL_MAJOR))))

/* Integer work vector */
enum{
	kFORMER_INDEX,			/* Position of circular buffer		*/
	kNUMIWORKITEMS
};

#define GET_FORMER_INDEX		(ssGetIWorkValue (S, kFORMER_INDEX))
#define SET_FORMER_INDEX(x)		(ssSetIWorkValue (S, kFORMER_INDEX, x))

/* Real work vector */
enum{
	kVALUE,					/* Value from previous iteration	*/
	kNUMRWORKITEMS
};

#define GET_VALUE(channel)		(ssGetRWorkValue (S, kVALUE+(channel)))
#define SET_VALUE(channel,x)	(ssSetRWorkValue (S, kVALUE+(channel), (x)))


/* States */
#define FORMER_VALUE(channel)		(x[channel])
#define RAW_FORMER_INPUT(channel,i)	(x[numChannels*((i)+1)+(channel)])

/* Macros */
#define MIN(x,y)				((x)<(y) ? (x) : (y))
#define RETURN_IF_ERROR			if (ssGetErrorStatus (S) != NULL) return;
#define ERROR_STRING(a)			a

/* Prototypes */
static void mdlCheckParameters (SimStruct *S);


/*====================*
 * S-function methods *
 *====================*/

/* Function: mdlInitializeSizes ===============================================
 * Abstract:
 *
 * The sizes information is used by SIMULINK to determine the S-function 
 * block's characteristics (number of inputs, outputs, states, etc.).
 * 
 */
static void mdlInitializeSizes(SimStruct *S)
{

	ssSetNumSFcnParams (S, kNUM_PARAMETERS);  /* Number of expected parameters */

#if defined(MATLAB_MEX_FILE)
	if (ssGetNumSFcnParams (S) != ssGetSFcnParamsCount (S)) 
		return;

	mdlCheckParameters (S);		RETURN_IF_ERROR;
#endif

    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, (1+INTEGRATION_TIME) * NUM_CHANNELS);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 1)) return;
	if (!ssSetNumOutputPorts (S, 1)) return;

    ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
									/* number of inputs                      */
    ssSetOutputPortWidth(  S, 0, BUFFER_SIZE * NUM_CHANNELS);	
									/* number of outputs                     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
									/* direct feedthrough flag               */
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, NUM_CHANNELS);   
									/* number of real work vector elements   */
    ssSetNumIWork(         S, kNUMIWORKITEMS);
									/* number of integer work vector elements*/
    ssSetNumPWork(         S, 0);   /* number of pointer work vector elements*/
    ssSetNumModes(         S, 0);   /* number of mode work vector elements   */
    ssSetNumNonsampledZCs( S, 0);   /* number of nonsampled zero crossings   */
#if 1
    ssSetOptions(          S, 0);	/* general options (SS_OPTION_xx)        */
#else
	/* We don't ever call mexErrMsgTxt, any mex... functions,
	   or otherwise cause a "long jump"
	 */
	ssSetOptions (S, SS_OPTION_EXCEPTION_FREE_CODE);
#endif
}



/* Function: mdlInitializeSampleTimes =========================================
 * Abstract:
 *
 * This function is used to specify the sample time(s) for your S-function.
 * You must register the same number of sample times as specified in 
 * ssSetNumSampleTimes. 
 */
static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, FIXED_IN_MINOR_STEP_OFFSET);
}



/* Function: mdlInitializeConditions ==========================================
 * Abstract:
 *
 * In this function, you should initialize the continuous and discrete
 * states for your S-function block.  The initial states are placed
 * in the x0 variable.  You can also perform any other initialization
 * activities that your S-function may require.
 */
#define MDL_INITIALIZE_CONDITIONS
static void mdlInitializeConditions(SimStruct *S)
{
	long		i, m, channel, numChannels;

	real_T		*x;

	x = ssGetRealDiscStates (S);
	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		FORMER_VALUE(channel) = INITIAL_VALUE;


	SET_FORMER_INDEX (0);

	/* Prehistory inputs are zero */
	m = INTEGRATION_TIME;
	for (i=0; i < m; i++)
	for (channel=0; channel < numChannels; channel++)
		RAW_FORMER_INPUT(channel,i) = 0.0;
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

#define FORMER_INPUT(channel,i)		(RAW_FORMER_INPUT((channel),(formerIndex + (i)) % integrationTime))
#define INPUT_R(channel,i)			(*uPtrs[numChannels*(i)+(channel)])
#define OUTPUT_R(channel,i)			(y     [numChannels*(i)+(channel)])
#define INPUT_C(channel,i)			(*uPtrs[(i)+n*(channel)])
#define OUTPUT_C(channel,i)			(y     [(i)+n*(channel)])

static void mdlOutputs(SimStruct *S, int_T tid)
{
	int_T				i, n, m, integrationTime, formerIndex, channel, numChannels;
	real_T				*x, *y; 
	InputRealPtrsType	uPtrs;
	real_T				normalizationFactor;

	x = ssGetRealDiscStates (S);
	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);
	y = ssGetOutputPortSignal (S, 0);

	numChannels     = NUM_CHANNELS;
	n               = BUFFER_SIZE;
	integrationTime = INTEGRATION_TIME;
	formerIndex     = GET_FORMER_INDEX;

	if (NORMALIZE)
		 normalizationFactor = 1.0 / integrationTime;
	else
		 normalizationFactor = 1.0;

	/* Copy values from previous buffer */
	for (channel=0; channel < numChannels; channel++)
		SET_VALUE (channel, FORMER_VALUE(channel));

	if (COL_MAJOR)
	{
		/* Former inputs are saved in state vector */
		m = MIN(n, integrationTime);
		for (i=0; i < m; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_VALUE (channel, GET_VALUE(channel) + INPUT_C(channel,i) - FORMER_INPUT(channel,i));
			OUTPUT_C(channel,i) = GET_VALUE(channel) * normalizationFactor;
		}

		/* If state vector is used up, continue with current inputs */
		for ( ; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_VALUE (channel, GET_VALUE(channel) + INPUT_C(channel,i) - INPUT_C(channel,i-integrationTime));
			OUTPUT_C(channel,i) = GET_VALUE(channel) * normalizationFactor;
		}
	}

	else /* ROW MAJOR */
	{
		/* Former inputs are saved in state vector */
		m = MIN(n, integrationTime);
		for (i=0; i < m; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_VALUE (channel, GET_VALUE(channel) + INPUT_R(channel,i) - FORMER_INPUT(channel,i));
			OUTPUT_R(channel,i) = GET_VALUE(channel) * normalizationFactor;
		}

		/* If state vector is used up, continue with current inputs */
		for ( ; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_VALUE (channel, GET_VALUE(channel) + INPUT_R(channel,i) - INPUT_R(channel,i-integrationTime));
			OUTPUT_R(channel,i) = GET_VALUE(channel) * normalizationFactor;
		}
	}
}



/* Function: mdlUpdate ========================================================
 * Abstract:
 *
 * This function is called once for every major integration time step.
 * Discrete states are typically updated here, but this function is useful
 * for performing any tasks that should only take place once per integration
 * step.
 */
#define MDL_UPDATE
static void mdlUpdate(SimStruct *S, int_T tid)
{
	int_T				i, n, integrationTime, formerIndex, channel, numChannels;
	real_T				*x; 
	InputRealPtrsType	uPtrs;

	x     = ssGetRealDiscStates (S);
	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);

	n               = BUFFER_SIZE;
	integrationTime = INTEGRATION_TIME;
	formerIndex     = GET_FORMER_INDEX;
	numChannels     = NUM_CHANNELS;


	/* Copy final value (why?) */
	for (channel=0; channel < numChannels; channel++)
		FORMER_VALUE(channel) = GET_VALUE (channel);

	if (COL_MAJOR)
	{
		/* Save last inputs */
		if (integrationTime <= n)
		{
			SET_FORMER_INDEX (0);

			for (i=0; i < integrationTime; i++)
			for (channel=0; channel < numChannels; channel++)
				RAW_FORMER_INPUT(channel,i) = INPUT_C(channel, n-integrationTime+i);
		}
		else
		{
			for (i=0; i < n; i++)
			for (channel=0; channel < numChannels; channel++)
				FORMER_INPUT(channel,i) = INPUT_C(channel, i);

			SET_FORMER_INDEX ((formerIndex + n) % integrationTime);
		}
	}

	else /* ROW MAJOR */
	{
		/* Save last inputs */
		if (integrationTime <= n)
		{
			SET_FORMER_INDEX (0);

			for (i=0; i < integrationTime; i++)
			for (channel=0; channel < numChannels; channel++)
				RAW_FORMER_INPUT(channel,i) = INPUT_R(channel, n-integrationTime+i);
		}
		else
		{
			for (i=0; i < n; i++)
			for (channel=0; channel < numChannels; channel++)
				FORMER_INPUT(channel,i) = INPUT_R(channel, i);

			SET_FORMER_INDEX ((formerIndex + n) % integrationTime);
		}
	}
}



/* Function: mdlTerminate =====================================================
 * Abstract:
 *
 * In this function, you should perform any actions that are necessary
 * at the termination of a simulation.  For example, if memory was allocated
 * in mdlInitializeConditions, this is the place to free it.
 */
static void mdlTerminate(SimStruct *S)
{
}

# if defined(MATLAB_MEX_FILE)
  /* Function: mdlCheckParameters =============================================
   * Abstract:
   *    This routine will be called after mdlInitializeSizes, whenever 
   *    parameters change or get re-evaluated. The purpose of this routine is 
   *    to verify that the new parameter setting are correct.
   *
   *    You should add a call to this routine from mdlInitalizeSizes
   *    to check the parameters. After setting the number of parameters
   *    you expect in your S-function via ssSetNumSFcnParams(S,n), you should:
   *     #if defined(MATLAB_MEX_FILE)
   *       if (ssGetNumSFcnParams(S) == ssGetSFcnParamsCount(S)) {
   *           mdlCheckParameters(S);
   *           if (ssGetErrorStatus(S) != NULL) return;
   *       } else {
   *           return;     Simulink will report a parameter mismatch error
   *       }
   *     #endif
   *    See matlabroot/simulink/src/sfun_errhdl.c for an example.    
   *
   *    Your work vectors, for example RWork can be modified based upon
   *    parameter changes, however you must first verify that any work
   *    vectors such as RWork are non-NULL. 
   *
   *    When a Simulation is running, changes to S-function parameters 
   *    can occur either at the start of a simulation step, or during a 
   *    simulation step. When changes to S-function parameters occur during 
   *    a simulation step, this routine is called twice, for the same 
   *    parameter changes. The first call during the simulation step is used 
   *    to verify that the parameters are correct. After verifying the
   *    new parameters, the simulation continues using the original parameter 
   *    values until the next simulation step at which time the new parameter
   *    values will be used. During the first call, the work vectors, 
   *    e.g. RWork will be NULL indicating that you should only check the 
   *    parameters. The second call occurs after the simulation step is 
   *    complete, i.e. at the start of the next simulation step. During the
   *    second call, the work vectors will be non-NULL allowing you to
   *    modify them based upon the new parameter values. Changing parameters
   *    at the start of a simulation step, prevents numerical difficulties in 
   *    the solvers due to abrupt parameter changes during a simulation step.
   */
# define MDL_CHECK_PARAMETERS

static void mdlCheckParameters (SimStruct *S)
{
	long		i;

	for (i=0; i < kNUM_PARAMETERS; i++)
		if (mxGetNumberOfElements (ssGetSFcnParam(S,i)) != 1)
		{
			ssSetErrorStatus (S, ERROR_STRING ("Parameter must be a scalar"));
			return;
		}


	if (BUFFER_SIZE <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Input width must be positive"));
		return;
	}

	if (INTEGRATION_TIME <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Integration time must be positive"));
		return;
	}

	if (NUM_CHANNELS <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Number of channels must be positive"));
		return;
	}
}
# endif



/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef	MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
