/* $Id: cmslnr.c,v 1.7 2006/04/14 16:34:12 hvengel Exp $ */
/*  Little cms - profiler construction set */
/*  Copyright (C) 1998-2001 Marti Maria */
/* */
/* THIS SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, */
/* EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY */
/* WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. */
/* */
/* IN NO EVENT SHALL MARTI MARIA BE LIABLE FOR ANY SPECIAL, INCIDENTAL, */
/* INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, */
/* OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, */
/* WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF */
/* LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE */
/* OF THIS SOFTWARE. */
/* */
/* This file is free software; you can redistribute it and/or modify it */
/* under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or */
/* (at your option) any later version. */
/* */
/* This program is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* General Public License for more details. */
/* */
/* You should have received a copy of the GNU General Public License */
/* along with this program; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */
/* */
/* As a special exception to the GNU General Public License, if you */
/* distribute this file as part of a program that contains a */
/* configuration script generated by Autoconf, you may include it under */
/* the same distribution terms that you use for the rest of that program. */
/* */
/* Version 1.09a */


#include "lcmsprf.h"


LPGAMMATABLE cdecl cmsxEstimateGamma(LPSAMPLEDCURVE X, LPSAMPLEDCURVE Y, int nResultingPoints);
void		 cdecl cmsxCompleteLabOfPatches(LPMEASUREMENT m, SETOFPATCHES Valids, int Medium);

void cdecl cmsxComputeLinearizationTables(LPMEASUREMENT m, 
                                    int ColorSpace, 
                                    LPGAMMATABLE Lin[3],
									int nResultingPoints,
									int Medium);
                                          
                                          
void cdecl cmsxApplyLinearizationTable(double In[3], 
                                       LPGAMMATABLE Gamma[3], 
                                       double Out[3]);

void cdecl cmsxApplyLinearizationGamma(WORD In[3], LPGAMMATABLE Gamma[3], WORD Out[3]);



/* ------------------------------------------------------------- Implementation */


#define EPSILON	0.00005
#define LEVENBERG_MARQUARDT_ITERATE_MAX  150

/* In order to track linearization tables, we use following procedure */
/* */
/* We first assume R', G' and B' does exhibit a non-linear behaviour */
/* that can be separated for each channel as Yr(R'), Yg(G'), Yb(B') */
/* This is the shaper step */
/* */
/*  R = Lr(R') */
/*  G = Lg(G') */
/*  B = Lb(B')   (0.0) */
/* */
/* After this step, RGB is converted to XYZ by a matrix multiplication */
/* */
/*  |X|       |R| */
//  |Y| = [M]�|G|
/*  |Z|       |B|    (1.0) */
/* */
/* In order to extract Lr,Lg,Lb tables, we are interested only on Y part  */
/*  */
/*  Y = (m1 * R + m2 * G + m3 * B)  (1.1) */
/* */
/* The total intensity for maximum RGB = (1, 1, 1) should be 1, */
/* */
/*  1 = m1 * 1 + m2 * 1 + m3 * 1, so */
/* */
/*  m1 + m2 + m3 = 1.0		(1.2) */
/* */
/* We now impose that for neutral (gray) patches, RGB components must be equal */
/* */
/*	R = G = B = Gray  */
/*  */
/* So, substituting in (1.1): */
/*  */
/*  Y = (m1 + m2 + m3) Gray */
/*  */
/* and for (1.2), (m1+m2+m3) = 1, so */
/* */
/*  Y = Gray = Lr(R') = Lg(G') = Lb(B') */
/* */
/* That is, after prelinearization, RGB of gray patches should give  */
/* same values for R, G and B. And this value is Y. */
/* */
/* */


static
LPSAMPLEDCURVE NormalizeTo(LPSAMPLEDCURVE X, double N, BOOL lAddEndPoint)
{
		int i, nItems;
		LPSAMPLEDCURVE XNorm;
		
		nItems = X ->nItems;
		if (lAddEndPoint) nItems++;

		XNorm = cmsAllocSampledCurve(nItems);

		for (i=0; i < X ->nItems; i++) {

			XNorm ->Values[i] = X ->Values[i] / N;  			
		}
		
		if (lAddEndPoint)
			XNorm -> Values[X ->nItems] = 1.0;

		return XNorm;	
}


