/*
 * scounter.c: Buffered counter
 *
 * Takes buffered boolean input streams, "enable",
 * "reset", and "preset".  Counts either up or down.
 * Will not count if count value is beyond the stop count, 
 * if active.  "Counting" is actually a floating point 
 * integrator, and can be used this way.
 *
 * Reset and preset take effect on the current sample
 * (direct feedthrough).  Reset overrides preset, if
 * both are present.  If the enable input is not present,
 * the counter is always enabled.
 *
 * The input vector has width n if no reset or preset
 * are present.  If all three are present, the input
 * width is 3n, in the order <enable, reset, preset>.
 *
 * The RWork vector holds the current count.  One state
 * carries over the final/initial count.
 *
 * Parameters are:
 *		Size of buffer
 *      Has enable input
 *		Has reset input
 *		Has preset input
 *		Count down (TRUE) or up (FALSE)
 *		Preset value
 *      Has stopcount
 *      Stopcount value
 *      Initial count
 *      Number of channels
 *      Multichannel order (1= column major, 0 = row major)
 *      Sample Time
 *
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

#define S_FUNCTION_NAME scounter
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
	kHAS_ENABLE,			/* TRUE if enable line present	*/
	kHAS_RESET,				/* TRUE if reset line present	*/
	kHAS_PRESET,			/* TRUE if preset line present	*/
	kCOUNT_DOWN,			/* TRUE if counting downwards	*/
	kPRESET,				/* Preset value					*/
	kHAS_STOP_COUNT,		/* TRUE if stop count active	*/
	kSTOP_COUNT,			/* Stop count value				*/
	kINITIAL_COUNT,			/* Initial counter value		*/
	kNUM_CHANNELS,			/* Number of channels			*/
	kCOL_MAJOR,				/* Nonzero if multi channels are stored as columns	*/
	kSAMPLE_TIME,			/* Simulink sample time			*/
	kNUM_PARAMETERS
};

#define BUFFER_SIZE			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define HAS_ENABLE			((short) (0.0 !=   *mxGetPr(ssGetSFcnParam (S, kHAS_ENABLE))))
#define HAS_RESET			((short) (0.0 !=   *mxGetPr(ssGetSFcnParam (S, kHAS_RESET))))
#define HAS_PRESET			((short) (0.0 !=   *mxGetPr(ssGetSFcnParam (S, kHAS_PRESET))))
#define COUNT_DOWN			((short) (0.0 !=   *mxGetPr(ssGetSFcnParam (S, kCOUNT_DOWN))))
#define PRESET				(                  *mxGetPr(ssGetSFcnParam (S, kPRESET)))
#define HAS_STOP_COUNT		((short) (0.0 !=   *mxGetPr(ssGetSFcnParam (S, kHAS_STOP_COUNT))))
#define STOP_COUNT			(                  *mxGetPr(ssGetSFcnParam (S, kSTOP_COUNT)))
#define INITIAL_COUNT		(                  *mxGetPr(ssGetSFcnParam (S, kINITIAL_COUNT)))
#define NUM_CHANNELS		((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNUM_CHANNELS))))
#define COL_MAJOR			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kCOL_MAJOR))))
#define SAMPLE_TIME			(                  *mxGetPr(ssGetSFcnParam (S, kSAMPLE_TIME)))

/* Real work vector items */
enum{
	kCOUNT,					/* Count from previous iteration	*/
	kNUMRWORKITEMS
};

#define GET_COUNT(channel)		(ssGetRWorkValue (S, kCOUNT+(channel)))
#define SET_COUNT(channel,x)	(ssSetRWorkValue (S, kCOUNT+(channel), (x)))

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
	int_T		numInputPorts;


	ssSetNumSFcnParams (S, kNUM_PARAMETERS);  /* Number of expected parameters */

#if defined(MATLAB_MEX_FILE)
	if (ssGetNumSFcnParams (S) != ssGetSFcnParamsCount (S)) 
		return;

	mdlCheckParameters (S);		RETURN_IF_ERROR;
