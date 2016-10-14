/*
 * sgatedshiftregister.c: Buffered gated shift register
 *
 * Takes buffered input stream, "x" and "gate", shifts values
 * from "x" into the shift register when "gate" is non-zero.
 * Outputs shift register output "y".  "Prehistory" 
 * inputs are assumed to be zero.
 *
 * The input vector has width n.
 *
 * Simulink state elements carry the contents of the shift register.
 * The states are organized as a circular buffer.
 *
 * Parameters are:
 *		Size of buffer
 *      Length of shift register
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
 *    9/23/98     SGM      Initial version
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME sgatedshiftregister
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>



/* Simulink block parameters */
enum{
	kBUFFER_SIZE,			/* Input width					*/
	kREG_SIZE,				/* Length of shift register		*/
	kNUM_CHANNELS,			/* Number of channels			*/
	kCOL_MAJOR,				/* Nonzero if multi channels are stored as columns	*/
	kNUM_PARAMETERS
};

#define BUFFER_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define REG_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kREG_SIZE))))
#define NUM_CHANNELS		((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNUM_CHANNELS))))
#define COL_MAJOR			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kCOL_MAJOR))))

/* Integer work vector */
enum{
	kFORMER_INDEX,			/* Position of circular buffer		*/
	kNUMIWORKITEMS
};

#define GET_FORMER_INDEX		(ssGetIWorkValue (S, kFORMER_INDEX))
#define SET_FORMER_INDEX(x)		(ssSetIWorkValue (S, kFORMER_INDEX, (x)))

/* States */
#define RAW_FORMER_INPUT(channel,i)		(x[numChannels*(i)+(channel)])

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
    ssSetNumDiscStates(    S, REG_SIZE * NUM_CHANNELS);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 2)) return;
	if (!ssSetNumOutputPorts (S, 1)) return;

    ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
    ssSetInputPortWidth(   S, 1, BUFFER_SIZE);   
									/* number of inputs                      */
    ssSetOutputPortWidth(  S, 0, BUFFER_SIZE * NUM_CHANNELS);	
									/* number of outputs                     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
									/* direct feedthrough flag               */
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, 0);   
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

	SET_FORMER_INDEX (0);

	/* Prehistory inputs are zero */
	m = REG_SIZE;
	numChannels = NUM_CHANNELS;
	for (i=0; i < m; i++)
	for (channel=0; channel < numChannels; channel++)
		RAW_FORMER_INPUT(channel, i) = 0.0;
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

#define FORMER_INPUT(channel,i)		(RAW_FORMER_INPUT((channel),(GET_FORMER_INDEX + (i)) % reg_size))
#define INPUT_R(channel,i)			(*uPtrs[numChannels*(i)+(channel)])
#define OUTPUT_R(channel,i)			(y     [numChannels*(i)+(channel)])
#define INPUT_C(channel,i)			(*uPtrs[(i)+n*(channel)])
#define OUTPUT_C(channel,i)			(y     [(i)+n*(channel)])
#define GATE(i)						(*gPtrs[i])

static void mdlOutputs(SimStruct *S, int_T tid)
{
	int_T				i, n, reg_size, channel, numChannels;
	real_T				*x, *y; 
	InputRealPtrsType	uPtrs, gPtrs;

	x = ssGetRealDiscStates (S);
	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);
	gPtrs = ssGetInputPortRealSignalPtrs (S, 1);
	y = ssGetOutputPortSignal (S, 0);

	n = BUFFER_SIZE;
	reg_size = REG_SIZE;
	numChannels = NUM_CHANNELS;

	if (COL_MAJOR)
	{
		/* Simulate operation of shift register */
		for (i=0; i < n; i++)
		{
			/* Shift new input into shift register */
			if (GATE (i) != 0.0)
			{
				SET_FORMER_INDEX ((GET_FORMER_INDEX + 1) % reg_size);
	
				for (channel=0; channel < numChannels; channel++)
					FORMER_INPUT(channel,reg_size-1)=INPUT_C(channel,i);
			}

			/* Output oldest value */
			for (channel=0; channel < numChannels; channel++)
				OUTPUT_C(channel,i) = FORMER_INPUT(channel,0);
			
		}
	}

	else /* ROW MAJOR */
	{
		/* Simulate operation of shift register */
		for (i=0; i < n; i++)
		{
			/* Shift new input into shift register */
			if (GATE (i) != 0.0)
			{
				SET_FORMER_INDEX ((GET_FORMER_INDEX + 1) % reg_size);
	
				for (channel=0; channel < numChannels; channel++)
					FORMER_INPUT(channel,reg_size-1)=INPUT_R(channel,i);
			}

			/* Output oldest value */
			for (channel=0; channel < numChannels; channel++)
				OUTPUT_R(channel,i) = FORMER_INPUT(channel,0);
			
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

	if (REG_SIZE < 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Shift register length must be non-negative"));
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