/* */
/* ------------------------------------------------------------------------------ */
/* */
/* Our Monitor model. We assume gamma has a general expression of */
/* */
/*  Fn(x) = (Gain * x + offset) ^ gamma | for x >= 0 */
/*	Fn(x) = 0							| for x < 0 */
/*	 */
/*	First partial derivatives are */
/* */
/*  dFn/dGamma  = Fn * ln(Base) */
/*  dFn/dGain   = gamma * x * ((Gain * x + Offset) ^ (gamma -1)) */
/*	dFn/dOffset = gamma * ((Gain * x + Offset) ^ (gamma -1)) */
/* */

static 
void GammaGainOffsetFn(double x, double *a, double *y, double *dyda, int na)
{
    double Gamma,Gain,Offset;
	double Base;

    Gamma  = a[0];
    Gain   = a[1];
    Offset = a[2];

    Base = Gain * x + Offset; 	

	if (Base < 0) {

		Base = 0.0;
		*y = 0.0;    
		dyda[0] = 0.0;
		dyda[1] = 0.0;
		dyda[2] = 0.0;


	} else {
		
	
		/* The function itself */
		*y = pow(Base, Gamma);

		/* dyda[0] is partial derivative across Gamma */
		dyda[0] = *y * log(Base);
	    	    
		/* dyda[1] is partial derivative across gain */
		dyda[1] = (x * Gamma) * pow(Base, Gamma-1.0);

		/* dyda[2] is partial derivative across offset */
		dyda[2] =     Gamma * pow(Base, Gamma-1.0);
	}
} 


/* Fit curve to our gamma-gain-offset model. */

static
BOOL OneTry(LPSAMPLEDCURVE XNorm, LPSAMPLEDCURVE YNorm, double a[])
{
	LCMSHANDLE h;
	double ChiSq, OldChiSq;		
	int i;
	BOOL Status = TRUE;

		/* initial guesses */

	    a[0] = 3.0;			/* gamma */
	    a[1] = 4.0;			/* gain */
	    a[2] = 6.0;			/* offset */
		a[3] = 0.0;			/* Thereshold */
		a[4] = 0.0;			/* Black */

							
		/* Significance = 0.02 gives good results */

		h = cmsxLevenbergMarquardtInit(XNorm, YNorm,  0.02, a, 3, GammaGainOffsetFn);							
		if (h == NULL) return FALSE;


		OldChiSq = cmsxLevenbergMarquardtChiSq(h);

		for(i = 0; i < LEVENBERG_MARQUARDT_ITERATE_MAX; i++) {

			if (!cmsxLevenbergMarquardtIterate(h)) {
				Status = FALSE;
				break;
			}

			ChiSq = cmsxLevenbergMarquardtChiSq(h);
		
			if(OldChiSq != ChiSq && (OldChiSq - ChiSq) < EPSILON)
				break;

			OldChiSq = ChiSq;
		}
		
		cmsxLevenbergMarquardtFree(h);

		return Status;
}

/* Tries to fit gamma as per IEC 61966-2.1 using Levenberg-Marquardt method */
/*  */
/* Y = (aX + b)^Gamma | X >= d */
/* Y = cX             | X < d */

LPGAMMATABLE cmsxEstimateGamma(LPSAMPLEDCURVE X, LPSAMPLEDCURVE Y, int nResultingPoints)
{
	double a[5];
	LPSAMPLEDCURVE XNorm, YNorm;	
	double e, Max;


		/* Coarse approximation, to find maximum.  */
	    /* We have only a portion of curve. It is likely */
	    /* maximum will not fall on exactly 100. */

		if (!OneTry(X, Y, a)) 
			return FALSE;

		/* Got parameters. Compute maximum. */
		e = a[1]* 255.0 + a[2];
		if (e < 0) return FALSE;
		Max = pow(e, a[0]);
		

		/* Normalize values to maximum */
		XNorm = NormalizeTo(X, 255.0, FALSE);
		YNorm = NormalizeTo(Y, Max, FALSE);

		/* Do the final fitting  */
		if (!OneTry(XNorm, YNorm, a))
				return FALSE;
				
		/* Type 3 = IEC 61966-2.1 (sRGB) */
        /* Y = (aX + b)^Gamma | X >= d */
        /* Y = cX             | X < d */
		return  cmsBuildParametricGamma(nResultingPoints, 3, a);
}





/* A dumb bubble sort */

static
void Bubble(LPSAMPLEDCURVE C, LPSAMPLEDCURVE L)
{
#define SWAP(a, b)      { tmp = (a); (a) = (b); (b) = tmp; }

        BOOL lSwapped;
        int i, nItems;
        double tmp;

		nItems = C -> nItems;
        do {
                lSwapped = FALSE;

                for (i= 0; i <  nItems - 1; i++) {

                        if (C->Values[i] > C->Values[i+1]) {

                                SWAP(C->Values[i], C->Values[i+1]);
								SWAP(L->Values[i], L->Values[i+1]);                            
                                lSwapped = TRUE;
                        }
                }

        } while (lSwapped);

#undef SWAP
}