#endif

	numInputPorts = !!HAS_RESET + !!HAS_PRESET + !!HAS_ENABLE;
	if (!ssSetNumInputPorts  (S, numInputPorts)) return;
	switch (numInputPorts)
	{
		case 0:
			break;

		case 1:
			ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortDirectFeedThrough(S, 0, 1);   
			break;

		case 2:
			ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortWidth(   S, 1, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortDirectFeedThrough(S, 0, 1);   
			ssSetInputPortDirectFeedThrough(S, 1, 1);   
			break;

		case 3:
			ssSetInputPortWidth(   S, 0, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortWidth(   S, 1, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortWidth(   S, 2, BUFFER_SIZE * NUM_CHANNELS);   
			ssSetInputPortDirectFeedThrough(S, 0, 1);   
			ssSetInputPortDirectFeedThrough(S, 1, 1);   
			ssSetInputPortDirectFeedThrough(S, 2, 1);   
			break;
	}

	if (!ssSetNumOutputPorts (S, 1)) return;
	ssSetOutputPortWidth(  S, 0, BUFFER_SIZE * NUM_CHANNELS);	
									/* number of outputs                     */

    ssSetNumContStates(    S, 0);   /* number of continuous states           */
    ssSetNumDiscStates(    S, NUM_CHANNELS);   
									/* number of discrete states             */
    ssSetNumSampleTimes(   S, 1);   /* number of sample times                */
    ssSetNumRWork(         S, NUM_CHANNELS);   
									/* number of real work vector elements   */
    ssSetNumIWork(         S, 0);   /* number of integer work vector elements*/
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
    ssSetSampleTime(S, 0, SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
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
	long		channel, numChannels;
	real_T		*x;

	x = ssGetRealDiscStates (S);

	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		x[channel] = INITIAL_COUNT;
}



/* Function: mdlOutputs =======================================================
 * Abstract:
 *
 * In this function, you compute the outputs of your S-function
 * block. The outputs are placed in the y variable.
 */

#define CHECK_RESET_R(channel,i)		(*resetPtrs  [numChannels*(i) + (channel)] != 0.0)
#define CHECK_PRESET_R(channel,i)		(*presetPtrs [numChannels*(i) + (channel)] != 0.0)
#define OUTPUT_R(channel,i)				( y          [numChannels*(i) + (channel)])
#define ENABLE_R(channel,i)				(*enablePtrs [numChannels*(i) + (channel)])
#define CHECK_RESET_C(channel,i)		(*resetPtrs  [(i) + n*(channel)] != 0.0)
#define CHECK_PRESET_C(channel,i)		(*presetPtrs [(i) + n*(channel)] != 0.0)
#define OUTPUT_C(channel,i)				( y          [(i) + n*(channel)])
#define ENABLE_C(channel,i)				(*enablePtrs [(i) + n*(channel)])

static void mdlOutputs(SimStruct *S, int_T tid)
{
	InputRealPtrsType	enablePtrs, resetPtrs, presetPtrs;
	real_T				stopCount, eps=1e-12;
	int_T				i, n, channel, numChannels;
	real_T				*x, *y; 


	x = ssGetRealDiscStates (S);
	y = ssGetOutputPortSignal (S, 0);

	n = BUFFER_SIZE;
	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		SET_COUNT (channel, x[channel]);	/* State holds previous final count */

	if      (!COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) + 1.0);
			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));
			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if (!COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_R (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_R (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_R (channel, i));

			OUTPUT_R (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) + 1.0);
			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT && !HAS_ENABLE)
	{
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		resetPtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT && !HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - 1.0);

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));
			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET && !COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 - eps) - eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) < stopCount)
				SET_COUNT (channel, GET_COUNT (channel) + ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN && !HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR && !HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET && !HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
		}
	}
	else if ( COL_MAJOR &&  HAS_RESET &&  HAS_PRESET &&  COUNT_DOWN &&  HAS_STOP_COUNT &&  HAS_ENABLE)
	{
		stopCount = STOP_COUNT * (1.0 + eps) + eps;
		enablePtrs = ssGetInputPortRealSignalPtrs (S, 0);
		resetPtrs  = ssGetInputPortRealSignalPtrs (S, 1);
		presetPtrs = ssGetInputPortRealSignalPtrs (S, 2);
		for (i=0; i < n; i++)
		for (channel=0; channel < numChannels; channel++)
		{
			if (CHECK_RESET_C (channel, i))
				SET_COUNT (channel, 0.0);
			else if (CHECK_PRESET_C (channel, i))
				SET_COUNT (channel, PRESET);
			else if (GET_COUNT (channel) > stopCount)
				SET_COUNT (channel, GET_COUNT (channel) - ENABLE_C (channel, i));

			OUTPUT_C (channel, i) = GET_COUNT (channel);
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
	real_T		*x; 

	x = ssGetRealDiscStates (S);

	numChannels = NUM_CHANNELS;

	for (channel=0; channel < numChannels; channel++)
		x[channel] = GET_COUNT (channel);
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
