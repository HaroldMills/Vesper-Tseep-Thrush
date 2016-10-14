/*
 * sdistributor.c: Buffered Polyphase Distributor
 *
 * Takes buffered input stream, "x", downsamples by a number
 * of samples (FACTOR), and outputs "y1,...,yFactor".  
 *
 * The input vector has width n.  Factor must be a divisor of n.
 * Output vectors have width n/factor.
 *
 * Simulink state elements carry over the final DELAY inputs.
 * The states are organized as a circular buffer.
 *
 * Parameters are:
 *		Size of buffer
 *      Factor
 *      Phases to output
 *
 * Author:
 *		Steve Mitchell
 *		Sapphire Computers
 *		PO Box 542
 *		Yellow Springs, OH  45387
 *
 * Revision:
 *    1/31/99     SGM      Initial version
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME sdistributor
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
	kFACTOR,				/* Decimation factor			*/
	kOUTPUT_PHASES,			/* Vector of output phases		*/
	kNUM_PARAMETERS
};

#define BUFFER_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define FACTOR				((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kFACTOR))))
#define OUTPUT_PHASE(i)		((long)  floor(0.5+ mxGetPr(ssGetSFcnParam (S, kOUTPUT_PHASES))[i]))
#define NUM_OUTPUT_PHASES	(mxGetN(ssGetSFcnParam(S, kOUTPUT_PHASES)))	

/* Pointer work vector */
enum{
	kYS,				/* Output vector pointers		*/
	kPHASES,			/* Output phases				*/
	kNUMPWORKITEMS
};

#define GET_YS					(ssGetPWorkValue (S, kYS))
#define SET_YS(x)				(ssSetPWorkValue (S, kYS, (x)))
#define GET_PHASES				(ssGetPWorkValue (S, kPHASES))
#define SET_PHASES(x)			(ssSetPWorkValue (S, kPHASES, (x)))

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
	int		i;

	ssSetNumSFcnParams (S, kNUM_PARAMETERS);  /* Number of expected parameters */

#if defined(MATLAB_MEX_FILE)
	if (ssGetNumSFcnParams (S) != ssGetSFcnParamsCount (S)) 
		return;

	mdlCheckParameters (S);		RETURN_IF_ERROR;
#endif

    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, 0);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 1)) return;
	if (!ssSetNumOutputPorts (S, NUM_OUTPUT_PHASES)) return;

    ssSetInputPortWidth(   S, 0, BUFFER_SIZE);   
									/* number of inputs                      */
	for (i=0; i < NUM_OUTPUT_PHASES; i++)
		ssSetOutputPortWidth(  S, i, BUFFER_SIZE / FACTOR);	
									/* number of outputs                     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
									/* direct feedthrough flag               */
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, 0);   
									/* number of real work vector elements   */
    ssSetNumIWork(         S, 0);
									/* number of integer work vector elements*/
    ssSetNumPWork(         S, kNUMPWORKITEMS);   
									/* number of pointer work vector elements*/
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
	real_T				**ppys;
	int_T				i, *pPhases;

	ppys = (real_T**) malloc (NUM_OUTPUT_PHASES * sizeof (real_T*));
	if (ppys == NULL)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Out of memory"));
		return;
	}

	SET_YS (ppys);

	pPhases = (int_T*) malloc (NUM_OUTPUT_PHASES * sizeof (int_T));
	if (pPhases == NULL)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Out of memory"));
		return;
	}

	/* Renumber phases from 0 to factor-1 (instead of 1 to factor) */
	for (i=0; i < NUM_OUTPUT_PHASES; i++)
		pPhases[i] = OUTPUT_PHASE(i)-1;

	SET_PHASES (pPhases);
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

static void mdlOutputs(SimStruct *S, int_T tid)
{
	int_T				i, n, j, factor, numPhases;
	real_T				**ppys; 
	InputRealPtrsType	uPtrs;
	int_T				*pPhases;


	n = BUFFER_SIZE / FACTOR;
	factor = FACTOR;
	numPhases = NUM_OUTPUT_PHASES;
	pPhases = GET_PHASES;
	ppys = GET_YS;

	for (j=0; j < numPhases; j++)
		ppys[j] = ssGetOutputPortSignal (S, j);
	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);


	for (i=0; i < n; i++)
		for (j=0; j < numPhases; j++)
			ppys [j] [i] = *uPtrs [factor * i + pPhases[j]];
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
	free (GET_PHASES);
	free (GET_YS);
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

	for (i=0; i < 2; i++)
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

	if (FACTOR <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Decimation factor must be positive"));
		return;
	}

	if (BUFFER_SIZE % FACTOR != 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Decimation factor must divide input width"));
		return;
	}

	for (i=0; i < NUM_OUTPUT_PHASES; i++)
	{
		if (OUTPUT_PHASE(i) < 1 || OUTPUT_PHASE(i) > FACTOR)
		{
			ssSetErrorStatus (S, ERROR_STRING ("Output phase must be between 1 and Factor"));
			return;
		}
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