/* Check for monotonicity. Force it if is not the case. */

static
void CheckForMonotonicSampledCurve(LPSAMPLEDCURVE t)
{
    int n = t ->nItems;
    int i;
	double last;

    last = t ->Values[n-1];
    for (i = n-2; i >= 0; --i) {
        
        if (t ->Values[i] > last)

                t ->Values[i] = last;
        else
                last = t ->Values[i];

    }
    
}

/* The main gamma inferer. Tries first by gamma-gain-offset,  */
/* if not proper reverts to curve guessing. */

static
LPGAMMATABLE BuildGammaTable(LPSAMPLEDCURVE C, LPSAMPLEDCURVE L, int nResultingPoints)
{
	LPSAMPLEDCURVE Cw, Lw, Cn, Ln;
	LPSAMPLEDCURVE out;
	LPGAMMATABLE Result;
	double Lmax, Lend, Cmax;

	/* Try to see if it can be fitted 	 */
	Result = cmsxEstimateGamma(C, L, nResultingPoints);  
	if (Result)
		return Result;
	

	/* No... build curve from scratch. Since we have not */
	/* endpoints, a coarse linear extrapolation should be */
	/* applied in order to get the expected maximum. */

	Cw = cmsDupSampledCurve(C);
	Lw = cmsDupSampledCurve(L);
	
	Bubble(Cw, Lw);

    /* Get endpoint */
	Lmax = Lw->Values[Lw ->nItems - 1];
	Cmax = Cw->Values[Cw ->nItems - 1];

	/* Linearly extrapolate */
	Lend = (255 * Lmax) / Cmax;

	Ln = NormalizeTo(Lw, Lend, TRUE);
	Cn = NormalizeTo(Cw, 255.0, TRUE);

	cmsFreeSampledCurve(Cw);
	cmsFreeSampledCurve(Lw);

	/* Add endpoint */
	out = cmsJoinSampledCurves(Cn, Ln,  nResultingPoints);
	
	cmsFreeSampledCurve(Cn);
	cmsFreeSampledCurve(Ln);
	
	CheckForMonotonicSampledCurve(out);

	cmsSmoothSampledCurve(out, nResultingPoints*4.);
	cmsClampSampledCurve(out, 0, 1.0);			
	
	Result = cmsConvertSampledCurveToGamma(out, 1.0);    

	cmsFreeSampledCurve(out);
	return Result;
}




void cmsxCompleteLabOfPatches(LPMEASUREMENT m, SETOFPATCHES Valids, int Medium)
{
    LPPATCH White;
    cmsCIEXYZ WhiteXYZ;

    int i;

    if (Medium == MEDIUM_REFLECTIVE_D50)
    {
	WhiteXYZ.X = D50X * 100.;
	WhiteXYZ.Y = D50Y * 100.;
	WhiteXYZ.Z = D50Z * 100.;
    }
    else
    {
        White = cmsxPCollFindWhite(m, Valids, NULL);
	if (!White) return;

	WhiteXYZ = White ->XYZ;
    }


    /* For all patches with XYZ and without Lab, add Lab values. */
    /* Transmissive profiles does need to locate its own white */
    /* point for device gray. Reflective does use D50 */

    for (i=0; i < m -> nPatches; i++)
    {
        if (Valids[i])
        {
            LPPATCH p = m -> Patches + i;
            if ((p ->dwFlags & PATCH_HAS_XYZ) &&
	        (!(p ->dwFlags & PATCH_HAS_Lab) || (Medium == MEDIUM_TRANSMISSIVE)))
            {
                cmsXYZ2Lab(&WhiteXYZ, &p->Lab, &p->XYZ);
                p -> dwFlags |= PATCH_HAS_Lab;
            }
        }
    }
}


/* Compute linearization tables, trying to fit in a pure  */
/* exponential gamma. If gamma cannot be accurately infered, */
/* then does build a smooth, monotonic curve that does the job. */

void cmsxComputeLinearizationTables(LPMEASUREMENT m, 
                                    int ColorSpace, 
                                    LPGAMMATABLE Lin[3],
									int nResultingPoints,
									int Medium)                                    
                                    
