/**
	@file GetImages.c

	@code $Id: GetImages.c 7 2005-03-23 19:17:08Z mjs duyong$ @endcode

	@brief Several functions to read in the image data required for
	reconstruction.

	@note Define DEBUG while compiling to print out some extra information
	and save images of the modified projections and atten map in reordered
	format (including scaling). See reorder comment below.
	
	If you want these functions to reorder the image pixels to the IRL
	internal format then define REORDER_PIXELS while compiling.
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <mip/irl.h>
#include <mip/miputil.h>
#include <mip/imgio.h>
#include <mip/errdefs.h>
#include <mip/getparms.h>
#include <mip/printmsg.h>

#include "protos.h"

/**
	 @brief Gets sizes and number of angles for projection data and
	 reconstructed/actitity image.

	 These are obtained from the parameter file and the activity image.
	 
	 @param *pchActImageName - ptr to the activity image data filename.
	 @param *psParms - ptr to the IrlParms_t struct.
	 @param iMode - 0 for osems, 1 for genprjs
	 
	 @return void
*/
void vGetImageSizes(char *pchActImageName, IrlParms_t *psParms, int iMode)
{
	int iXdim, iYdim, iZdim, iSliceStart, iSliceEnd, bFound;
	int iDefaultEndSlice;
	IMAGE *pImage;

	vPrintMsg(4,"\nGetImageSizes\n");
	pImage = imgio_openimage(pchActImageName, 'o', &iXdim, &iYdim, &iZdim);
	vPrintMsg(6,"input image=(%d x %d x %d)\n",iXdim, iYdim, iZdim);

	iSliceStart  = iGetIntParm("slice_start", &bFound, 0);
	if (iMode == 0){
		/* for osem the default end slice is iYdim, the y dimension of the
		   input projection data.
		*/
		iDefaultEndSlice=iYdim-1;
	}else{
		/* for genprj the default end slice is iZdim, the z dimension of the
		   input activity image
		*/
		iDefaultEndSlice=iZdim-1;
	}
	vPrintMsg(6,"Default End Slice=%d\n",iDefaultEndSlice);
	iSliceEnd = iGetIntParm("slice_end", &bFound, iDefaultEndSlice); 
	psParms->NumPixels = iXdim;
	psParms->NumSlices = iSliceEnd - iSliceStart + 1;
	
	if (psParms->NumSlices <= 0)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "vGetImageSizes", "Number of slices must be > 0: start=%d, end=%d, num=%d\n", iSliceStart, iSliceEnd, psParms->NumSlices);
	
	if (iMode == 0){ //osems
		psParms->NumViews = iZdim;
		vPrintMsg(7,"NumPix=%d, NumAngles=%d \n", psParms->NumPixels, psParms->NumViews);
	}else{ // genprjs
		if (iXdim != iYdim)
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "vGetImageSizes", "X and Y dimension of activity image should be equal : x-dimension = %d, Y-dimension = %d", iXdim, iYdim);
		vPrintMsg(7,"NumPix=%d, NumSlices=%d \n", psParms->NumPixels, psParms->NumSlices);
	}
	imgio_closeimage(pImage);
}

