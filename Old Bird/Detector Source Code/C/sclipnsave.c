/*
 * sclipnsave.c: Clip and save sample segments
 *
 * Takes two buffered 
 * input streams, "Samples" and "Gate".  Samples may be several 
 * channels; Gate should be just a single channel.  
 * Typically the Samples stream will contain overlapping buffers.  
 * When Gate goes high, samples are collected.  When Gate 
 * goes low, the collected samples are saved to a disk file.
 *
 * A multichannel input stream may be organized in row-major order
 * (interleaved channels) or column-major order (block channels).
 * The Matlab and binary file formats will have the same organization
 * as the input.  WAVE and ASCII files are row-major order.  Mac Binary
 * files are column-major.
 *
 * The file name contains a time stamp.  The time stamp can
 * be either GMT, local time, or relative to the start of the
 * simulation.  The last is calculated using BlockCount and the
 * sample number within the block.
 *
 * The clip file can be either a WAVE file, a Mac-compatible
 * (big-endian) 16-bit binary file, or a Matlab file.  If a
 * Matlab file, the data is saved in variable "sound", and the 
 * sample rate is saved in variable "srate".  Multiple channels
 * in a Mac-compatible or Matlab file are stored in column order 
 * (non-interleaved).
 *
 * The Simulink states are organized as a circular buffer.
 *
 *
 * Parameters are:
 *		Buffer size
 *      FIFO size
 *      Number of Channels (Samples)
 *      Multichannel order (1= column major, 0 = row major)
 *      Base file name (string)
 *      Save directory (string)
 *      File type (1=Wave, 2=Mac Binary, 3=Matlab, 4=ASCII floating point, 5=ASCII fixed point
 *                 6=Binary floating point, 7=Binary fixed point)
 *                 *** Warning *** Saves Level 1.0 Mat file format under RTW
 *      Time Stamp Option (1=GMT, 2=Local, 3=From Start)
 *      Sample Rate
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
 *    3/27/98     SGM      Added file types and time stamp options
 *    4/24/98     SGM      Added column-major multichannel storage
 *                         Converted to Level 2 S-function
 *    9/23/98     SGM      Added AIFF file type
 */


/*
 * You must specify the S_FUNCTION_NAME as the name of your S-function.
 */

#define S_FUNCTION_NAME sclipnsave
#define S_FUNCTION_LEVEL 2

/*
 * Need to include simstruc.h for the definition of the SimStruct and
 * its associated macro definitions.
 */
#include "simstruc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef MATLAB_MEX_FILE
#include "mat.h"
#endif
#include <windows.h>
#include <mmsystem.h>

/* Names of variables in Matlab files */
#define MATLAB_SOUND_VAR "soundData"
#define MATLAB_SRATE_VAR "fs"

/* WAVE file format */
#define BITS_PER_SAMPLE 16


/* File suffixes */
#define WAVE_FILE_SUFFIX		".wav"
#define MAC_FILE_SUFFIX			".mac"
#define MATLAB_FILE_SUFFIX		".mat"
#define ASCII_FLOAT_SUFFIX		".txt"
#define ASCII_FIXED_SUFFIX		".txt"
#define BINARY_FLOAT_SUFFIX		".flt"
#define BINARY_FIXED_SUFFIX		".bin"
#define AIFF_FILE_SUFFIX        ".aif"


/* Simulink block parameters */
enum{
	kBUFFER_SIZE,			/* Samples are buffered								*/
	kFIFO_SIZE,				/* Sample and gate buffers overlap					*/
	kNUM_CHANNELS,			/* Number of channels								*/
	kCOL_MAJOR,				/* Nonzero if multi channels are stored as columns	*/
	kFILENAME,				/* Base file name									*/
	kSAVE_DIR,				/* Save directory									*/
	kFILE_TYPE,				/* File type (1=Wave, 2=Mac, 3=Matlab)				*/
	kTIME_STAMP_OPTION,		/* Time stamp option (1=GMT, 2=Local, 3=From Start) */
	kSAMPLE_RATE,			/* Audio sample rate								*/
	kNUM_PARAMETERS
};

/* File formats */
enum{
	kWAVE_FILE=1,		/* Windows WAVE file					*/
	kMAC_FILE,			/* 16-bit binary big-endian, col major	*/
	kMATLAB_FILE,		/* MAT file	(Level 1.0 under RTW)		*/
	kASCII_FLOAT,		/* ASCII floating point					*/
	kASCII_FIXED,		/* ASCII fixed point					*/
	kBINARY_FLOAT,		/* Binary 64-bit IEEE float				*/
	kBINARY_FIXED,		/* Binary 16-bit signed integer			*/
	kAIFF_FILE,			/* AIFF (Mac, etc) file					*/
	kNUM_FILE_TYPES
};

/* Time stamp options */
enum{
	kGMT=1,				/* Greenwich Mean Time					*/
	kLOCAL_TIME,		/* Local time zone						*/
	kTIME_FROM_START,	/* Time calculated by sample number		*/
	kNUM_TIME_OPTIONS
};