{
    LPSAMPLEDCURVE R, G, B, L;
	LPGAMMATABLE gr, gg, gb;
    SETOFPATCHES Neutrals;
    int nGrays;
    int i;
	      
	/* We need Lab for grays. */
	cmsxCompleteLabOfPatches(m, m->Allowed, Medium);

    /* Add neutrals, normalize to max */
    Neutrals = cmsxPCollBuildSet(m, FALSE);
    cmsxPCollPatchesNearNeutral(m, m ->Allowed, 15, Neutrals);	

    nGrays = cmsxPCollCountSet(m, Neutrals);
    
    R = cmsAllocSampledCurve(nGrays);   
    G = cmsAllocSampledCurve(nGrays);
    B = cmsAllocSampledCurve(nGrays);
    L = cmsAllocSampledCurve(nGrays);
            
	nGrays = 0;
	    
    /* Collect patches    */
    for (i=0; i < m -> nPatches; i++) {

                if (Neutrals[i]) {

                        LPPATCH gr = m -> Patches + i;

						
                        R -> Values[nGrays] = gr -> Colorant.RGB[0];
                        G -> Values[nGrays] = gr -> Colorant.RGB[1];
                        B -> Values[nGrays] = gr -> Colorant.RGB[2];
						L -> Values[nGrays] = gr -> XYZ.Y; 
                        
                        nGrays++;
                }

    }


	gr = BuildGammaTable(R, L, nResultingPoints);
	gg = BuildGammaTable(G, L, nResultingPoints);
	gb = BuildGammaTable(B, L, nResultingPoints);

	cmsFreeSampledCurve(R);
	cmsFreeSampledCurve(G);
	cmsFreeSampledCurve(B);
	cmsFreeSampledCurve(L);

	if (ColorSpace == PT_Lab) {
		LPGAMMATABLE Gamma30 = cmsBuildGamma(nResultingPoints, 3.0);

		Lin[0] = cmsJoinGammaEx(gr, Gamma30, nResultingPoints);
		Lin[1] = cmsJoinGammaEx(gg, Gamma30, nResultingPoints);
		Lin[2] = cmsJoinGammaEx(gb, Gamma30, nResultingPoints);

		cmsFreeGamma(gr); cmsFreeGamma(gg); cmsFreeGamma(gb);
		cmsFreeGamma(Gamma30);
	}
	else {
		LPGAMMATABLE Gamma1 = cmsBuildGamma(nResultingPoints, 1.0);

		Lin[0] = cmsJoinGammaEx(gr, Gamma1, nResultingPoints);
		Lin[1] = cmsJoinGammaEx(gg, Gamma1, nResultingPoints);
		Lin[2] = cmsJoinGammaEx(gb, Gamma1, nResultingPoints);

		cmsFreeGamma(gr); cmsFreeGamma(gg); cmsFreeGamma(gb);
		cmsFreeGamma(Gamma1);
	}
}



/* Apply linearization. WORD encoded version */

void cmsxApplyLinearizationGamma(WORD In[3], LPGAMMATABLE Gamma[3], WORD Out[3])
{
        L16PARAMS Lut16;

        cmsCalcL16Params(Gamma[0] -> nEntries, &Lut16);

        Out[0] = cmsLinearInterpLUT16(In[0], Gamma[0] -> GammaTable, &Lut16);
        Out[1] = cmsLinearInterpLUT16(In[1], Gamma[1] -> GammaTable, &Lut16);
        Out[2] = cmsLinearInterpLUT16(In[2], Gamma[2] -> GammaTable, &Lut16);


}



/* Apply linearization. double version */

void cmsxApplyLinearizationTable(double In[3], LPGAMMATABLE Gamma[3], double Out[3])
{
        WORD rw, gw, bw;
        double rd, gd, bd;
        L16PARAMS Lut16;


        cmsCalcL16Params(Gamma[0] -> nEntries, &Lut16);

		rw = (WORD) floor(_cmsxSaturate255To65535(In[0]) + .5);
		gw = (WORD) floor(_cmsxSaturate255To65535(In[1]) + .5);
		bw = (WORD) floor(_cmsxSaturate255To65535(In[2]) + .5);

        rd = cmsLinearInterpLUT16(rw , Gamma[0] -> GammaTable, &Lut16);
        gd = cmsLinearInterpLUT16(gw,  Gamma[1] -> GammaTable, &Lut16);
        bd = cmsLinearInterpLUT16(bw,  Gamma[2] -> GammaTable, &Lut16);

        Out[0] = _cmsxSaturate65535To255(rd);            /* back to 0..255 */
        Out[1] = _cmsxSaturate65535To255(gd);
        Out[2] = _cmsxSaturate65535To255(bd);
}

