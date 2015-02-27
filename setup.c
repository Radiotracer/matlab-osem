/*
	GenprjSetup.c

	$Id: setup.c 106 2014-07-06 16:07:53Z frey $
*/

#include <stdio.h>
#include <stdlib.h>
#ifndef WIN32
#define __USE_GNU
#include <fenv.h>
#endif
#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>

#include <mip/miputil.h>
#include <mip/printmsg.h>
#include <mip/cmdline.h>
#include <mip/errdefs.h>
#include <mip/getparms.h>
#include <mip/getline.h>
#include <mip/irl.h>
#include <mip/osemhooks.h>

#include "protos.h"
#include "saveitercheck.h"

char *pchGetNormBase(char *pchBase)
{
	char *pchTmpDir;
	char *pchNormBase;
	int bNormInMemory;
	int bFound;

#ifdef WIN32
	pchTmpDir=pchIrlStrdup(pchGetStrParm("tmpdir",&bFound,""));
#else
	pchTmpDir=pchIrlStrdup(pchGetStrParm("tmpdir",&bFound,"/var/tmp"));
#endif
	bNormInMemory = bGetBoolParm("norm_in_memory", &bFound, TRUE);
	if (bNormInMemory)
		return NULL;
	pchNormBase = (char *) pvIrlMalloc((int)strlen(pchTmpDir) + (int)strlen(pchBase)+2, "GetNormBase:pchNormBase");
	sprintf(pchNormBase,"%s/%s",pchTmpDir,pchBase);
	IrlFree(pchTmpDir);
	return pchNormBase;
}

// get Parameters that are needed for osems or genprjs
//	 @param iMode - 0 for osems, 1 for genprjs
void vGetParms(IrlParms_t *psParms, Options_t *psOptions, int iMode)
{
	int bFound, bFoundAll, bDrfFromFile, bUseGrfInBck, iNumSubsets, i;
	int save_interval;
	char *pchIterSaveString;

	vPrintMsg(4,"\nGetParms\n");
	psParms->BinWidth = (float)dGetDblParm("pixwidth",&bFound, 0.0);
	if (iMode == 1)	// genprjs
		psParms->NumViews = iGetIntParm("nang", &bFound, 1);
	else // osems
	{
		psParms->NumIterations = iGetIntParm("iterations", &bFound, 1);
		save_interval = iGetIntParm("save_int",&bFound,1);
		pchIterSaveString = pchGetStrParm("save_iterations",&bFound,""); 
		vInitIterSaveString(save_interval, psParms->NumIterations, pchIterSaveString);

		psParms->NumAngPerSubset = iGetIntParm("num_ang_per_set",&bFound, psParms->NumViews);
		if (psParms->NumViews % psParms->NumAngPerSubset)
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetParms", "No. of angles not an integer multiple of the no. of angles per subset\n NumAngles=%d, NumAngPerSet=%d", psParms->NumViews, psParms->NumAngPerSubset);
		iNumSubsets = psParms->NumViews/psParms->NumAngPerSubset;
		if (iNumSubsets != 1 && iNumSubsets % 2)
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetParms", "Number of subsets (%d) for num_ang_per_set=%d is not even\n", iNumSubsets, psParms->NumAngPerSubset);
		psParms->iNumSrfIterations=iGetIntParm("num_srf_iterations",&bFound, 2);
	}
	i=psParms->SrfCollapseFac=iGetIntParm("srf_collapse_fac", &bFound, 1);
	if (i != 1 && i != 2 && i != 4)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "SrfSetup", "SrfFactor must be 1 (no collapse), 2 or 4", i);

	psParms->fAtnScaleFac = (float) dGetDblParm("AtnMapFac", &bFound, 1.0);
	bDrfFromFile = bGetBoolParm("drf_from_file",&bFound, FALSE);
	if (bDrfFromFile && (iMode == 0))
		bUseGrfInBck = psOptions->bUseGrfInBck = bGetBoolParm("use_grf_in_bck",&bFound,FALSE);
	else
		bUseGrfInBck = psOptions->bUseGrfInBck = FALSE;
	if (psOptions->bModelDrf && (!bDrfFromFile || bUseGrfInBck)){
		vPrintMsg(8,"Generating drf on the fly, getting parameters");
		psParms->fHoleLen = (float) dGetDblParm("collthickness", &bFound, 1.0);
		bFoundAll=bFound;
		psParms->fHoleDiam = (float) dGetDblParm("holediam", &bFound, 1.0);
		bFoundAll &= bFound;
		psParms->fBackToDet = (float) dGetDblParm("gap", &bFound, 1.0);
		bFoundAll &= bFound;
		psParms->fIntrinsicFWHM = (float) dGetDblParm("intrinsicFWHM", &bFound, 1.0);
		bFoundAll &= bFound;
		if (!bFoundAll)
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "vGetParms", "Missing gap, collthickness holediam or intrinsicfwhm with DRF modeling");
	}else{
		psParms->fHoleLen=0.0;
		psParms->fHoleDiam=0.0;
		psParms->fBackToDet=0.0;
		psParms->fIntrinsicFWHM=0.0;
	}
	if (psOptions->bModelDrf)
		psOptions->bFFTConvolve=bGetBoolParm("fft_convolve", &bFound, FALSE);
	else
		psOptions->bFFTConvolve=FALSE;
	
	psOptions->fAtnMapThresh = (float) dGetDblParm("atnmap_support_thresh",&bFound, 0.0);
	psOptions->bUseContourSupport = bGetBoolParm("use_contour_support",&bFound,0);
	psOptions->fAtnMapThresh *= psParms->BinWidth;
	psOptions->iMsgLevel=iGetIntParm("debug_level",&bFound, 4);
	psOptions->iAxialPadLength=iGetIntParm("axial_pad_length",&bFound,0);
	psOptions->iAxialAvgLength=iGetIntParm("axial_avg_length",&bFound,0);
}