/**
	 @brief Reads the projection data from the image stored in pImage and puts
	 it into the modified output projection image.
	 
	 Only the requested slices (y values) are put in the image. In the x
	 direction the data are shifted so the COR is in the center of the
	 array. The input image has dimension iNumBins x iNumInSlices x
	 iNumAngles and the output image has dimensions iOutNumBins x
	 iOutNumSlices x iNumAngles.  It is assumed that iStartSlice and
	 iEndSlice have already been checked against the size of the input image.

	 @param *pImage       - image file containing  projection pixels.
	 @param iInNumBins    - number of projection bins (xdim) in input image. 
	 @param iOutNumBins   - number of bins in output modified projections.
	 @param iInNumSlices  - number of slices (ydim) in input image.
	 @param iNumAngles    - number of angles (zdim) in nput and output image.
	 @param iStartSlice   - starting slice (y value) to put at top of output img.
	 @param iOutNumSlices - number of slices (y values) in output image.
	 @param fInBinWidth   - bin width in input prj image file.
	 @param fOutBinWidth  - bin width in the ouput "modified" projections.
	 @param fScaleFac     - factor to scale projections with (ignored if 0 or 1).
	 @param *psViews      - structure containing information about cor for each.
	                        view that is needed to shift the raw projections into
	                        the output matrix.

	 @return ptr to projection data.
*/
static float *pfReadPrjPix(IMAGE *pImage, int iInNumBins, int iOutNumBins, int iInNumSlices, int iNumAngles, int iStartSlice, int iOutNumSlices, float fInBinWidth, float fOutBinWidth, float fScaleFac, PrjView_t *psViews)
{
	float *pfPrjPixels; // entire set of projection data needed to reconstruct desired slices
	float *pfReadPix; // raw projection data for one 2d view
	int iAngle;
	float fPrjSum=0.0;

	vPrintMsg(6,"\nReadPrjPix\n");
	vPrintMsg(7,"            InBins=%d, OutBins=%d, InSlices=%d, OutSlices=%d\n", iInNumBins, iOutNumBins, iInNumSlices, iOutNumSlices);
	vPrintMsg(7,"            NumAngles=%d, StartSlice=%d, normfac=%.3g\n", iNumAngles, iStartSlice,fScaleFac);
	
	pfPrjPixels=(float *) pvIrlMalloc(sizeof(float)*iOutNumSlices*iOutNumBins*iNumAngles, "ReadPrjImage:PrjPixels");
	set_float(pfPrjPixels, iOutNumSlices*iOutNumBins*iNumAngles, 0.0);
	pfReadPix=(float *) pvIrlMalloc(sizeof(float)*iInNumBins*iInNumSlices,"ReadPrjImage:ReadPix");	// BUG, changed from iInNumBins*iInNumSlices*iNumAngles
	
	for(iAngle=0; iAngle < iNumAngles; ++iAngle){
		// read all pixels in one projection view
		imgio_readslices(pImage,iAngle,iAngle,pfReadPix);
		fPrjSum += sum_float(pfReadPix+iStartSlice*iInNumBins, iInNumBins*iOutNumSlices);
		
		// shift only the slices we need into the right place in PrjPixels
		vMeasToModPrj(iInNumBins, iOutNumBins, iOutNumSlices, psViews[iAngle].Left,fInBinWidth,fOutBinWidth, pfReadPix+iStartSlice*iInNumBins, pfPrjPixels+iAngle*iOutNumSlices*iOutNumBins);
	}
	if (fScaleFac != 0.0 && fScaleFac != 1.0)
		scale_float(pfPrjPixels, iOutNumSlices*iOutNumBins*iNumAngles, fScaleFac);
	vPrintMsg(8,"prjsum=%.2f, modprjsum=%.2f\n",fPrjSum, sum_float(pfPrjPixels,iOutNumSlices*iOutNumBins*iNumAngles)); 
	IrlFree(pfReadPix);
	return pfPrjPixels;
}

/**
	 @brief Opens prj images and calls ReadPrjImages to get projection data
	 and copy and scale it into the projection image.

	 @return a pointer to the projection data.
*/
float *pfGetPrjImage(char *pchPrjImageName, IrlParms_t *psParms, PrjView_t *psViews)
{
	int iXdim, iYdim, iZdim, bFound, iStartSlice;
	IMAGE *pPrjImage; 
	float *pfPrjPixels; // entire set of projection data needed to reconstruct desired slices

	vPrintMsg(4,"\nGetPrjImage\n");
	vPrintMsg(6,"reading projection data from %s\n",pchPrjImageName);
	pPrjImage = imgio_openimage(pchPrjImageName, 'o', &iXdim, &iYdim, &iZdim);

	// Do not need to check image size again since those were all checked in vGetImageSizes
	iStartSlice = iGetIntParm("slice_start", &bFound, 0);
	vPrintMsg(6,"numbins=%d, numangles=%d, start=%d, end=%d, img=%d x %d x %d\n", psParms->NumPixels, psParms->NumViews, iStartSlice, iStartSlice+psParms->NumViews, iXdim, iYdim, iZdim);
	
	// scale by number of angles so reconstructed image is in units of total acquisition time, not time per view
	pfPrjPixels = pfReadPrjPix(pPrjImage, psParms->NumPixels, psParms->NumPixels, iYdim, psParms->NumViews, iStartSlice, psParms->NumSlices, psParms->BinWidth, psParms->BinWidth, (float)psParms->NumViews, psViews);

	imgio_closeimage(pPrjImage);
	return(pfPrjPixels);
}