#define BUFFER_SIZE					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kBUFFER_SIZE))))
#define FIFO_SIZE					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kFIFO_SIZE))))
#define NUM_CHANNELS				((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kNUM_CHANNELS))))
#define COL_MAJOR					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kCOL_MAJOR))))
#define GET_FILENAME(buf,buflen)	(mxGetString(ssGetSFcnParam (S, kFILENAME),buf,buflen))
#define GET_SAVE_DIR(buf,buflen)	(mxGetString(ssGetSFcnParam (S, kSAVE_DIR),buf,buflen))
#define FILE_TYPE					((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kFILE_TYPE))))
#define TIME_STAMP_OPTION			((long)  floor(0.5+*mxGetPr(ssGetSFcnParam (S, kTIME_STAMP_OPTION))))
#define SAMPLE_RATE					(                  *mxGetPr(ssGetSFcnParam (S, kSAMPLE_RATE)))

/* Integer work vector */
enum{
	kBUFFER_COUNT,			/* Count of total number of buffers in simulation	*/
	kNUMIWORKITEMS
};

#define GET_BUFFER_COUNT		(ssGetIWorkValue (S, kBUFFER_COUNT))
#define SET_BUFFER_COUNT(x)		(ssSetIWorkValue (S, kBUFFER_COUNT, (x)))


/* Macros */
#define RETURN_IF_ERROR			if (ssGetErrorStatus (S) != NULL) return;
#define ERROR_STRING(a)			a
#define SET_ERROR(err)			{ssSetErrorStatus (S, ERROR_STRING (err));return;}
#define MMERROR					(mmResult != MMSYSERR_NOERROR)

