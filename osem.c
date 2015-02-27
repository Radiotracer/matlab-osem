/*
osem.c

$Id: osem.c 106 2014-07-06 16:07:53Z frey $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>

#include <mip/errdefs.h>
#include <mip/printmsg.h>
#include <mip/miputil.h>
#include <mip/imgio.h>
#include <mip/irl.h>
#ifndef WIN32
#include "buildinfo.h"
#endif
#include "protos.h"
#include "saveitercheck.h"
#include "mex.h"

struct {
	int iNumPixels;
	int iNumSlices;
	char *pchOutBase;
	char *pchOutNameBuf;
} sIterationCallbackData;


void vSetCallBackData(int iNumPixels, int iNumSlices, char *pchOutBase);
void vSetCallBackData(int iNumPixels, int iNumSlices, char *pchOutBase)
{
	fprintf(stderr, "setting up to save output images to %s\n", pchOutBase);
	sIterationCallbackData.iNumPixels = iNumPixels;
	sIterationCallbackData.iNumSlices = iNumSlices;
	sIterationCallbackData.pchOutBase = pchOutBase;
	// for output buffer we need one for the null, plus 1 for the two periods, plus the length of the extension
	sIterationCallbackData.pchOutNameBuf = (char *)pvIrlMalloc((int)strlen(pchOutBase) + 13 + (int)strlen(IMAGE_EXTENSION), "SetCallBackData:OutNameBuf");
}

void vIterationCallback(int iIteration, float *pfCurrentEstimate);
void vIterationCallback(int iIteration, float *pfCurrentEstimate)
{
	/* 	char *pchOutName;
	int iNumPix=sIterationCallbackData.iNumPixels;
	int iNumSlices=sIterationCallbackData.iNumSlices;

	pchOutName=sIterationCallbackData.pchOutNameBuf;
	fprintf(stderr ,"writing output image to %s.%d%s ... ", sIterationCallbackData.pchOutBase, iIteration, IMAGE_EXTENSION);
	sprintf(pchOutName,"%s.%d%s",sIterationCallbackData.pchOutBase, iIteration, IMAGE_EXTENSION);
	if (bCheckIfSaveIteration(iIteration))
	writeimage(pchOutName, iNumPix, iNumPix, iNumSlices, pfCurrentEstimate);
	fprintf(stderr ,"done!\n"); */
}

char *pUsageMsg(void);
char *pUsageMsg(void)
{
	return("usage: osems parmfile [options] prjimage [atnmap] [initest] recon\n");
}