/**
	 @brief Reads from image and gets the projection slices needed for
	        reconstruction.

	 @note The output image is NOT in "reordered" (y varying slowest) format
	       unless REORDER_PIXELS is defined during compilation.
	 
	 @param *pImage     pointer to input image.
	 @param iStartSlice start slice in input image. 
	 @param iNumSlices  number of slices in output image.
	 @param iNumPixels  number of pixels (xdim and ydim) in input and 
	                    output image.
	 
	 @return a pointer to these pixels.
*/
static float *pfReadIrlImage(IMAGE *pImage, int iStartSlice, int iNumSlices, int iNumPixels)
{
	int		iLen = iNumSlices*iNumPixels*iNumPixels;
	float	*pfPixels;
	pfPixels = (float *) pvIrlMalloc(sizeof(float)*iLen, "ReadIrlImage:Pixels");
	
#ifndef REORDER_PIXELS
	imgio_readslices(pImage, iStartSlice, iStartSlice+iNumSlices-1, pfPixels);
#else
	float *pfReadSlice;
	int iX, iY;
	pfReadSlice = (float *) pvIrlMalloc(sizeof(float)*iNumPixels*iNumPixels,"ReadIrlImage:ReadSlice");
	for(iSlice=0; iSlice < iNumSlices; ++iSlice){
		imgio_readslices(pImage, iStartSlice+iSlice, iStartSlice+iSlice, pfReadSlice);
		for(iY=0; iY<iNumPixels; ++iY)
			for(iX=0; iX<iNumPixels; ++iX)
				//store pixels in reordered format
				pfPixels[iX + iNumPixels*(iSlice + iY*iNumSlices)] = pfReadSlice[iX + iY*iNumPixels];
	}
	IrlFree(pfReadSlice);
#endif
	return(pfPixels);
}

/**
	 @brief Reads the attenunation map.

	 @note The output image is NOT in "reordered" (y varying slowest) format
	       unless REORDER_PIXELS is defined during compilation.

	 @param *pchAtnMapName - Attenuation data file name.
	 @param *psParms       - Parameters struct.

	 @return ptr to attenuation map.
*/
float *pfGetAtnMap(char *pchAtnMapName, IrlParms_t *psParms)
{
	int iStartSlice, iXdim, iYdim, iZdim, bFound;
	float *pfAtnPixels;
	IMAGE *pAtnImage;

	vPrintMsg(4,"\nGetAtnMap\n");
	vPrintMsg(6,"  Atn Map=%s\n",pchAtnMapName);
	iStartSlice=iGetIntParm("atn_slice_start",&bFound,0);
	if (iStartSlice < 0)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetAtnMap", "Start Slice for Atn Map must be >= 0, not %d",iStartSlice);

	pAtnImage=imgio_openimage(pchAtnMapName,'o',&iXdim,&iYdim,&iZdim);
	if (iYdim != iXdim || iXdim != psParms->NumPixels)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetAtnMap", "Atn Map must be same size as reconstructed/activity image");
	
	if (iStartSlice + psParms->NumSlices - 1 >= iZdim)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetAtnMap", "Not enough slices in atn map. First slice=%d, last slice needed: %d", iStartSlice, iStartSlice + psParms->NumSlices - 1);
	
	pfAtnPixels = pfReadIrlImage(pAtnImage,iStartSlice, psParms->NumSlices, psParms->NumPixels);

#ifdef DEBUG
	writeimage("atnimage.im", psParms->NumPixels, psParms->NumSlices, psParms->NumPixels, pfAtnPixels);
#endif
	imgio_closeimage(pAtnImage);
	return(pfAtnPixels);
}

