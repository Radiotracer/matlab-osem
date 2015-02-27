/**
	@file MeasToModPrj.c 

	@code $Id: MeasToModPrj.c 53 2010-02-12 20:24:32Z mjs $ @endcode

*/

#include <math.h>
#include <mip/irl.h>
#include <mip/miputil.h>
#include <mip/printmsg.h>

#include "protos.h"

void vMeasToModPrj(
			 int nBins,
			 int nRotPixs,
			 int nSlices,
			 float Left,
			 float BinWidth,
			 float PixelWidth,
			 float *RawPrjData,
		    float *ModPrjData
		    )
{
#define raw_ptr(iSlice, iBin) ((iSlice)*nBins+(iBin))
#define mod_ptr(iSlice, iBin) ((iSlice)*nRotPixs+(iBin))
  
  /* pixel width in units of bin width */ 
	float PWid = PixelWidth / BinWidth;
	float PixelLeft, PixelRight;
	int	BinLeft,   BinRight;
	float BinValue;
	int iSlice, iPixel, iBin;
	
  /********************************************************************** 
    
    ( Raw Data:RawPrjData )
    bin0  bin1  bin2  bin3  bin4  bin5  bin6  bin7  bin8  bin9
    |-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
    0     1     2     3     4     5     6     7     8     9     10
    |-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
    CoR
    |          |          |          |          |
    |---PWid---|---PWid---|---PWid---|---PWid---|
    |__________|__________|__________|__________|
    |	  pix1       pix2       pix3       pix4   |
    |															|
    Left		  ( Modified Data:ModPrjData )	 Right
    |                                           |
    |___________________________________________|
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |__________|__________|__________|__________|
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |__________|__________|__________|__________|
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |__________|__________|__________|__________|
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |          |          |          |          |
    |__________|__________|__________|__________|
    ( Rotated Image Grid )
    
    **********************************************************************/
  
	set_float(ModPrjData, nSlices*nRotPixs, 0.0);
	
	PixelLeft = Left;
	vPrintMsg(8,"MeasToModPrj\n");
	vPrintMsg(9,"  left=%.2f, nrotpix=%d, nbins=%d, nslices=%d\n",
		Left, nRotPixs, nBins, nSlices);
	
	for (iPixel=0; iPixel<nRotPixs; iPixel++) {
		PixelRight = PixelLeft + PWid;
		BinLeft = (int) floor((double)PixelLeft);
		BinRight = (int) floor((double)PixelRight);
		
		for (iSlice=0; iSlice<nSlices; iSlice++) {
			if ((BinLeft<0 && BinRight<0) || (BinLeft>=nBins && BinRight>=nBins))
				continue;
			if (BinLeft == BinRight) /* the pixel is within the bin */
				ModPrjData[mod_ptr(iSlice,iPixel)]+=((PixelRight-PixelLeft) * 
							 RawPrjData[raw_ptr(iSlice, BinLeft)]);
			else /* the pixel covers more than one bin */
				for (iBin=BinLeft; iBin<=BinRight; iBin++) {
					if (iBin<0 || iBin>=nBins)
						BinValue = 0.0;
					else
						BinValue = RawPrjData[raw_ptr(iSlice, iBin)];
					
					if (iBin < PixelLeft)
						ModPrjData[mod_ptr(iSlice, iPixel)] += ((iBin+1-PixelLeft) * 
											BinValue);
					else if (iBin+1 > PixelRight)
						ModPrjData[mod_ptr(iSlice, iPixel)] += ((PixelRight-iBin) * 
											BinValue);
					else
						ModPrjData[mod_ptr(iSlice, iPixel)] +=	BinValue;
				}
		}
		PixelLeft = PixelRight;
	}
}
	
#undef raw_ptr
#undef mod_ptr




/*
	Interp_bck 10 2005-05-27 modified duyong$
	
   To interpolate output of irlGenprj with pixel size equals pixelwidth 
   to user specified binwidth.
   Process only one view.
   Since currently only support BinWidth=PixelWidth, what this function does
   is only to normalize the projections with 1/NumViews

*/