/**
@brief The main function for the osems program
@callgraph
*/
int main(int iArgc, char **ppchArgv)
{
	int i;
	float *pfPrjImage = NULL, *pfAtnMap = NULL, *pfReconImage = NULL, *pfScatterEstimate = NULL;
	char  *pchDrfTabFile = NULL, *pchSrfKrnlFile = NULL, *pchLogFile = NULL, *pchMsgFile = NULL, *pchOutBase = NULL;
	PrjView_t *psViews;
	Options_t sOptions;
	IrlParms_t sIrlParms;
	int iMsgLevel = 4;/* default message level*/

#ifndef WIN32
	extern BuildInfo osems_buildinfo;
	extern BuildInfo libcl_buildinfo;
	extern BuildInfo libirl_buildinfo;
	extern BuildInfo libmiputil_buildinfo;
	extern BuildInfo libimgio_buildinfo;
	extern BuildInfo libim_buildinfo;
	extern BuildInfo libfft_fftw3_buildinfo;

	vSetMsgLevel(iMsgLevel);
	if (CheckForVersionSwitch(iArgc, ppchArgv)) {
		fprintf(stderr, "%s\n", GetBuildSummary(&osems_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libirl_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libfft_fftw3_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libcl_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libimgio_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libim_buildinfo));
		fprintf(stderr, "%s\n", GetBuildSummary(&libmiputil_buildinfo));
		exit(0);
	}
	else
		fprintf(stderr, "%s\n", GetBuildString(&osems_buildinfo));
#endif

	// Print command
	fprintf(stderr, "Information: calling command line:\n");
	for (i = 0; i< iArgc; i++)
		fprintf(stderr, "%s ", ppchArgv[i]);
	fprintf(stderr, "\n\n");

	if (iSetupFromCmdLine(iArgc, ppchArgv, &sIrlParms, &sOptions, &psViews,
		&pchSrfKrnlFile, &pchDrfTabFile, &pchLogFile, &pchMsgFile, &pchOutBase,
		&pfPrjImage, &pfAtnMap, &pfReconImage, &pfScatterEstimate, NULL, 0, pUsageMsg))
	{
		fprintf(stderr, "%s", pUsageMsg());
		exit(1);
	}

	vSetCallBackData(sIrlParms.NumPixels, sIrlParms.NumSlices, pchOutBase);

	//	vPrintMsg(4,"AxialPadLength=%d, AxialAvgLength=%d\n", sOptions.iAxialPadLength, sOptions.iAxialAvgLength);

	PrintTimes("Start IrlOsem");

	i = IrlOsem(&sIrlParms, &sOptions, psViews, pchDrfTabFile, pchSrfKrnlFile, vIterationCallback, pfScatterEstimate, pfAtnMap, pfPrjImage, pfReconImage, pchLogFile, pchMsgFile);
	vSetMsgFilePtr(3, stderr);	// has to reset since it was set to NULL or msg_file in IrlOsem 
	PrintTimes("Done");

	fprintf(stderr, "sum of pfRecn after IrlOsem=%.4g (%d x %d x %d)\n", sum_float(pfReconImage, sIrlParms.NumPixels*sIrlParms.NumPixels*sIrlParms.NumSlices), sIrlParms.NumPixels, sIrlParms.NumPixels, sIrlParms.NumSlices);
	if (i)
		fprintf(stderr, "fatal error in IrlOsem: ErrNum=%d\n      %s", i, pchIrlErrorString());

	vFreeIterSaveString();
	IrlFree(sIrlParms.pchNormImageBase);
	IrlFree(sIterationCallbackData.pchOutNameBuf);
	IrlFree(psViews);
	IrlFree(pchOutBase);
	IrlFree(pfPrjImage);
	IrlFree(pfReconImage);
	if (pfAtnMap) IrlFree(pfAtnMap);
	if (pfScatterEstimate) IrlFree(pfScatterEstimate);
	if (pchSrfKrnlFile) IrlFree(pchSrfKrnlFile);
	if (pchDrfTabFile) IrlFree(pchDrfTabFile);
	if (pchLogFile)	IrlFree(pchLogFile);
	if (pchMsgFile)	IrlFree(pchMsgFile);
	//	vFreeAllMem();
	exit(i);
}


void *pvLocalMalloc(int iSize, char *pchName)
{
	void *pvPtr;
	pvPtr = malloc(iSize);
	if (pvPtr == NULL){
		fprintf(stderr, "LocalMalloc: unable to allocate %d bytes for %s",
			iSize, pchName);
		exit(1);
	}
	return(pvPtr);
}

int exists(const char *fname)
{
	FILE *file;
	if (file = fopen(fname, "r"))
	{
		fclose(file);
		return 1;
	}
	return 0;
}

void vGetmxImageSizesForRecon(mxArray *prjImg, IrlParms_t *psParms)
{
	int iXdim, iYdim, iZdim;
	int bFound;
	int ndim = mxGetNumberOfDimensions(prjImg);
	const mwSize *dims = mxGetDimensions(prjImg);

	if (ndim != 3)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "vGetmxImageSizesForPrj",
		"Image Dimension = %d. The input activity image should be 3D", ndim);

	psParms->NumSlices = dims[0]; // The slice selection in the parameter file is disabled.
	psParms->NumPixels = dims[1];
	psParms->NumViews = dims[2];
}