/**
	 @brief Determines whether there is an additive scatter estimate and if
	 so, reads it in and scale it.
	 
	 @param *psParms
	 @param *psViews

	 @return ptr to scaled scatter estimate.
*/
float *pfGetScatterEstimate(IrlParms_t *psParms, PrjView_t *psViews)
{
	int		iStartSlice, iXdim, iYdim, iZdim, bFound;
	char	*pchScatterEstimateFile;
	float	*pfScatterEstimate=NULL;
	IMAGE	*pScatImage;

	vPrintMsg(4,"\nGetScatterEstimate\n");

	pchScatterEstimateFile=pchGetStrParm("scat_est_file",&bFound, "");
	if (!bFound){
		vPrintMsg(6,"  No scatter estimate file will be used\n");
		psParms->fScatEstFac = 1;	// Suppose no need, but just in case
		return NULL;
	}
	psParms->fScatEstFac = (float) dGetDblParm("scat_est_fac",&bFound,1.0);
	iStartSlice=iGetIntParm("scat_startslice",&bFound, iGetIntParm("slice_start", &bFound, 0));
	
	pScatImage = imgio_openimage(pchScatterEstimateFile, 'o', &iXdim, &iYdim, &iZdim);
	if (iXdim != psParms->NumPixels || iZdim != psParms->NumViews)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetScatterEstimate", "Number of bins (%i) or number of angles (%i) in scat est file not correct (%i/%i)\n     Scat est file: %s\n", iXdim, iZdim, psParms->NumPixels, psParms->NumViews, pchScatterEstimateFile);
	if (iStartSlice < 0 || iStartSlice > iStartSlice + psParms->NumSlices - 1 || iStartSlice + psParms->NumSlices - 1 >= iYdim)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetScatterEstimate", "Start & end values for scatter slice are illegal or inconsistent\n");
	
	// get the projection pixels, treat them just as we do the projection data in terms of shifting, Scale them by psParms->NumViews (will scale by fScatEstFac in libirl)
	pfScatterEstimate = pfReadPrjPix(pScatImage, psParms->NumPixels, psParms->NumPixels, iYdim, psParms->NumViews, iStartSlice, psParms->NumSlices, psParms->BinWidth, psParms->BinWidth, (float)psParms->NumViews, psViews);
	imgio_closeimage(pScatImage);
	return (pfScatterEstimate);
}

/**
	 @brief Reads the initial estimate.
	 
	 @note The output image is NOT in "reordered" (y varying slowest) format
	       unless REORDER_PIXELS is defined during compilation.

	 @param *pchInitImageName
	 @param *psParms

	 @return ptr to initial estimate.
*/
float *pfGetInitialEst(char *pchInitImageName, IrlParms_t *psParms)
{
	int		iStartSlice, iXdim, iYdim, iZdim, i, bFound;
	float	*pfPixels, fAvg;
	IMAGE	*pInitImage;

	vPrintMsg(4,"GetInitialEstimate\n");
	iStartSlice=iGetIntParm("initest_slice_start",&bFound,0);
	if (iStartSlice < 0)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetInitialEst", "Start Slice for Atn Map must be >= 0, not %d",iStartSlice);

	pInitImage=imgio_openimage(pchInitImageName,'o',&iXdim,&iYdim,&iZdim);
	if (iYdim != iXdim || iXdim != psParms->NumPixels)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetInitialEst", "Initial Estimate must be same size as reconstructed image");
	
	if (iStartSlice + psParms->NumSlices - 1 >= iZdim)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetInitialEst", "Not enough slices in initial est. First slice=%d, last slice needed: %d", iStartSlice, iStartSlice + psParms->NumSlices - 1);
	pfPixels = pfReadIrlImage(pInitImage,iStartSlice, psParms->NumSlices, psParms->NumPixels);
	imgio_closeimage(pInitImage);
	
	// compute mean in initial estimate
	fAvg = sum_float(pfPixels, psParms->NumSlices*psParms->NumPixels*psParms->NumPixels);
	fAvg /= (float)psParms->NumSlices*psParms->NumPixels*psParms->NumPixels;
	if (fAvg <= 0.0) fAvg = 1.0;
	
	// check for negatives. If negative, replace by Average value
	for (i=0;i<psParms->NumSlices * iXdim * iXdim; ++i)
		if (pfPixels[i] < 0.0) pfPixels[i] = fAvg;
	
	if (bGetBoolParm("save_initial_estimate", &bFound, FALSE)){
		float *pfWriteImage;
		pfWriteImage = (float *) pvIrlMalloc(sizeof(float)*psParms->NumSlices*iXdim*iXdim, "GetInitialEstimate:WriteImage");
#ifdef REORDER_PIXELS
		reorder(iXdim, psParms->NumSlices, iXdim, pfPixels, pfWriteImage);	
#endif
#ifdef IMGIO_NOIM
		writeimage("initest.img",iXdim, iXdim, psParms->NumSlices,pfWriteImage);
#else
		writeimage("initest.im",iXdim, iXdim, psParms->NumSlices,pfWriteImage);
#endif
		IrlFree(pfWriteImage);
	}
	return(pfPixels);
}

