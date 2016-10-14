/*
 * splimflipflop.c: Buffered RS Flip-Flop with output pulse duration limiting
 *
 * Takes buffered boolean input streams, "Reset" and "Set",
 * and outputs a boolean "Q".  Reset and Set take effect
 * on the current sample (direct feedthrough).  Reset 
 * overrides Set, if both are present.
 *
 * The flip-flop is reset if the pulse duration exceeds maxLimit.
 * If the output goes high, it will not go low again until
 * at least minLimit samples have gone by.  Setting maxLimit
 * to zero is the same as setting it to Inf.
 *
 * The input vector has width 2*n in the order 
 * <reset>, <set>.
 *
 * The RWork vector holds the current state.  One Simulink
 * state element carries over the final/initial flip-flop state.
 *
 * Parameters are:
 *		Size of buffer
 *      Initial state
 *      Minimum pulse duration
 *      Maximum pulse duration
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
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME splimflipflop
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>



/* Simulink block parameters */
enum{
	kBUFFER_SIZE,			/* Input width						*/
	kINITIAL_STATE,			/* Initial flip-flop state			*/
	kMIN_LIMIT,				/* Minimum output pulse duration	*/
	kMAX_LIMIT,				/* Maximum output pulse duartion	*/
	kNUM_CHANNELS,			/* Number of channels				*/
	kCOL_MAJOR,				/* Nonzero if multi channels are stored as columns	*/
	kNUM_PARAMETERS
};

#define BUFFER_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define INITIAL_STATE		(        0.0 !=    *mxGetPr(ssGetSFcnParam (S, kINITIAL_STATE)))
#define MIN_LIMIT			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kMIN_LIMIT))))
#define MAX_LIMIT			 (mxIsInf(*mxGetPr(ssGetSFcnParam (S, kMAX_LIMIT))) ? 0 : \
							((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kMAX_LIMIT)))))
#define NUM_CHANNELS		((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNUM_CHANNELS))))
#define COL_MAJOR			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kCOL_MAJOR))))


/* Macros */
#define RETURN_IF_ERROR			if (ssGetErrorStatus (S) != NULL) return;
#define ERROR_STRING(a)			a


/* Real work vector */
enum{
	kSTATE,					/* State from previous iteration	*/
	kNUMRWORKITEMS
};

#define GET_STATE(channel)		(ssGetRWorkValue (S, kSTATE+(channel)))
#define SET_STATE(channel,x)	(ssSetRWorkValue (S, kSTATE+(channel), (x)))


/* Integer work vector */
enum{
	kCOUNT,					/* Number of samples output has been high */
	kNUMIWORKITEMS
};

#define GET_COUNT(channel)		(ssGetIWorkValue (S, kCOUNT+(channel)))
#define SET_COUNT(channel,x)	(ssSetIWorkValue (S, kCOUNT+(channel), (x)))


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
    ssSetNumDiscStates(    S, 2 * NUM_CHANNELS);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 2)) return;
	if (!ssSetNumOutputPorts (S, 1)) return;

    ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
    ssSetInputPortWidth(   S, 1, BUFFER_SIZE * NUM_CHANNELS);   
									/* number of inputs                      */
    ssSetOutputPortWidth(  S, 0, BUFFER_SIZE * NUM_CHANNELS);	
									/* number of outputs                     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
    ssSetInputPortDirectFeedThrough(S, 1, 1);   
									/* direct feedthrough flag               */
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, NUM_CHANNELS);   
									/* number of real work vector elements   */
    ssSetNumIWork(         S, NUM_CHANNELS);   
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
	long		channel, numChannels, initialCount;
	real_T		*x;

	x = ssGetRealDiscStates (S);

	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		x[channel] = INITIAL_STATE;

	initialCount = INITIAL_STATE ? 1 : 0;
	for (channel=0; channel < numChannels; channel++)
		x[numChannels + channel] = initialCount;
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

#define CHECK_RESET_R(channel,i)		(*resetPtrs [numChannels*(i) + (channel)] != 0.0)
#define CHECK_SET_R(channel,i)			(*setPtrs   [numChannels*(i) + (channel)] != 0.0)
#define OUTPUT_R(channel,i)				(y          [numChannels*(i) + (channel)])
#define CHECK_RESET_C(channel,i)		(*resetPtrs [(i) + n*(channel)] != 0.0)
#define CHECK_SET_C(channel,i)			(*setPtrs   [(i) + n*(channel)] != 0.0)
#define OUTPUT_C(channel,i)				(y          [(i) + n*(channel)])