/* Prototypes */
static void mdlCheckParameters (SimStruct *S);
static void SaveClip (SimStruct *S, InputRealPtrsType samplesPtrs, int_T start, int_T end);
static void GetTime (char *timeStamp, int_T timeStampOption, int_T bufferCount, int_T bufferSize, int_T start, real_T fs);
static void SaveASCIIFloat  (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveASCIIFixed  (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveMATFile     (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveMacBinary   (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveAIFFFile    (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveWAVFile     (SimStruct *S,       char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveBinaryFloat (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static void SaveBinaryFixed (SimStruct *S, const char *savePath, int_T numChannels, int_T start, int_T end, InputRealPtrsType samplesPtrs);
static short quantize (real_T x);
static unsigned short byteswap  (unsigned short x);
static unsigned long  byteswap4 (unsigned long  x);
static void doubleToExtended (double x, unsigned short y [5]);
static void MakePCMWaveFormat (SimStruct *S, LPPCMWAVEFORMAT lpwf);

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
    ssSetNumDiscStates(    S, 1);   
									/* number of discrete states             */
	if (!ssSetNumInputPorts  (S, 2)) return;
	if (!ssSetNumOutputPorts (S, 0)) return;

    ssSetInputPortWidth(   S, 0, FIFO_SIZE * NUM_CHANNELS);
									/* number of sample inputs				 */
    ssSetInputPortWidth(   S, 1, FIFO_SIZE);   
									/* number of gate inputs                 */
    ssSetInputPortDirectFeedThrough(S, 0, 1);   
    ssSetInputPortDirectFeedThrough(S, 1, 1);   
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
	SET_BUFFER_COUNT (0);
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



/* Function: mdlUpdate ========================================================
 * Abstract:
 *
 * This function is called once for every major integration time step.
 * Discrete states are typically updated here, but this function is useful
 * for performing any tasks that should only take place once per integration
 * step.
 */

#define MDL_UPDATE
#define CHECK_GATE(i)	(*gatePtrs [i] != 0.0)
#define MAX(x,y)		((x) > (y) ? (x) : (y))

static void mdlUpdate(SimStruct *S, int_T tid)
{
	int_T				overlap, fifosize;
	int_T				risingEdge, trailingEdge;
	real_T				*x; 
	InputRealPtrsType	samplesPtrs, gatePtrs;

	x = ssGetRealDiscStates (S);
	samplesPtrs = ssGetInputPortRealSignalPtrs (S, 0);
	gatePtrs = ssGetInputPortRealSignalPtrs (S, 1);

	fifosize = FIFO_SIZE;
	overlap = fifosize-BUFFER_SIZE;

	risingEdge = overlap;

	while (1)
	{
		/* Find the next rising edge */
		for ( ; risingEdge < fifosize; risingEdge++)
		{
			if (CHECK_GATE(risingEdge))
				break;
		}

		if (risingEdge==fifosize)
			break;		/* Gate is low to end of FIFO */

		if (risingEdge == overlap)
			/* Back up and find first rising edge */
			for ( ; risingEdge > 0; risingEdge--)
			{
				if (!CHECK_GATE(risingEdge-1))
					break;
			}

		/* Find the trailing edge */
		trailingEdge = MAX (risingEdge, overlap);
		for ( ; trailingEdge < fifosize; trailingEdge++)
		{
			if (!CHECK_GATE(trailingEdge))
				break;
		}

		if (trailingEdge == fifosize)
			break;		/* We'll pick this up next time */

		/* Save a clip to disk */
		SaveClip (S, samplesPtrs, risingEdge, trailingEdge);

		/* Find next rising edge */
		risingEdge = trailingEdge;
	}

	/* Increment buffer count */
	SET_BUFFER_COUNT (GET_BUFFER_COUNT+1);
}

/* Function: SaveClip ===================================================
 * Abstract:
 *
 * Save some data to a disk file.
 */

#define STRLEN 512
#define PATH_SEPARATOR "\\"

void SaveClip (SimStruct *S, InputRealPtrsType samplesPtrs, int_T start, int_T end)
{
	long			numChannels, tryNum, fileType;
	char			fname [STRLEN], tryPath [STRLEN], savePath [STRLEN];
	char			timeStamp [STRLEN], tryStr [STRLEN], suffix [STRLEN];
	FILE			*fid;

	numChannels = NUM_CHANNELS;

	/* Generate file name */

	GET_FILENAME    (fname,    STRLEN);
	GET_SAVE_DIR    (savePath, STRLEN);

	switch (fileType=FILE_TYPE)
	{
		case kWAVE_FILE:		/* Windows WAVE file					*/
			strcpy (suffix, WAVE_FILE_SUFFIX);
			break;

		case kMAC_FILE:			/* 16-bit binary big-endian, col major	*/
			strcpy (suffix, MAC_FILE_SUFFIX);
			break;

		case kMATLAB_FILE:		/* MAT file								*/
			strcpy (suffix, MATLAB_FILE_SUFFIX);
			break;

		case kASCII_FLOAT:		/* ASCII floating point					*/
			strcpy (suffix, ASCII_FLOAT_SUFFIX);
			break;

		case kASCII_FIXED:		/* ASCII fixed point					*/
			strcpy (suffix, ASCII_FIXED_SUFFIX);
			break;

		case kBINARY_FLOAT:		/* Binary 64-bit IEEE float				*/
			strcpy (suffix, BINARY_FLOAT_SUFFIX);
			break;

		case kBINARY_FIXED:		/* Binary 16-bit signed integer			*/
			strcpy (suffix, BINARY_FIXED_SUFFIX);
			break;

		case kAIFF_FILE:		/* Mac 16-bit big-endian signed integer	*/
			strcpy (suffix, AIFF_FILE_SUFFIX);
			break;
	}

	GetTime (timeStamp, TIME_STAMP_OPTION, GET_BUFFER_COUNT, BUFFER_SIZE, start, SAMPLE_RATE);

	strcpy (tryPath, savePath);
	strcat (tryPath, PATH_SEPARATOR);
	strcat (tryPath, fname);
	strcat (tryPath, timeStamp);

	/* Add unique sub-second identifier */
	tryNum = 0;
	do
	{
		strcpy (savePath, tryPath);
		sprintf (tryStr, "_%.2d", tryNum);
		strcat (savePath, tryStr);
		strcat (savePath, suffix);

		fid = fopen (savePath, "rb");
		if (fid != NULL)
			fclose (fid);

		tryNum++;
	}
	while (fid != NULL && tryNum < 100);

	switch (fileType)
	{
		case kWAVE_FILE:		/* Windows WAVE file					*/
			SaveWAVFile     (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kMAC_FILE:			/* 16-bit binary big-endian, col major	*/
			SaveMacBinary   (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kMATLAB_FILE:		/* MAT file								*/
			SaveMATFile     (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kASCII_FLOAT:		/* ASCII floating point					*/
			SaveASCIIFloat  (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kASCII_FIXED:		/* ASCII fixed point					*/
			SaveASCIIFixed  (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kBINARY_FLOAT:		/* Binary 64-bit IEEE float				*/
			SaveBinaryFloat (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kBINARY_FIXED:		/* Binary 16-bit signed integer			*/
			SaveBinaryFixed (S, savePath, numChannels, start, end, samplesPtrs);
			break;

		case kAIFF_FILE:		/* Binary 16-bit signed integer			*/
			SaveAIFFFile    (S, savePath, numChannels, start, end, samplesPtrs);
			break;
	}
}



/* Function: SaveASCIIFloat ===================================================
 * Abstract:
 *
 * Write floating point ASCII file.
 */

void SaveASCIIFloat (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			n, channel;
	FILE			*fid;

	/* Open file */

	fid = fopen (savePath, "w");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
				if (fprintf (fid, "%.15e ", *samplesPtrs [fifoSize * channel + n]) < 0)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}

			if (fprintf (fid, "\n") < 0)
			{
				fclose (fid);
				SET_ERROR ("Error writing clip file");
			}
		}
	}

	else /* ROW_MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
				if (fprintf (fid, "%.15e ", *samplesPtrs [channel + numChannels * n]) < 0)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}

			if (fprintf (fid, "\n") < 0)
			{
				fclose (fid);
				SET_ERROR ("Error writing clip file");
			}
		}
	}


	fclose (fid);
}


/* Function: SaveASCIIFixed ===================================================
 * Abstract:
 *
 * Write fixed point ASCII file.
 */

void SaveASCIIFixed (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			n, channel;
	FILE			*fid;

	/* Open file */

	fid = fopen (savePath, "w");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
				if (fprintf (fid, "%6d ", quantize (*samplesPtrs [fifoSize * channel + n])) < 0)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}

			if (fprintf (fid, "\n") < 0)
			{
				fclose (fid);
				SET_ERROR ("Error writing clip file");
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
				if (fprintf (fid, "%6d ", quantize (*samplesPtrs [channel + numChannels * n])) < 0)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}

			if (fprintf (fid, "\n") < 0)
			{
				fclose (fid);
				SET_ERROR ("Error writing clip file");
			}
		}
	}


	fclose (fid);
}


/* Function: SaveMATFile ===================================================
 * Abstract:
 *
 * Write Matlab MAT file using old Level 1.0 Mat format (RTW)
 *                              or Level 2.0 Mat format (Mex file)
 *
 * Mat file is in column-major or row-major order depending on the
 * convention used for the input signal.
 */



void SaveMATFile    (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
#ifdef MATLAB_MEX_FILE
	long			channel, n, length, mRows, nCols;
	MATFile			*fid;
	mxArray			*sound, *srate;
	double			*soundPr;


	/* Create Matlab matrix structures */

	length = end - start;

	if (COL_MAJOR)
	{
		mRows = length;
		nCols = numChannels;
	}

	else /* ROW MAJOR */
	{
		mRows = numChannels;
		nCols = length;
	}

	sound = mxCreateDoubleMatrix (mRows, nCols, mxREAL);
	if (sound == NULL)
		SET_ERROR ("Error allocating temporary Matlab array");

	srate = mxCreateDoubleMatrix (1, 1, mxREAL);
	if (srate == NULL)
	{
		mxDestroyArray (sound);
		SET_ERROR ("Error allocating temporary Matlab array");
	}

	mxSetName (sound, MATLAB_SOUND_VAR);
	mxSetName (srate, MATLAB_SRATE_VAR);

	
	/* Copy samples into Matlab matrix structure */
	
	*mxGetPr (srate) = SAMPLE_RATE;

	soundPr = mxGetPr (sound);

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (channel=0; channel < numChannels; channel++)
			for (n=start; n < end; n++)
				*soundPr++ = *samplesPtrs [fifoSize * channel + n];
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
			for (channel=0; channel < numChannels; channel++)
				*soundPr++ = *samplesPtrs [channel + numChannels * n];
	}


	/* Open file */

	fid = matOpen (savePath, "w");
	if (fid == NULL)
	{
		mxDestroyArray (sound);
		mxDestroyArray (srate);
		SET_ERROR ("Error creating clip file");
	}

	
	/* Write data */

	if (matPutArray (fid, sound))
	{
		mxDestroyArray (sound);
		mxDestroyArray (srate);
		matClose (fid);
		SET_ERROR ("Error writing clip file");
	}

	if (matPutArray (fid, srate))
	{
		mxDestroyArray (sound);
		mxDestroyArray (srate);
		matClose (fid);
		SET_ERROR ("Error writing clip file");
	}


	/* Clean up */

	mxDestroyArray (sound);
	mxDestroyArray (srate);
	matClose (fid);

#else /* MATLAB_MEX_FILE */

/* Under RTW, "mat" routines are not available	*/
/* Use old Level 1.0 Mat file structure			*/
/* See Matlab 4.2c External Interface Guide,	*/
/*    Appendix A for documentation				*/

typedef struct {
	long	type;		/* type */
	long	mrows;		/* row dimension */
	long	ncols;		/* column dimension */
	long	imagf;		/* flag indicating imag part */
	long	namelen;	/* name length (including NULL) */
} Fmatrix;

/* FMatrix "type" field */

/* Type = M * 1000 + P * 10 + T */
#define FMatrixType(M,P,T) (1000*(M)+10*(P)+(T))

/* M = machine type */
enum {
	Little_Endian,	/* Intel, DEC Risc */
	Big_Endian,		/* Mac, SPARC, Apollo, SGI, HP, Motorola */
	VAX_DFloat,
	VAX_GFloat,
	Cray
};

/* P = precision */
enum {
	Float64,		/* 64-bit floating point (double) */
	Float32,		/* 32-bit floating point (single) */
	Int32,			/* 32-bit signed integer */
	Int16,			/* 32-bit signed integer */
	Uint16,			/* 16-bit unsigned integer */
	Uint8			/* 8-bit unsigned integer */
};

/* Note: 
 * 
 * "The precision used by the save command depends on
 * the size and type of each Matrix.  Matrices with any
 * noninteger entries and Matrices with 10,000 or fewer
 * elements are saved in floating point formats requiring
 * 8 bytes per real element.  Matrices with all integer en-
 * tries and more than 10,000 elements are saved in the
 * following formats, requiring fewer bytes per element.
 * 
 * Element range       Bytes per element     [P]
 *
 * [0:255]                  1                 5
 * [0:65535]                2                 4
 * [-32767:32767]           2                 3
 * [-2^31+1:2^31-1]         4                 2
 * other                    8                 0
 *
 * [From Matlab 4.2c External Interface Guide, Appendix A]
 */

/* T = matrix type */
enum {
	Numeric_Matrix,
	Text_Matrix,
	Sparse_Matrix
};


	Fmatrix			fmatrix;
	char			name [256];
	long			nameLen;
	double			srate, sample;
	long			m, n, length, mRows, nCols, channel;
	FILE			*fid;


	/* Open file */

	fid = fopen (savePath, "wb");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	length = end - start;
	if (COL_MAJOR)
	{
		mRows = length;
		nCols = numChannels;
	}

	else /* ROW MAJOR */
	{
		mRows = numChannels;
		nCols = length;
	}

	/* Write sample data header */
	strcpy (name, MATLAB_SOUND_VAR);
	nameLen = strlen (name);

	fmatrix.type = FMatrixType (Little_Endian, Float64, Numeric_Matrix);
	fmatrix.mrows = mRows;
	fmatrix.ncols = nCols;
	fmatrix.imagf = 0;
	fmatrix.namelen = nameLen+1;

	m = fwrite (&fmatrix, sizeof (Fmatrix), 1, fid);
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	m = fwrite (name, sizeof (char), nameLen+1, fid); 
	if (m != nameLen+1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}


	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (channel = 0; channel < numChannels; channel++)
		{
			for (n=start; n < end; n++)
			{
				sample = (double) *samplesPtrs [fifoSize * channel + n];
				m = fwrite (&sample, sizeof (double), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel = 0; channel < numChannels; channel++)
			{
				sample = (double) *samplesPtrs [channel + numChannels * n];
				m = fwrite (&sample, sizeof (double), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}


	/* Write sampling rate */
	strcpy (name, MATLAB_SRATE_VAR);
	nameLen = strlen (name);

	fmatrix.type = FMatrixType (Little_Endian, Float64, Numeric_Matrix);
	fmatrix.mrows = 1;
	fmatrix.ncols = 1;
	fmatrix.imagf = 0;
	fmatrix.namelen = nameLen+1;

	m = fwrite (&fmatrix, sizeof (Fmatrix), 1, fid);
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	m = fwrite (name, sizeof (char), nameLen+1, fid);  
	if (m != nameLen+1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	srate = SAMPLE_RATE;
	m = fwrite (&srate, sizeof (double), 1, fid);
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	fclose (fid);

#endif
}


/* Function: SaveMacBinary ===================================================
 * Abstract:
 *
 * Write 16 bit binary big-endian file.
 * Files are arranged in column-major order.
 */

void SaveMacBinary  (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			channel, n, m;
	FILE			*fid;
	unsigned short	sample;

	/* Open file */

	fid = fopen (savePath, "wb");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (channel=0; channel < numChannels; channel++)
		{
			for (n=start; n < end; n++)
			{
				sample = byteswap ((unsigned short) quantize (*samplesPtrs [fifoSize * channel + n]));
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (channel=0; channel < numChannels; channel++)
		{
			for (n=start; n < end; n++)
			{
				sample = byteswap ((unsigned short) quantize (*samplesPtrs [channel + numChannels * n]));
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}


	fclose (fid);
}


/* Function: SaveWAVFile ===================================================
 * Abstract:
 *
 * Write WAV audio file.
 */

void SaveWAVFile    (SimStruct			*S,
					 char				*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			m, n, channel, length, dataLength;
	HMMIO			fid;
	MMRESULT		mmResult;
	MMCKINFO		mmWAVECkInfo, mmFMTCkInfo, mmDATACkInfo;
	PCMWAVEFORMAT	pcmWaveFormat;
	short			sample;


	/* Check file name */
/*
	if (strlen (savePath) > 127)
		SET_ERROR ("File name too long")

	if (strchr (savePath, '+') != NULL)
		SET_ERROR ("File name must not contain a \"+\" character")
*/

	/* Open file */
	fid = mmioOpen (savePath, NULL, MMIO_WRITE | MMIO_CREATE);
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");


	length = end - start;
	dataLength = length * numChannels * sizeof (short);


	/* Create 'WAVE' chunk */
	mmWAVECkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E'); 
	/* 'WAVE' + 'fmt ' + size + 'data' + size = 20 */
	mmWAVECkInfo.cksize = 20 + sizeof (PCMWAVEFORMAT) + dataLength; 
	mmResult = mmioCreateChunk (fid, &mmWAVECkInfo, MMIO_CREATERIFF);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* Create 'fmt ' chunk */
	mmFMTCkInfo.ckid = mmioFOURCC('f', 'm', 't', ' '); 
	mmFMTCkInfo.cksize = sizeof (PCMWAVEFORMAT); 
	mmResult = mmioCreateChunk (fid, &mmFMTCkInfo, 0);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	MakePCMWaveFormat (S, &pcmWaveFormat);

	/* Write format chunk */
	m = mmioWrite (fid, (char*) &pcmWaveFormat, sizeof (PCMWAVEFORMAT));
	if (m != sizeof (PCMWAVEFORMAT))
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* End format chunk */
	mmResult = mmioAscend (fid, &mmFMTCkInfo, 0);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* Create 'data' chunk */
	mmDATACkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a'); 
	mmDATACkInfo.cksize = dataLength; 
	mmResult = mmioCreateChunk (fid, &mmDATACkInfo, 0);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* Write data chunk */
	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = quantize (*samplesPtrs [fifoSize * channel + n]);
				m = mmioWrite (fid, (char*) &sample, sizeof (short));
				if (m != sizeof (short))
				{
					mmioClose (fid, 0);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = quantize (*samplesPtrs [channel + numChannels * n]);
				m = mmioWrite (fid, (char*) &sample, sizeof (short));
				if (m != sizeof (short))
				{
					mmioClose (fid, 0);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}


	/* End data chunk */
	mmResult = mmioAscend (fid, &mmDATACkInfo, 0);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* End WAVE chunk */
	mmResult = mmioAscend (fid, &mmWAVECkInfo, 0);
	if (MMERROR)
	{
		mmioClose (fid, 0);
		SET_ERROR ("Error writing clip file")
	}

	/* Close file */
	mmResult = mmioClose (fid, 0); 
	if (MMERROR)
		SET_ERROR ("Error writing clip file")
}


/* Function: MakePCMWaveFormat =======================================================
 * Abstract:
 *
 *		Utility for building wave input format struct.
 */
void MakePCMWaveFormat (SimStruct *S, LPPCMWAVEFORMAT lpwf)
{
	lpwf->wf.wFormatTag			= WAVE_FORMAT_PCM;
	lpwf->wf.nChannels			= (short) NUM_CHANNELS;
	lpwf->wf.nSamplesPerSec		= (long) floor (0.5 + SAMPLE_RATE);
	lpwf->wBitsPerSample		= BITS_PER_SAMPLE;
	lpwf->wf.nBlockAlign		= lpwf->wf.nChannels * (lpwf->wBitsPerSample / 8);
	lpwf->wf.nAvgBytesPerSec	= lpwf->wf.nSamplesPerSec * lpwf->wf.nBlockAlign;
}


/* Function: SaveBinaryFloat ===================================================
 * Abstract:
 *
 * Write 32-bit floating point file.
 * Output file is row-major or column-major, depending on input convention
 */

void SaveBinaryFloat (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			m, n, channel;
	real_T			sample;
	FILE			*fid;

	/* Open file */

	fid = fopen (savePath, "wb");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (channel=0; channel < numChannels; channel++)
		{
			for (n=start; n < end; n++)
			{
				sample = *samplesPtrs [fifoSize * channel + n];
				m = fwrite (&sample, sizeof (real_T), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = *samplesPtrs [channel + numChannels * n];
				m = fwrite (&sample, sizeof (real_T), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	
	fclose (fid);
}


/* Function: SaveBinaryFixed ===================================================
 * Abstract:
 *
 * Write 16-bit binary file.
 * Output file is row-major or column-major, depending on input convention
 */

void SaveBinaryFixed (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			channel, n, m;
	FILE			*fid;
	short			sample;

	/* Open file */

	fid = fopen (savePath, "wb");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");

	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (channel=0; channel < numChannels; channel++)
		{
			for (n=start; n < end; n++)
			{
				sample = quantize (*samplesPtrs [fifoSize * channel + n]);
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = quantize (*samplesPtrs [channel + numChannels * n]);
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}


	fclose (fid);
}


/* Function: SaveAIFFFile ===================================================
 * Abstract:
 *
 * Write 16-bit AIFF file.
 */

void SaveAIFFFile (SimStruct			*S,
					 const char			*savePath,
					 int_T				numChannels,
					 int_T				start,
					 int_T				end,
					 InputRealPtrsType	samplesPtrs)
{
	long			channel, n, m;
	FILE			*fid;
	short			sample, shortNum;
	long			numFrames, numSampleBytes, longNum;
	unsigned short	extSampleRate [5];

	if (numChannels > 2)
		SET_ERROR ("AIFF clip file can only save up to two channels.");

	numFrames = end - start;
	numSampleBytes = numFrames * numChannels * 2;

	/* Open file */

	fid = fopen (savePath, "wb");
	if (fid == NULL)
		SET_ERROR ("Error creating clip file");


	/* write FORM chunk */
	longNum = byteswap4 ('FORM');
	m = fwrite (&longNum, 4, 1, fid);						/* 'FORM' */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = byteswap4 (46+numSampleBytes);
	m = fwrite (&longNum, 4, 1, fid);						/* FORM chunk size */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = byteswap4 ('AIFF');
	m = fwrite (&longNum, 4, 1, fid);						/* 'AIFF' */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}


	/* write COMM chunk */
	longNum = byteswap4 ('COMM');
	m = fwrite (&longNum, 4, 1, fid);						/* 'COMM' */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = byteswap4 (18);
	m = fwrite (&longNum, 4, 1, fid);						/* COMM chunk size */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	shortNum = byteswap ((unsigned short) numChannels);
	m = fwrite (&shortNum, 2, 1, fid);						/* number of channels              */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = byteswap4 (numFrames);
	m = fwrite (&longNum, 4, 1, fid);						/* number of samples per channel   */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	shortNum = byteswap (16);
	m = fwrite (&shortNum, 2, 1, fid);						/* sample size                     */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	doubleToExtended (SAMPLE_RATE, extSampleRate);
	m = fwrite (extSampleRate, 10, 1, fid);					/* sample rate */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}


	/* write SSND chunk */
	longNum = byteswap4 ('SSND');
	m = fwrite (&longNum, 4, 1, fid);						/* 'SSND' */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = byteswap4 (8+numSampleBytes);
	m = fwrite (&longNum, 4, 1, fid);						/* SSND chunk size */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = 0;
	m = fwrite (&longNum, 4, 1, fid);						/* offset */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}

	longNum = 0;
	m = fwrite (&longNum, 4, 1, fid);						/* block size */
	if (m != 1)
	{
		fclose (fid);
		SET_ERROR ("Error writing clip file");
	}


	/* Write data */

	if (COL_MAJOR)
	{
		long	fifoSize;

		fifoSize = FIFO_SIZE;

		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = byteswap ((unsigned short) quantize (*samplesPtrs [fifoSize * channel + n]));
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}

	else /* ROW MAJOR */
	{
		for (n=start; n < end; n++)
		{
			for (channel=0; channel < numChannels; channel++)
			{
				sample = byteswap ((unsigned short) quantize (*samplesPtrs [channel + numChannels * n]));
				m = fwrite (&sample, sizeof (short), 1, fid);
				if (m != 1)
				{
					fclose (fid);
					SET_ERROR ("Error writing clip file");
				}
			}
		}
	}


	fclose (fid);
}


/* Function: quantize ===================================================
 * Abstract:
 *
 * Floating point to fixed point conversion.
 */
short quantize (real_T x)
{
	return (short) floor (0.5 + 32767.0 * x);
}


/* Function: byteswap ===================================================
 * Abstract:
 *
 * Convert 16 bit little-endian to big-endian.
 */
unsigned short byteswap (unsigned short x)
{
	return x >> 8 | x << 8;
}


/* Function: byteswap4 ===================================================
 * Abstract:
 *
 * Convert 32 bit little-endian to big-endian.
 */

typedef union {unsigned long a;
               unsigned char b[4];
} Uint32;

unsigned long byteswap4 (unsigned long x)
{
	Uint32			y;
	unsigned char	temp;

	y.a = x;

	temp = y.b [0];
	y.b [0] = y.b [3];
	y.b [3] = temp;

	temp = y.b [1];
	y.b [1] = y.b [2];
	y.b [2] = temp;

	return y.a;
}


/* Function: doubleToExtended ===================================================
 * Abstract:
 *
 * Convert IEEE 754 64-bit little-endian floating point number to IEEE 754 big-endian
 * 80-bit floating point number.
 * This function correctly converts normalized numbers, zero, infinity, and NaN, and
 * aborts on denormalized numbers.
 *
 * (This code borrowed from Harold Mills.)
 */

typedef union {
    double			d;
    unsigned short	s[4];
} Double;

void doubleToExtended (double x, unsigned short y [5]) 
{
    Double z;
    unsigned short exponent, temp;


    z.d = x;

    /* swap little-endian words */
    temp = z.s[0];
    z.s[0] = z.s[3];
    z.s[3] = temp;
    temp = z.s[1];
    z.s[1] = z.s[2];
    z.s[2] = temp;

	/* Now most significant word is first, but bytes within words 
	 * are still in the native little-endian format
	 */

    /* clear extended number */
    y[0] = y[1] = y[2] = y[3] = y[4] = 0;

    /* copy sign bit */
    y[0] |= z.s[0] & 0x8000;

    /* copy mantissa */
    y[1] |= ((z.s[0] & 0xF) << 11) | (z.s[1] >> 5);
    y[2] |= (z.s[1] << 11) | (z.s[2] >> 5);
    y[3] |= (z.s[2] << 11) | (z.s[3] >> 5);
    y[4] |= z.s[3] << 11;

    exponent = (z.s[0] & 0x7FF0) >> 4;

    if (exponent == 2047) {
        /* number is either infinity or NaN */

        exponent = 32767;

    }

    else if (exponent == 0) {
        /* number is either zero or denormalized */

        if ((z.s[0] & 0xF) == 0 && z.s[1] == 0 && z.s[2] == 0 && z.s[3] == 0) {
            /* number is zero */

        }

        else {
            /* number is denormalized */

/*           fprintf(stderr, "error: denormalized input to DoubleToExtended\n");
            abort();
*/
			/* convert exponent */
			exponent -= 1023;
			exponent += 16383;

       }
            
    }

    else {
        /* number is normalized */

        /* convert exponent */
        exponent -= 1023;
        exponent += 16383;

        /* set i bit */
        y[1] |= 0x8000;

    }

    /* set exponent */
    y[0] |= exponent;

	/* Swap bytes within words */
	y[0] = byteswap (y[0]);
	y[1] = byteswap (y[1]);
	y[2] = byteswap (y[2]);
	y[3] = byteswap (y[3]);
	y[4] = byteswap (y[4]);
}



/* Function: GetTime ===================================================
 * Abstract:
 *
 * Create a time stamp string.
 */
void GetTime (char		*timeStamp, 
			  int_T		timeStampOption, 
			  int_T		bufferCount, 
			  int_T		bufferSize, 
			  int_T		start, 
			  real_T	fs)
{

	switch (timeStampOption)
	{
		case kGMT:
		case kLOCAL_TIME:
			{
				time_t		ltime;
				struct tm	*theTime;
				char		timeStr[26];
				char		theMonth[4];

				time (&ltime);

				switch (timeStampOption)
				{
					case kGMT:
						theTime = gmtime (&ltime);
						break;		

					case kLOCAL_TIME:
						theTime = localtime (&ltime);
						break;
				}

				strcpy (timeStr, asctime (theTime));
		
		
				/* Get rid of trailing newline character and internal spaces
				 * and colons (illegal or troublesome filename characters).
				 *
				 * asctime format:
				 * "Wed Jan 02 02:03:55 1980\n"
				 *  0123456789012345678901234
				 *            111111111122222
				 */
			
				timeStr[7]  = '-';
				timeStr[10] = '_';
				timeStr[13] = '.';
				timeStr[16] = '.';
				timeStr[19] = '\0';
				timeStr[24] = '-';
			
			
				strcpy (timeStamp, "_");
			
				/* First the year */
				strcat (timeStamp, timeStr+20);
			
				/* Convert the month to 2 digits */
				sprintf(theMonth,"%.2d", theTime->tm_mon+1);
				strcat (timeStamp, theMonth);
				
				/* Now the day and time */
				strcat (timeStamp, timeStr+7);
			}
			break;
	
		case kTIME_FROM_START:
			{
				double		time;
				long		hours, minutes, seconds;

				time = (bufferCount * bufferSize + start) / fs;

				hours = (long) floor (time / 3600);
				time = time - 3600 * hours;
				minutes = (long) floor (time / 60);
				time = time - 60 * minutes;
				seconds = (long) floor (time);

				sprintf (timeStamp, "_%.3d.%.2d.%.2d", hours, minutes, seconds);
			}
			break;
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
	if (mxGetNumberOfElements (ssGetSFcnParam(S,kBUFFER_SIZE)) != 1)
		SET_ERROR ("Buffer Size must be a scalar");

	if (mxGetNumberOfElements (ssGetSFcnParam(S,kFIFO_SIZE)) != 1)
		SET_ERROR ("FIFO Size must be a scalar");

	if (mxGetNumberOfElements (ssGetSFcnParam(S,kNUM_CHANNELS)) != 1)
		SET_ERROR ("Number of Channels must be a scalar");

	if (mxGetNumberOfElements (ssGetSFcnParam(S,kSAMPLE_RATE)) != 1)
		SET_ERROR ("Sample rate must be a scalar");

	if (!mxIsChar (ssGetSFcnParam(S,kFILENAME)))
		SET_ERROR ("File name prefix must be a string");

	if (!mxIsChar (ssGetSFcnParam(S,kSAVE_DIR)))
		SET_ERROR ("Save Directory must be a string");


	if (BUFFER_SIZE <= 0)
		SET_ERROR ("Buffer Size must be positive");

	if (FIFO_SIZE < BUFFER_SIZE)
		SET_ERROR ("FIFO Size must not be smaller than Buffer Size");

	if (NUM_CHANNELS <= 0)
		SET_ERROR ("Number of channels must be positive");

	if (FILE_TYPE < 1 || FILE_TYPE >= kNUM_FILE_TYPES)
		SET_ERROR ("Unrecognized file type");

	if (TIME_STAMP_OPTION < 1 || TIME_STAMP_OPTION >= kNUM_TIME_OPTIONS)
		SET_ERROR ("Unrecognized time stamp option");
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