mxArray* mxScale(const mxArray*input, const double scaler)
{
	unsigned int numel = mxGetNumberOfElements(input);
	double* inptr = (double*)mxGetData(input);
	mxArray* output = mxDuplicateArray(input);
	double* outptr = (double*)mxGetData(output);

	for (int i = 0; i < numel; i++)
		outptr[i] = inptr[i] * scaler;
	return outptr;
}


float* ToFloatArray(const mxArray* input, const double scale)
{
	float* output;
	int i;
	mwSize ndim = mxGetNumberOfDimensions(input);
	mwSize *tdims = mxGetDimensions(input);
	unsigned int numel = mxGetNumberOfElements(input);

	output = pvLocalMalloc(numel*sizeof(float), "ToFloatArray:output");

	mwSize dims[3];
	dims[0] = tdims[0];
	dims[1] = tdims[1];
	if (ndim == 2)
	{
		dims[2] = 1;
	}
	else if (ndim == 3)
	{
		dims[2] = tdims[2];
	}
	else
		mexErrMsgTxt("Input image matrix must be 2d or 3d.");


	switch (mxGetClassID(input))
	{
	case mxDOUBLE_CLASS:
	{
						   double* inptr = (double*)mxGetData(input);
						   i = 0;
						   //Reorder the matrix into row major.
						   for (int s = 0; s < dims[2]; s++)
						   for (int n = 0; n < dims[1]; n++)
						   for (int m = 0; m < dims[0]; m++)
						   {
							   output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							   i++;
						   }
						   break;
	}
	case mxSINGLE_CLASS:
	{
						   float *inptr = (float*)mxGetData(input);
						   i = 0;
						   //Reorder the matrix into row major.
						   for (int s = 0; s < dims[2]; s++)
						   for (int n = 0; n < dims[1]; n++)
						   for (int m = 0; m < dims[0]; m++)
						   {
							   output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							   i++;
						   }
						   break;
	}
	case mxINT8_CLASS:
	{
						 signed char* inptr = (signed char*)mxGetData(input);
						 i = 0;
						 //Reorder the matrix into row major.
						 for (int s = 0; s < dims[2]; s++)
						 for (int n = 0; n < dims[1]; n++)
						 for (int m = 0; m < dims[0]; m++)
						 {
							 output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							 i++;
						 }
						 break;
	}
	case mxUINT8_CLASS:
	{
						  unsigned char* inptr = (unsigned char*)mxGetData(input);
						  i = 0;
						  //Reorder the matrix into row major.
						  for (int s = 0; s < dims[2]; s++)
						  for (int n = 0; n < dims[1]; n++)
						  for (int m = 0; m < dims[0]; m++)
						  {
							  output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							  i++;
						  }
						  break;
	}
	case mxINT16_CLASS:
	{
						  short int* inptr = (short int*)mxGetData(input);
						  i = 0;
						  //Reorder the matrix into row major.
						  for (int s = 0; s < dims[2]; s++)
						  for (int n = 0; n < dims[1]; n++)
						  for (int m = 0; m < dims[0]; m++)
						  {
							  output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							  i++;
						  }
						  break;
	}
	case mxUINT16_CLASS:
	{
						   unsigned short int* inptr = (unsigned short int*)mxGetData(input);
						   i = 0;
						   //Reorder the matrix into row major.
						   for (int s = 0; s < dims[2]; s++)
						   for (int n = 0; n < dims[1]; n++)
						   for (int m = 0; m < dims[0]; m++)
						   {
							   output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							   i++;
						   }
						   break;
	}
	case mxINT32_CLASS:
	{
						  int* inptr = (int*)mxGetData(input);
						  i = 0;
						  //Reorder the matrix into row major.
						  for (int s = 0; s < dims[2]; s++)
						  for (int n = 0; n < dims[1]; n++)
						  for (int m = 0; m < dims[0]; m++)
						  {
							  output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							  i++;
						  }
						  break;
	}
	case mxUINT32_CLASS:
	{
						   unsigned int* inptr = (unsigned int*)mxGetData(input);
						   i = 0;
						   //Reorder the matrix into row major.
						   for (int s = 0; s < dims[2]; s++)
						   for (int n = 0; n < dims[1]; n++)
						   for (int m = 0; m < dims[0]; m++)
						   {
							   output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							   i++;
						   }
						   break;
	}

	case mxINT64_CLASS:
	{
						  int64_T* inptr = (int64_T*)mxGetData(input);
						  i = 0;
						  //Reorder the matrix into row major.
						  for (int s = 0; s < dims[2]; s++)
						  for (int n = 0; n < dims[1]; n++)
						  for (int m = 0; m < dims[0]; m++)
						  {
							  output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							  i++;
						  }
						  break;
	}
	case mxUINT64_CLASS:
	{
						   uint64_T* inptr = (uint64_T*)mxGetData(input);
						   i = 0;
						   //Reorder the matrix into row major.
						   for (int s = 0; s < dims[2]; s++)
						   for (int n = 0; n < dims[1]; n++)
						   for (int m = 0; m < dims[0]; m++)
						   {
							   output[s*dims[0] * dims[1] + m*dims[1] + n] = inptr[i]*scale;
							   i++;
						   }
						   break;
	}

	default:
		mexErrMsgTxt("The class of input array is not numerical.");
		break;
	}

	return output;
}