static void mdlOutputs(SimStruct *S, int_T tid)
{
	int_T				i, n, channel, numChannels, minLimit, maxLimit;
	real_T				*x, *y; 
	InputRealPtrsType	resetPtrs, setPtrs;

	x = ssGetRealDiscStates (S);
	resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
	setPtrs   = ssGetInputPortRealSignalPtrs (S, 1);
	y = ssGetOutputPortSignal (S, 0);

	numChannels = NUM_CHANNELS;
	n = BUFFER_SIZE;
	minLimit = MIN_LIMIT;
	maxLimit = MAX_LIMIT;

	/* Simulink state element holds previous final state	*/
	for (channel=0; channel < numChannels; channel++)
		SET_STATE(channel, x[channel]);	

	/* ...and previous output count							*/
	for (channel=0; channel < numChannels; channel++)
		SET_COUNT(channel, (int_T) floor(0.5+x[numChannels + channel]));	

	if (COL_MAJOR)
	{
		if (maxLimit != 0 && minLimit != 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_C (channel,i) || GET_COUNT(channel) >= maxLimit)
						SET_STATE(channel,0.0);
					else if (CHECK_SET_C (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel) && 
						(GET_COUNT(channel) == 0 || GET_COUNT(channel) >= minLimit))
					{
						OUTPUT_C(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_C(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else if (maxLimit != 0 && minLimit == 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_C (channel,i) || GET_COUNT(channel) >= maxLimit)
						SET_STATE(channel,0.0);
					else if (CHECK_SET_C (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel))
					{
						OUTPUT_C(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_C(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else if (maxLimit == 0 && minLimit != 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_C (channel,i))
						SET_STATE(channel,0.0);
					else if (CHECK_SET_C (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel) && 
						(GET_COUNT(channel) == 0 || GET_COUNT(channel) >= minLimit))
					{
						OUTPUT_C(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_C(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else /* No output pulse duration limits */
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_C (channel,i))
						SET_STATE(channel,0.0);
					else if (CHECK_SET_C (channel,i))
						SET_STATE(channel,1.0);
					
					OUTPUT_C(channel,i) = GET_STATE(channel);
				}
		}
	}

	else /* ROW MAJOR */
	{
		if (maxLimit != 0 && minLimit != 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_R (channel,i) || GET_COUNT(channel) >= maxLimit)
						SET_STATE(channel,0.0);
					else if (CHECK_SET_R (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel) && 
						(GET_COUNT(channel) == 0 || GET_COUNT(channel) >= minLimit))
					{
						OUTPUT_R(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_R(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else if (maxLimit != 0 && minLimit == 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_R (channel,i) || GET_COUNT(channel) >= maxLimit)
						SET_STATE(channel,0.0);
					else if (CHECK_SET_R (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel))
					{
						OUTPUT_R(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_R(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else if (maxLimit == 0 && minLimit != 0)
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_R (channel,i))
						SET_STATE(channel,0.0);
					else if (CHECK_SET_R (channel,i))
						SET_STATE(channel,1.0);
					
					if (!GET_STATE(channel) && 
						(GET_COUNT(channel) == 0 || GET_COUNT(channel) >= minLimit))
					{
						OUTPUT_R(channel,i) = 0.0;
						SET_COUNT(channel, 0);
					}
					else
					{
						OUTPUT_R(channel,i) = 1.0;
						SET_COUNT(channel,GET_COUNT(channel)+1);
					}
				}
		}
		else /* No output pulse duration limits */
		{
			for (i=0; i < n; i++)
				for (channel=0; channel < numChannels; channel++)
				{
					if (CHECK_RESET_R (channel,i))
						SET_STATE(channel,0.0);
					else if (CHECK_SET_R (channel,i))
						SET_STATE(channel,1.0);
					
					OUTPUT_R(channel,i) = GET_STATE(channel);
				}
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
	long		channel, numChannels;
	real_T				*x; 

	x = ssGetRealDiscStates (S);

	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		x[channel] = GET_STATE(channel);

	for (channel=0; channel < numChannels; channel++)
		x[numChannels + channel] = GET_COUNT(channel);
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

	if (NUM_CHANNELS <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Number of channels must be positive"));
		return;
	}

	if (MIN_LIMIT > MAX_LIMIT)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Minimum output pulse duration must not be larger than maximum output pulse duration"));
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
