/*
 * stranspose.c: Matrix Transpose
 *
 * Takes A(:), where A is an m x n matrix, and outputs
 * B(:), where B=A'.
 *
 * The input vector has width m * n.
 *
 * Parameters are:
 *		m
 *      n
 *
 * Author:
 *		Steve Mitchell
 *		Dept. of Electrical Engineering
 *		Rhodes Hall
 *		Cornell University
 *		Ithaca, NY  14853
 *
 * Revision:
 *    2/11/98     SGM      Initial version
 *    4/24/98     SGM      Converted to Level 2 S-function
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME stranspose
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>



/* Simulink block parameters */
enum{
	kM,						/* Number of rows				*/
	kN,						/* Number of columns			*/
	kNUM_PARAMETERS
};

#define M					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kM))))
#define N					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kN))))

/* Macros */
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
    ssSetNumDiscStates(    S, 0);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 1)) return;
	if (!ssSetNumOutputPorts (S, 1)) return;

    ssSetInputPortWidth(   S, 0, M * N);   
									/* number of inputs                      */
    ssSetOutputPortWidth(  S, 0, M * N);	
									/* number of outputs                     */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, 0);   
									/* number of real work vector elements   */
    ssSetNumIWork(         S, 0);
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



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */
static void mdlOutputs(SimStruct *S, int_T tid)
{
	int_T				i, j, m, n;
	real_T				*x, *y; 
	InputRealPtrsType	uPtrs;

	x = ssGetRealDiscStates (S);
	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);
	y = ssGetOutputPortSignal (S, 0);

	m = M;
	n = N;

	for (i=0; i < m; i++)
	for (j=0; j < n; j++)
		y[n*i+j]=*uPtrs[m*j+i];
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


	if (M <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Number of rows must be positive"));
		return;
	}

	if (N <= 0)
	{
		ssSetErrorStatus (S, ERROR_STRING ("Number of columns must be positive"));
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