/**
	 @brief Reads the activity image and extracts the requisite slices.

	 @note The output image is NOT in "reordered" (y varying slowest) format
	       unless REORDER_PIXELS is defined during compilation.

	 @param pchActImageName
	 @param psParms

	 @return ptr to the activity image.
*/
float *pfGetActImage(char *pchActImageName, IrlParms_t *psParms)
{
	int		iStartSlice, iXdim, iYdim, iZdim, bFound;
	float	*pfPixels;
	IMAGE	*pActImage;

	vPrintMsg(4,"\nGetActImage\n");
	vPrintMsg(6,"  Act Image=%s\n",pchActImageName);
	iStartSlice=iGetIntParm("slice_start",&bFound,0);
	if (iStartSlice < 0)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetActImage", "Start Slice for Act must be >= 0, not %d",iStartSlice);

	pActImage=imgio_openimage(pchActImageName,'o',&iXdim,&iYdim,&iZdim);
	if (iYdim != iXdim || iXdim != psParms->NumPixels)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetActImage", "Activity image must be square (should have been cauth earlier)");
	
	if (iStartSlice + psParms->NumSlices - 1 >= iZdim)
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetActImage", "Not enough slices in activity image. First slice=%d, last slice needed: %d", iStartSlice, iStartSlice + psParms->NumSlices - 1);
	pfPixels = pfReadIrlImage(pActImage,iStartSlice, psParms->NumSlices, psParms->NumPixels);
	imgio_closeimage(pActImage);
	return(pfPixels);
}

/**
	 @brief Reads in the image that is used to compute orbit.

	 @warning The Implementation appears to be INCOMPLETE.

	 @return ptr to the image.
*/

float *pfGetOrbitImage(char *pchFname, IrlParms_t *psParms)
{
/*	IMAGE *pInImage;
	float *pfPixels;
	float *pfSlice;
	int iXdim, iYdim, iZdim;
	int iImageSize;
	int iSlice, iStartSlice, iEndSlice;
	int i;
	int bFound;

	pInImage=imgio_openimage(pchFname,'o',&iXdim,&iYdim,&iZdim);

	if (iYdim != iXdim || iXdim != psParms->NumPixels){
		vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "ComputeOrbitFromFile",
			"Image used to compute orbit must be square ");
	}
	iImageSize=psParms->NumPixels * psParms->NumPixels, 
	pfPixels = (float *) pvIrlMalloc(sizeof(float)*iImageSize, "GetOrbitImage: Pixels");
	if (iZdim == 1){
		//Image is already 2d
		imgio_readslices(pInImage,0,0,pfPixels);
	}else{
		iStartSlice=iGetIntParm("slice_start",&bFound,0);
		iEndSlice = iStartSlice + psParms->NumSlices - 1;
		if (iEndSlice >= iZdim)
			vErrorHandler(ECLASS_FATAL, ETYPE_ILLEGAL_VALUE, "GetActImage",
		"Not enough slices in orbit image. First slice=%d, last slice needed: %d",
			iStartSlice, iEndSlice);
		pfSlice = (float *) pvIrlMalloc(sizeof(float)*iImageSize, "GetOrbitImage: Slice");
		set_float(pfPixels,iImageSize,0.0);
		for(iSlice=iStartSlice; i<= iEndSlice; ++i){
			imgio_readslices(pInImage,iSlice,iSlice,pfSlice);
			for(i=0; i<iImageSize; ++i){
				pfPixels[i] += pfSlice[i];
			}
		}
	}
	imgio_closeimage(pInImage);
*/
	return NULL;
}