// The matlab interface function
 void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{ // recon=osems("osem.par",prj,[atnmap],[initest])
	if (nrhs<2 || nrhs>4 || !mxIsChar(prhs[0])){
		mexErrMsgTxt("Invalid input. \nUsage:prjimg=genprj('genprj.par',actimg,[atnmap])");
	}


	IrlParms_t sIrlParms;
	Options_t sOptions;
	PrjView_t *psViews;
	float *pfPrjImage = NULL, *pfAtnMap = NULL, *pfActImage = NULL, *pfScatterEstimate = NULL;
	char *pchDrfTabFile = NULL, *pchSrfKrnlFile = NULL, *pchLogFile = NULL, *pchMsgFile = NULL, *paraFileName = NULL,*pchPrjImageName=NULL;
	float fPrimaryFac = 1;
	int iMsgLevel = 4, bFound, bModelAtn = 0, bModelSrf = 0, bModelDrf = 0;


	long m = mxGetM(prhs[0]); long n = mxGetN(prhs[0]);
	paraFileName = (char*)mxCalloc(n + 1, sizeof(char));
	int status = mxGetString(prhs[0], paraFileName, n + 1);
	if (status != 0)
		mexErrMsgTxt("Failed to get the parameter file name! \n");
	if (!exists(paraFileName))
		mexErrMsgTxt("Parameter file does not exist! \n");

	vReadParmsFile(paraFileName);
	iMsgLevel = iGetIntParm("debug_level", &bFound, iMsgLevel);
	vSetMsgLevel(iMsgLevel);



	vGetEffectsToModel(&bModelAtn, &bModelDrf, &bModelSrf);
	sOptions.bModelDrf = bModelDrf;


	vGetmxImageSizesForRecon(prhs[1], &sIrlParms);
	vGetParms(&sIrlParms, &sOptions, 0);


	if (bModelAtn || bModelSrf){
		if (nrhs < 3)
			mexErrMsgTxt("\n The attenuation map must be provided to model attenuation or scatter. \n");
		//Todo: check the size of attenuation map
		pfAtnMap = ToFloatArray(prhs[2],1.0);
	}

	psViews = psSetupPrjViews(&sIrlParms);
	sIrlParms.pchNormImageBase = NULL;
	pfPrjImage = ToFloatArray(prhs[1],sIrlParms.NumViews);


	if (nrhs == 4)
	{
		pfActImage = ToFloatArray(prhs[3],1.0);
		sOptions.bReconIsInitEst = TRUE;
	}
	else
	{
		sOptions.bReconIsInitEst = FALSE;
		pfActImage = (float *)pvIrlMalloc(sizeof(float)*sIrlParms.NumPixels*sIrlParms.NumPixels*sIrlParms.NumSlices, "mexfunction: pfActImage");
	}

	pchSrfKrnlFile = bModelSrf ? pchGetSrfKrnlFname() : NULL;
	pchDrfTabFile = bModelDrf ? pchGetDrfTabFname() : NULL;


	//Scatter Estimate to be added to computed projection data
	pfScatterEstimate = pfGetScatterEstimate(&sIrlParms, psViews);


	char* pch = pchGetStrParm("msg_file", &bFound, "");
	//pchMsgFile = (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);
	pchMsgFile = NULL;


	pch = pchGetStrParm("log_file", &bFound, "");
	//pchLogFile = (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);
	pchLogFile = NULL;
	
	iDoneWithParms();

	int err_num = IrlOsem(&sIrlParms, &sOptions, psViews,
		pchDrfTabFile, pchSrfKrnlFile,
		vIterationCallback, pfScatterEstimate, pfAtnMap, pfPrjImage, pfActImage, pchLogFile, pchMsgFile);
		//vIterationCallback, pfScatterEstimate, pfAtnMap, pfPrjImage, pfActImage, "log.txt", "msg.txt");
	fprintf(stderr, "sum of pfRecn after IrlOsem=%.4g (%d x %d x %d)\n", sum_float(pfActImage, sIrlParms.NumPixels*sIrlParms.NumPixels*sIrlParms.NumSlices), sIrlParms.NumPixels, sIrlParms.NumPixels, sIrlParms.NumSlices);
	if (err_num) fprintf(stderr, "fatal error in IrlOsem: ErrNum=%d\n      %s", err_num, pchIrlErrorString());

	// Generate the output projection image
	nlhs = 1;
	mwSize actdims[3];
	actdims[0] = sIrlParms.NumPixels;
	actdims[1] = sIrlParms.NumPixels;
	actdims[2] = sIrlParms.NumSlices;
	plhs[0] = mxCreateNumericArray(3, actdims, mxDOUBLE_CLASS, mxREAL);
	double *pOut = mxGetData(plhs[0]);

	int i = 0;
	for (int s = 0; s < actdims[2]; s++)
	for (int m = 0; m < actdims[0]; m++)
	for (int n = 0; n < actdims[1]; n++)
	{
		pOut[s*actdims[0] * actdims[1] + n*actdims[0] + m] = pfActImage[i];
		i++;
	}

	vFreeIterSaveString();
	IrlFree(psViews);
	IrlFree(pfPrjImage);
	IrlFree(pfActImage);
	if (pfAtnMap) IrlFree(pfAtnMap);
	if (pfScatterEstimate) IrlFree(pfScatterEstimate);
	if (pchSrfKrnlFile) IrlFree(pchSrfKrnlFile);
	if (pchDrfTabFile) IrlFree(pchDrfTabFile);
	if (pchLogFile)	IrlFree(pchLogFile);
	if (pchMsgFile)	IrlFree(pchMsgFile);

} 
//

 /* int iSetup(int nrhs, mxArray *prhs[], IrlParms_t *psIrlParms, Options_t *psOptions, PrjView_t **ppsPrjViews, char **ppchSrfKrnlFile, char **ppchDrfTabFile, char **ppchLogFile, char **ppchMsgFile, char **ppchOutBase, float **ppfPrjImage, float **ppfAtnMap, float **ppfActImage, float **ppfScatterEstimate, float * pfPrimaryFac, int iMode, char *(pUsageMsg(void)))
 {
 	if (nrhs>3||nrhs<2){
 		mexErrMsgTxt("Invalid input. \nUsage:prjimg=genprj('osem.par',actimg,'outbase' ");
 	}
 
 	int		i, bFound, iNextImage, iNumImages, iNumArgs, iMsgLevel = 4, bModelAtn = 0, bModelSrf = 0, bModelDrf = 0, iMaxNames;
 	char	**ppchImageNames, *pch, *pchParmFileName = NULL, *pchAtnImageName = NULL, *pchActImageName = NULL, *pchInitImageName = NULL,*pchOutBase=NULL;
 	float	fTrueBinWidth, *pfAtnMap = NULL, *pfActImage = NULL, *pfPrjImage = NULL, *pfScatterEstimate = NULL;
 
 printf("1:\n");
 
 	vSetMsgLevel(iMsgLevel);
 	PrintTimes("Start");
 
 	long m = mxGetM(prhs[0]); long n = mxGetN(prhs[0]);
 	pchParmFileName = (char*)mxCalloc(n + 1, sizeof(char));
 	int status = mxGetString(prhs[0], pchParmFileName, n + 1);
 	if (status != 0)
 		mexErrMsgTxt("Failed to get the parameter file name! \n");
 	if (!exists(pchParmFileName))
 		mexErrMsgTxt("Parameter file does not exist! \n");
 
 printf("2:\n");
 
 	// reads parameters file and store items in it in database
 	vReadParmsFile(pchParmFileName);
 	iMsgLevel = iGetIntParm("debug_level", &bFound, iMsgLevel);
 	vSetMsgLevel(iMsgLevel);
 
 printf("3:\n");
 
 
     // now we need to figure out what effects are modeled so we set up the file names
 	vGetEffectsToModel(&bModelAtn, &bModelDrf, &bModelSrf);
 
 	psOptions->bModelDrf = bModelDrf;
 printf("4:\n");
 
 	m = mxGetM(prhs[1]); n = mxGetN(prhs[1]);
 	pchActImageName = (char*)mxCalloc(n + 1, sizeof(char));
 	status = mxGetString(prhs[1], pchActImageName, n + 1);
 	if (status != 0)
 		mexErrMsgTxt("Failed to get the parameter file name! \n");
 	if (!exists(pchActImageName))
 		mexErrMsgTxt("Parameter file does not exist! \n"); 
 
 printf("5:\n");
 
 
 	m = mxGetM(prhs[2]); n = mxGetN(prhs[2]);
 	pchOutBase = (char*)mxCalloc(n + 1, sizeof(char));
 	status = mxGetString(prhs[2], pchOutBase, n + 1);
 	if (status != 0)
 		mexErrMsgTxt("Failed to get the parameter file name! \n");
 	*ppchOutBase = pchIrlStrdup(pchStripExt(pchOutBase, IMAGE_EXTENSION));
 
 printf("6:\n");
 
 	// Get Sizes of projection and reconstructed images and number of bins, 
 	//	slices, and angles. The number of bins, angles and slices are stored 
 	//	in the Parms structure.
 	vGetImageSizes(pchActImageName, psIrlParms, iMode);
	//vGetmxImageSizesForRecon(prhs[1], psIrlParms);
 
 printf("7:\n");
 
 	vGetParms(psIrlParms, psOptions, iMode);
 printf("8:\n");
 
 
 	// get orbit information (cor and ror) and stores in views table
 	*ppsPrjViews = psSetupPrjViews(psIrlParms);
 	for (i = 0; i<psIrlParms->NumViews; ++i)
 		vPrintMsg(9, "iangle=%d, angle=%.2f cfcr=%2f\n", i, ((*ppsPrjViews)[i]).Angle, ((*ppsPrjViews)[i]).CFCR);
 printf("9:\n");
 
 	if (iMode == 0) //osems
 	{
 		psIrlParms->pchNormImageBase = pchGetNormBase(*ppchOutBase);
 		pfPrjImage = pfGetPrjImage(pchActImageName, psIrlParms, *ppsPrjViews);
		//pfPrjImage = ToFloatArray(prhs[1],psIrlParms->NumViews);  //The pfPrjImage needs to be corrected: multiply by #of views.
 		if (pchInitImageName != NULL && *pchInitImageName != '\0'){
 			pfActImage = pfGetInitialEst(pchInitImageName, psIrlParms);
 			psOptions->bReconIsInitEst = TRUE;
 		}
 		else{
 			psOptions->bReconIsInitEst = FALSE;
 			pfActImage = (float *)pvIrlMalloc(sizeof(float)*psIrlParms->NumPixels*psIrlParms->NumPixels*psIrlParms->NumSlices, "iSetupFromCmdLine: pfActImage");			
 		}
 	}
 	else { // genprjs
 		fTrueBinWidth = (float)dGetDblParm("binwidth", &bFound, psIrlParms->BinWidth);
 		if (fTrueBinWidth != psIrlParms->BinWidth)
 			vErrorHandler(ECLASS_FATAL, ETYPE_USAGE, "iSetupFromCmdLine", "%s\n", "Bin size must equals pixel size!");
 		pfPrjImage = (float *)pvIrlMalloc(psIrlParms->NumPixels*psIrlParms->NumSlices*psIrlParms->NumViews*sizeof(float), "iSetupFromCmdLine:pfPrjImage");
 		pfActImage = pfGetActImage(pchActImageName, psIrlParms);
 		fprintf(stderr, "  sum act=%.2f\n", sum_float(pfActImage, psIrlParms->NumPixels*psIrlParms->NumPixels*psIrlParms->NumSlices));
 	}
 printf("10:\n");
 
 	*ppchSrfKrnlFile = bModelSrf ? pchGetSrfKrnlFname() : NULL;
 	*ppchDrfTabFile = bModelDrf ? pchGetDrfTabFname() : NULL;
 printf("11:\n");
 
 	//Scatter Estimate to be added to computed projection data
 	pfScatterEstimate = pfGetScatterEstimate(psIrlParms, *ppsPrjViews);
 
 	vPrintMsg(4, "\nSetupFromCmdLine\n");
 printf("12:\n");
 
 	pch = pchGetStrParm("msg_file", &bFound, "");
 	//*ppchMsgFile = (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);
 	*ppchMsgFile = NULL;
 printf("13:\n");
 
 	pch = pchGetStrParm("log_file", &bFound, "");
 	//*ppchLogFile = (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);
 	*ppchLogFile =NULL;
 printf("14:\n");
 
 	// copy the pointers for the various data so they can be returned
 	*ppfPrjImage = pfPrjImage;
 	*ppfAtnMap = pfAtnMap;
 	*ppfActImage = pfActImage;
 	*ppfScatterEstimate = pfScatterEstimate;
 printf("15:\n");
 
 	if (iMode == 1)
 		*pfPrimaryFac = (float)dGetDblParm("PrimaryFac", &bFound, 1.0f);
 
 #ifndef WIN32
 	if (bGetBoolParm("trap_fpe", &bFound, FALSE)){
 		//Enable floating point exception trapping
 		feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
 	}
 #endif
 printf("16:\n");
 
 	SetAdvOpts();
 	
 printf("17:\n");
 
 	PrintTimes("\nDone Initialization");
 	i = iDoneWithParms();
 printf("18:\n");
 
 	IrlFree(ppchImageNames);
 printf("19:\n");
 
 	return(0);
 }
 
 void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
 {
 	int i;
 	float *pfPrjImage = NULL, *pfAtnMap = NULL, *pfReconImage = NULL, *pfScatterEstimate = NULL;
 	char  *pchDrfTabFile = NULL, *pchSrfKrnlFile = NULL, *pchLogFile = NULL, *pchMsgFile = NULL, *pchOutBase = NULL;
 	PrjView_t *psViews;
 	Options_t sOptions;
 	IrlParms_t sIrlParms;
 	int iMsgLevel = 4;// default message level
 
 #ifndef WIN32
 	extern BuildInfo osems_buildinfo;
 	extern BuildInfo libcl_buildinfo;
 	extern BuildInfo libirl_buildinfo;
 	extern BuildInfo libmiputil_buildinfo;
 	extern BuildInfo libimgio_buildinfo;
 	extern BuildInfo libim_buildinfo;
 	extern BuildInfo libfft_fftw3_buildinfo;
 
 	vSetMsgLevel(iMsgLevel);
 	if (CheckForVersionSwitch(iArgc, ppchArgv)) {
 		fprintf(stderr, "%s\n", GetBuildSummary(&osems_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libirl_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libfft_fftw3_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libcl_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libimgio_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libim_buildinfo));
 		fprintf(stderr, "%s\n", GetBuildSummary(&libmiputil_buildinfo));
 		exit(0);
 	}
 	else
 		fprintf(stderr, "%s\n", GetBuildString(&osems_buildinfo));
 #endif
 
 printf("hello\n");
 
 	if (iSetup(nrhs, prhs, &sIrlParms, &sOptions, &psViews,
 		&pchSrfKrnlFile, &pchDrfTabFile, &pchLogFile, &pchMsgFile, &pchOutBase,
 		&pfPrjImage, &pfAtnMap, &pfReconImage, &pfScatterEstimate, NULL, 0, pUsageMsg))
 	{
 		fprintf(stderr, "%s", pUsageMsg());
 		exit(1);
 	}
 printf("hello1\n");
 	vSetCallBackData(sIrlParms.NumPixels, sIrlParms.NumSlices, pchOutBase);
  printf("hello2\n");
 	//	vPrintMsg(4,"AxialPadLength=%d, AxialAvgLength=%d\n", sOptions.iAxialPadLength, sOptions.iAxialAvgLength);
 
 	PrintTimes("Start IrlOsem");
 
     i = IrlOsem(&sIrlParms, &sOptions, psViews, pchDrfTabFile, pchSrfKrnlFile, vIterationCallback, pfScatterEstimate, pfAtnMap, pfPrjImage, pfReconImage, pchLogFile, pchMsgFile);
 	vSetMsgFilePtr(3, stderr);	// has to reset since it was set to NULL or msg_file in IrlOsem 
 	PrintTimes("Done");
  printf("hello3\n");
 	fprintf(stderr, "sum of pfRecn after IrlOsem=%.4g (%d x %d x %d)\n", sum_float(pfReconImage, sIrlParms.NumPixels*sIrlParms.NumPixels*sIrlParms.NumSlices), sIrlParms.NumPixels, sIrlParms.NumPixels, sIrlParms.NumSlices);
 	if (i)
 		fprintf(stderr, "fatal error in IrlOsem: ErrNum=%d\n      %s", i, pchIrlErrorString());
 		
 		
 	// Generate the output projection image
 	nlhs = 1;
 	mwSize actdims[3];
 	actdims[0] = sIrlParms.NumPixels;
 	actdims[1] = sIrlParms.NumPixels;
 	actdims[2] = sIrlParms.NumSlices;
 	plhs[0] = mxCreateNumericArray(3, actdims, mxDOUBLE_CLASS, mxREAL);
 	double *pOut = mxGetData(plhs[0]);
 
     i = 0;
 	for (int s = 0; s < actdims[2]; s++)	
 	for (int m = 0; m < actdims[0]; m++)
 	for (int n = 0; n < actdims[1]; n++)
 	{
 		pOut[s*actdims[0]*actdims[1]+n*actdims[0]+m] = pfReconImage[i];
 		i++;
 	}
 
 	vFreeIterSaveString();
 	IrlFree(sIrlParms.pchNormImageBase);
 	IrlFree(sIterationCallbackData.pchOutNameBuf);
 	IrlFree(psViews);
 	IrlFree(pchOutBase);
 	IrlFree(pfPrjImage);
 	IrlFree(pfReconImage);
 	if (pfAtnMap) IrlFree(pfAtnMap);
 	if (pfScatterEstimate) IrlFree(pfScatterEstimate);
 	if (pchSrfKrnlFile) IrlFree(pchSrfKrnlFile);
 	if (pchDrfTabFile) IrlFree(pchDrfTabFile);
 	if (pchLogFile)	IrlFree(pchLogFile);
 	if (pchMsgFile)	IrlFree(pchMsgFile);
 
 }
 */