int iParseModelString(char *pchModelStr, char *pchName)
/* returns the integer effects string corresponding to pchModelStr.
	pchModel string must contain only characters a, d, or s. Case is
	not significant. If a is found then MODEL_ATN is set. If d is found
	then MODEL_DRF is set. If "s" is found then MODEL_SRF is set. These
	MODEL_STR bits are all or-ed together and returned. The pchName is the
	name of the model being parsed and is used in reporting errors
*/
{
	int iModel=0;

	while ((*pchModelStr) != '\0'){
		switch (tolower(*pchModelStr)){
			case 'a': 
				iModel |= MODEL_ATN;
				break;
			case 'd':
				iModel |= MODEL_DRF;
				break;
			case 's':
				iModel |= MODEL_SRF;
				break;
			default:
				vErrorHandler(ECLASS_WARN, ETYPE_ILLEGAL_VALUE, "ParseModelString", "illegal character (%c) found in %s. Ignoring.",*pchModelStr, pchName);
				break;
		}
		pchModelStr++;
	}
	return iModel;
}

void vGetEffectsToModel(int *pbModelAtn, int *pbModelDrf, int *pbModelSrf)
// Get Effects to model (e.g. atten, drf, srf)
{
	char *pch;
	int bFound;
	int iModel;
	
	vPrintMsg(4,"\nGetEffects\n");
	pch = pchGetStrParm("model",&bFound,"");
	pch = pchGetStrParm("prjmodel",&bFound,pch);
	if (bFound) 
		iModel= iParseModelString(pch, "prjmodel");
	else
		iModel= iParseModelString(pch, "model");
	*pbModelAtn = iModel & MODEL_ATN;
	*pbModelDrf = iModel & MODEL_DRF;
	*pbModelSrf = iModel & MODEL_SRF;
}