void Interp_bck(
						float *InPrjData,      /* input projection data with PixelWidth */
						float *OutPrjData,     /* output projection data with BinWidth */
						IrlParms_t *psParm, 
						PrjView_t psView, 
						float *Sum
               )
{
#define usr_ptr(iSlice, iBin) ((iSlice)*nBins + (iBin))
#define gen_ptr(iSlice, iBin) ((iSlice)*nRotPixs + (iBin))

	/* bin width in units of pixel width */ 
   /* float BWid = psParm->BinWidth / psParm->PixelWidth; 
      require Binwidth = PixelWidth */
   float BWid = 1.0;

	float BinLeft, BinRight;
	int	PixelLeft, PixelRight;
	float PixelValue;
	int	i, iSlice, iBin, iPixel;
	int 	nAngs = psParm->NumViews;
	int 	nRotPixs = psParm->NumPixels;
	int 	nBins = psParm->NumPixels;
	int 	nSlices = psParm->NumSlices;
   float	Fac = BWid/nAngs;  /* or 1/(PWid*nAngs) */


/********************************************************************** 

							  ( Usr Data:UsrPrjData )
	 bin0  bin1  bin2  bin3  bin4  bin5  bin6  bin7  bin8  bin9
	|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
	0     1     2     3     4     5     6     7     8     9     10
	|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|
													 CoR
					    |          |          |          |          |
					    |---PWid---|---PWid---|---PWid---|---PWid---|
					    |__________|__________|__________|__________|
						 |	  pix1       pix2       pix3       pix4   |
						 |															|
						Left		  ( Generated Data:GenPrjData )	 Right
						 |                                           |
						 |___________________________________________|
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |__________|__________|__________|__________|
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |__________|__________|__________|__________|
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |__________|__________|__________|__________|
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |          |          |          |          |
						 |__________|__________|__________|__________|
										 ( Rotated Image Grid )

**********************************************************************/

	set_float(OutPrjData, nSlices*nBins, 0.0);

	(*Sum) = 0.0;

	BinLeft = -psView.Left*BWid;

	for (iBin=0; iBin<nBins; iBin++) {
		BinRight = BinLeft + BWid;
		PixelLeft  = (int) floor((double)BinLeft);
		PixelRight = (int) floor((double)BinRight);

		for (iSlice=0; iSlice<nSlices; iSlice++) {
			if ((PixelLeft<0 && PixelRight<0) || 
				(PixelLeft>=nRotPixs && PixelRight>=nRotPixs)) {
				continue;

			} else if (PixelLeft == PixelRight){ /* the bin is within the pixel */
				OutPrjData[usr_ptr(iSlice, iBin)] += ((BinRight-BinLeft) * 
													InPrjData[gen_ptr(iSlice, PixelLeft)]);

			} else { /* the bin covers more than one pixel */
				for (iPixel=PixelLeft; iPixel<=PixelRight; iPixel++) {
					if (iPixel<0 || iPixel>=nRotPixs)
						PixelValue = 0.0;
					else
						PixelValue = InPrjData[gen_ptr(iSlice, iPixel)];

					if (iPixel < BinLeft)
						OutPrjData[usr_ptr(iSlice, iBin)] += ((iPixel+1-BinLeft) * 
																	PixelValue);
					else if (iPixel+1 > BinRight)
						OutPrjData[usr_ptr(iSlice, iBin)] += ((BinRight-iPixel) * 
																	PixelValue);
					else
						OutPrjData[usr_ptr(iSlice, iBin)] += PixelValue;
				}
			}

			(*Sum) += OutPrjData[usr_ptr(iSlice, iBin)];
		}

		BinLeft = BinRight;
	}

	for (i=0; i<nSlices*nBins; i++)
			OutPrjData[i] *= Fac;
		
   (*Sum) *= Fac;

#undef usr_ptr
#undef gen_ptr
}
