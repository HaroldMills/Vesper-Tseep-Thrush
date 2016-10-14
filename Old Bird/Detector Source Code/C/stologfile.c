/*
 * stologfile.c: ToLogFile
 *
 * Writes each sample vector as one line to an ascii log file, prepended with
 * the date and time.
 *
 * The input vector has width n.
 *
 * Simulink state elements carry over the final DELAY inputs.
 * The states are organized as a circular buffer.
 *
 * Parameters are:
 *      Width of input vector
 *      Time Stamp Option (1=GMT, 2=Local)
 *      Name of log file
 *
 * Author:
 *		Steve Mitchell
 *		Sapphire Computers
 *		PO Box 542
 *		Yellow Springs, OH  45387
 *
 * Revision:
 *    1/26/99     SGM      Initial version
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME stologfile
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>
#include <stdio.h>
#include <time.h>



/* Simulink block parameters */
enum{
	kINPUT_WIDTH,			/* Number of parameters to log			*/
	kTIME_STAMP_OPTION,		/* Time stamp option (1=GMT, 2=Local)	*/
	kFILENAME,				/* Name of log file						*/
	kNUM_PARAMETERS
};

#define INPUT_WIDTH					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kINPUT_WIDTH))))
#define TIME_STAMP_OPTION			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kTIME_STAMP_OPTION))))
#define GET_FILENAME(buf,buflen)	(mxGetString(ssGetSFcnParam (S, kFILENAME),buf,buflen))

/* Macros */
#define RETURN_IF_ERROR			if (ssGetErrorStatus (S) != NULL) return;
#define ERROR_STRING(a)			a
#define SET_ERROR(err)			{ssSetErrorStatus (S, ERROR_STRING (err));return;}

/* Prototypes */
static void mdlCheckParameters (SimStruct *S);
static void GetTime (char		*timeStamp, 
					int_T		timeStampOption);

/* Paths & strings */
#define STRLEN 512
#define PATH_SEPARATOR "\\"

/* Time stamp options */
enum{
	kGMT=1,				/* Greenwich Mean Time					*/
	kLOCAL_TIME,		/* Local time zone						*/
	kNUM_TIME_OPTIONS
};


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
	if (!ssSetNumOutputPorts (S, 0)) return;

    ssSetInputPortWidth(   S, 0, INPUT_WIDTH);   
									/* number of inputs                      */
    ssSetInputPortDirectFeedThrough(S, 0, 0);   
									/* direct feedthrough flag               */
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
	char			fname [STRLEN];
	char			timeStamp [STRLEN];
	FILE			*fid;
	long			i, numItems;
	InputRealPtrsType	uPtrs;

	uPtrs = ssGetInputPortRealSignalPtrs (S, 0);

	/* Generate file name */

	GET_FILENAME (fname, STRLEN);

	GetTime (timeStamp, TIME_STAMP_OPTION);

	/* Open file */

	fid = fopen (fname, "a");
	if (fid == NULL)
		SET_ERROR ("Error opening log file");

	/* Write data */
	if (fprintf (fid, "%s ", timeStamp) < 0)
	{
		fclose (fid);
		SET_ERROR ("Error writing log file");
	}

	numItems = INPUT_WIDTH;
	for (i=0; i < numItems; i++)
	{
		if (fprintf (fid, "%.15e ", *uPtrs[i]) < 0)
		{
			fclose (fid);
			SET_ERROR ("Error writing log file");
		}
	}

	if (fprintf (fid, "\n") < 0)
	{
		fclose (fid);
		SET_ERROR ("Error writing log file");
	}

	fclose (fid);
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

static void mdlOutputs(SimStruct *S, int_T tid)
{
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
	if (mxGetNumberOfElements (ssGetSFcnParam(S,kINPUT_WIDTH)) != 1)
		SET_ERROR ("Input vector width must be a scalar");
	if (!mxIsChar (ssGetSFcnParam(S,kFILENAME)))
		SET_ERROR ("Log file name must be a string");
}
# endif


/* Function: GetTime ===================================================
 * Abstract:
 *
 * Create a time stamp string.
 * timeStamp must have length 26 or greater.
 */
void GetTime (char		*timeStamp, 
			  int_T		timeStampOption)
{
	time_t		ltime;
	struct tm	*theTime;
	
	time (&ltime);
	
	switch (timeStampOption)
	{
	case kGMT:
		theTime = gmtime (&ltime);
		break;		
		
	case kLOCAL_TIME:
	default:
		theTime = localtime (&ltime);
		break;
	}
	
	strcpy (timeStamp, asctime (theTime));
	
	
	/*
	 * asctime format:
	 * "Wed Jan 02 02:03:55 1980\n"
	 *  0123456789012345678901234
	 *            111111111122222
	 */
	
	timeStamp[24] = '\0';
} 

 


/*=============================*
 * Required S-function trailer *
 *=============================*/

#ifdef	MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