PrjView_t *psSetupPrjViews(IrlParms_t *psParms)
/* sets up orbit (defined by radius of rotation and center of rotation in views
	structure. Handles either circular orbit (Single cor and ror) or
	arbitrary orbits (dift cor and ror for each view read from a file) */
{
	int bFound, iLine, bNonCircOrbit, iAng, iNumAngles;
	char *pchLine, *pchOrbitFileName=NULL;
	float fCFCR;
	double dDelAng, dAngleStart, dAngleRange;
	FILE *fpOrbitFile;
	PrjView_t *psViews ;

	vPrintMsg(4,"\nSetupPrjViews\n");

	iNumAngles = psParms->NumViews;
	psViews = (PrjView_t *) pvIrlMalloc(sizeof(PrjView_t)*iNumAngles,"SetupViews:Views");

	dAngleStart = (float)dGetDblParm("angle_start",&bFound, 0);
	dAngleRange = (float)dGetDblParm("angle_range",&bFound, 360);
	dDelAng = dAngleRange / iNumAngles;
	for(iAng = 0; iAng < iNumAngles; ++iAng)
		psViews[iAng].Angle = (float) ((dAngleStart + dDelAng*iAng)*M_PI/180.0);

	bNonCircOrbit = bGetBoolParm("Noncircular_orbit",&bFound,FALSE);

	if (! bNonCircOrbit){
  		// for circular orbit the radius of rotation and center of rotation are the same
		fCFCR = (float) dGetDblParm("Cor2Col", &bFound, 0);
		if (fCFCR < 0.0 )
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE,"psSetupPrjViews", "Cor2Col must be >= 0.0");
		for (iAng=0; iAng<iNumAngles; iAng++)
	   		psViews[iAng].CFCR = fCFCR;
		vPrintMsg(7,"Circular Orbit: ror=%.1f cm\n", fCFCR);
	}else{
		vPrintMsg(7,"NonCirular Orbit:\n");
		vPrintMsg(8,"  Angle      Ror      Cor\n");
		pchOrbitFileName = pchGetStrParm("orbit_file",0,NULL);
		fpOrbitFile = fopen(pchOrbitFileName,"rt");
		if(fpOrbitFile == NULL)
	  		 vErrorHandler(ECLASS_FATAL, ETYPE_IO, "psSetupPrjViews","cannot open file %s for read",pchOrbitFileName);
		for (iAng=0; iAng<iNumAngles; iAng++){
			pchLine=pchGetLine(fpOrbitFile,0,&iLine);
		  	if (sscanf(pchLine, " %f", &fCFCR) != 1)
				vErrorHandler(ECLASS_FATAL, ETYPE_IO, "psSetupPrjViews","Error reading from orbit file %s for angle %d", pchOrbitFileName,iAng);
			psViews[iAng].CFCR = fCFCR;
			vPrintMsg(8,"%7d %8.1f\n",iAng,psViews[iAng].CFCR);
		}
		fclose(fpOrbitFile);
	}
	
	// with the assumptions that the pixel and bin widths are the same,
	//	the number of pixels and number of bins are the same,
	//	and all the transaxial bins were measured, then Left and Right are
	//	easy to calculate
	for (iAng=0; iAng<iNumAngles; iAng++){
		psViews[iAng].Left=0;
		psViews[iAng].Right = (float) psParms->NumPixels-1;
	}
	return psViews;
}

char *pchGetSrfKrnlFname(void)
{
	char *pch;
	int bFound;

	pch=pchGetStrParm("srf_krnl_file",&bFound,"");
	if (!bFound)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetSrfKrnlFname", "Scatter modeling requested but srf_krnl_file not in parameter file");
	return pchIrlStrdup(pch);
}

char *pchGetDrfTabFname(void)
{
	char *pch;
	int bFound;

	pch=pchGetStrParm("drf_tab_file",&bFound,"");
	if (!bFound)
		return NULL;
	return pchIrlStrdup(pch);
}

void SetAdvOpts(void);
void SetAdvOpts(void)
{
	int bFound;
	sAdvOptions sAdvOpts;

	sAdvOpts.bFastRotate=bGetBoolParm("fastrotate",&bFound,TRUE);
	sAdvOpts.iStartIteration=iGetIntParm("start_iter",&bFound,0);
	if (sAdvOpts.iStartIteration < 0){
		vErrorHandler(ECLASS_WARN, ETYPE_ILLEGAL_VALUE, "SetAdvOpts", 
				"Start_iter must be >= 0, setting to 0");
			sAdvOpts.iStartIteration=0;
	}
	OsemSetAdvOptions(&sAdvOpts);
}

/*
void *pvIrlMalloc(int iSize, char *pchName)
{
	void *pvPtr;
	pvPtr=malloc(iSize);
	if (pvPtr == NULL){
		fprintf(stderr,"LocalMalloc: unable to allocate %d bytes for %s",iSize, pchName);
		exit(1);
	}
	return(pvPtr);
}
*/
int iSetupFromCmdLine(int iArgc, char **ppchArgv, IrlParms_t *psIrlParms, Options_t *psOptions, PrjView_t **ppsPrjViews, char **ppchSrfKrnlFile, char **ppchDrfTabFile, char **ppchLogFile, char **ppchMsgFile, char **ppchOutBase, float **ppfPrjImage, float **ppfAtnMap, float **ppfActImage, float **ppfScatterEstimate, float * pfPrimaryFac, int iMode, char *(pUsageMsg(void)))
{
	int		i, bFound, iNextImage, iNumImages,iNumArgs, iMsgLevel=4, bModelAtn=0, bModelSrf=0, bModelDrf=0, iMaxNames;
	char	**ppchImageNames, *pch, *pchParmFileName=NULL,*pchAtnImageName=NULL, *pchActImageName=NULL, *pchInitImageName=NULL;
	float	fTrueBinWidth, *pfAtnMap=NULL,*pfActImage=NULL, *pfPrjImage=NULL, *pfScatterEstimate=NULL;
	
	if (iArgc < 4)
		return (1);

	vSetMsgLevel(iMsgLevel);
	PrintTimes("Start");
	vInitParseCmdLine("", iArgc, ppchArgv, pUsageMsg);
	
	// Get Parms file name from command line. 
	if((pchParmFileName = pchParseCmdLine(&iNumArgs)) == NULL)
		vErrorHandler(ECLASS_FATAL,ETYPE_USAGE,"iSetupFromCmdLine", "Must supply parameter file name\n%s",pUsageMsg());
	
	// reads parameters file and store items in it in database
	vReadParmsFile(pchParmFileName);
	
	iMsgLevel=iGetIntParm("debug_level",&bFound, iMsgLevel);
	vSetMsgLevel(iMsgLevel);

	iMaxNames = iMode ? 3 : 4;	// genprjs max names is 3, osems max names is 4
	if ((ppchImageNames = (char **) pvIrlMalloc(sizeof(char *)*iMaxNames, "iSetupFromCmdLine:ppchImageNames")) == NULL)
		vErrorHandler(ECLASS_FATAL, ETYPE_MALLOC, "iSetupFromCmdLine", "Can not malloc memory for file name array!\n");

	// get the rest of the file names and command line options.
	for(i = 0; i < iMaxNames; ++i)
		if ((ppchImageNames[i] = pchParseCmdLine(&iNumArgs)) == NULL) break;
	
	iNumImages=i;
	if (iNumImages < 2 || (pchParseCmdLine(&iNumArgs) != NULL))
		vErrorHandler(ECLASS_FATAL,ETYPE_USAGE,"iSetupFromCmdLine", "%s\n%s", "incorrect number of images", pUsageMsg());

	// now we need to figure out what effects are modeled so we set up the file names
	vGetEffectsToModel(&bModelAtn, &bModelDrf, &bModelSrf);

	psOptions->bModelDrf=bModelDrf;
	pchActImageName=ppchImageNames[0];
	iNumImages--;
	
	iNextImage=1;
	if (bModelAtn || bModelSrf){
		// with attenuation or scatter compensation we need 3 images
		if (iNumImages > 1){
			pchAtnImageName = ppchImageNames[iNextImage++];
			iNumImages--;
		}else
			vErrorHandler(ECLASS_FATAL,ETYPE_USAGE,"iSetupFromCmdLine","Attenuation map is required for Atn or Srf Compensation");
	}
	if (iMode == 0 && iNumImages > 1) //osems with an intial estimate image
	{
		pchInitImageName=ppchImageNames[iNextImage++];
		iNumImages--;
	}
	if (iNumImages != 1)
		vErrorHandler(ECLASS_FATAL,ETYPE_USAGE,"iSetupFromCmdLine", "%s\n%s","too many images specified",pUsageMsg());
	
	*ppchOutBase = pchIrlStrdup(pchStripExt(ppchImageNames[iNextImage], IMAGE_EXTENSION));
		
	// Get Sizes of projection and reconstructed images and number of bins, 
	//	slices, and angles. The number of bins, angles and slices are stored 
	//	in the Parms structure.
	vGetImageSizes(pchActImageName, psIrlParms, iMode);
	
	vGetParms(psIrlParms, psOptions, iMode);

	// Get Attenuation image. 
	if (bModelAtn || bModelSrf){
		pfAtnMap=pfGetAtnMap(pchAtnImageName, psIrlParms);
		fprintf(stderr,"  sum atn=%.2f\n", sum_float(pfAtnMap,psIrlParms->NumPixels*psIrlParms->NumPixels*psIrlParms->NumSlices));
	}
	
	// get orbit information (cor and ror) and stores in views table
	*ppsPrjViews=psSetupPrjViews(psIrlParms);
	for(i=0; i<psIrlParms->NumViews; ++i)
		vPrintMsg(9,"iangle=%d, angle=%.2f cfcr=%2f\n", i,((*ppsPrjViews)[i]).Angle, ((*ppsPrjViews)[i]).CFCR);
	
	if (iMode == 0) //osems
	{
		psIrlParms->pchNormImageBase=pchGetNormBase(*ppchOutBase);
		pfPrjImage = pfGetPrjImage(pchActImageName, psIrlParms, *ppsPrjViews);
		if (pchInitImageName != NULL && *pchInitImageName != '\0'){
			pfActImage = pfGetInitialEst(pchInitImageName, psIrlParms);
			psOptions->bReconIsInitEst=TRUE;
		}else{
			psOptions->bReconIsInitEst=FALSE;
			pfActImage = (float *) pvIrlMalloc(sizeof(float)*psIrlParms->NumPixels*psIrlParms->NumPixels*psIrlParms->NumSlices,"iSetupFromCmdLine: pfActImage");
		}
	}else { // genprjs
		fTrueBinWidth = (float)dGetDblParm("binwidth",&bFound, psIrlParms->BinWidth);
		if (fTrueBinWidth != psIrlParms->BinWidth)
			vErrorHandler(ECLASS_FATAL,ETYPE_USAGE,"iSetupFromCmdLine", "%s\n","Bin size must equals pixel size!");
		pfPrjImage = (float *) pvIrlMalloc(psIrlParms->NumPixels*psIrlParms->NumSlices*psIrlParms->NumViews*sizeof(float), "iSetupFromCmdLine:pfPrjImage");
		pfActImage = pfGetActImage(pchActImageName, psIrlParms);
		fprintf(stderr,"  sum act=%.2f\n", sum_float(pfActImage,psIrlParms->NumPixels*psIrlParms->NumPixels*psIrlParms->NumSlices));
	}

	*ppchSrfKrnlFile = bModelSrf? pchGetSrfKrnlFname() : NULL;
	*ppchDrfTabFile = bModelDrf ? pchGetDrfTabFname():NULL;
	
	//Scatter Estimate to be added to computed projection data
	pfScatterEstimate=pfGetScatterEstimate(psIrlParms, *ppsPrjViews);
	
	vPrintMsg(4,"\nSetupFromCmdLine\n");
	
	pch=pchGetStrParm("msg_file",&bFound,"");
	*ppchMsgFile= (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);

	pch=pchGetStrParm("log_file",&bFound,"");
	*ppchLogFile= (pch == NULL || *pch == '\0') ? NULL : pchIrlStrdup(pch);
	
	// copy the pointers for the various data so they can be returned
	*ppfPrjImage = pfPrjImage;
	*ppfAtnMap=pfAtnMap;
	*ppfActImage=pfActImage;
	*ppfScatterEstimate=pfScatterEstimate;
	if (iMode == 1)	
		*pfPrimaryFac = (float) dGetDblParm("PrimaryFac",&bFound, 1.0f);

#ifndef WIN32
	if (bGetBoolParm("trap_fpe",&bFound,FALSE)){
		//Enable floating point exception trapping
		feenableexcept(FE_INVALID   | FE_DIVBYZERO | FE_OVERFLOW);
	}
#endif
	SetAdvOpts();

	PrintTimes("\nDone Initialization");
	i=iDoneWithParms();
	IrlFree(ppchImageNames);
	return(0);
}
