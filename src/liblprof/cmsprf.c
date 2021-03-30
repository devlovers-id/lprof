/* $Id: cmsprf.c,v 1.20 2006/04/14 05:29:16 jpizzi Exp $ */
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
/* */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2 of the License, or (at your option) any later version. */
/* */
/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */
/* */
/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "lcmsprf.h"

#if defined( _WIN32 )
#include <io.h>
#include <malloc.h>
#endif

#if !defined(__WIN32__) && (defined(_WIN32) || defined(WIN32))
#define __WIN32__
#endif


double cdecl _cmsxSaturate65535To255(double d);
double cdecl _cmsxSaturate255To65535(double d);


void   cdecl _cmsxClampXYZ100(LPcmsCIEXYZ xyz);


BOOL cdecl cmsxEmbedCharTarget(LPPROFILERCOMMONDATA hdr);
BOOL cdecl cmsxEmbedMatrixShaper(LPPROFILERCOMMONDATA hdr);
BOOL cdecl cmsxEmbedTextualInfo(LPPROFILERCOMMONDATA hdr);

/*
 * compute squared norm of a double[3]
 */

double
cmsxNorm3Sq(const double *x)
{
    return x[0] * x[0] + x[1] * x[1] + x[2] * x[2];
}

/*
 * cmsxDsort	sort double array
 * cmsxMedian	compute median of sorted double array
 * cmsxMad	compute median and MAD of sorted double array
 */

static int
cmp_double (const void *a, const void *b)
{
    const double *p1 = (const double *) a;
    const double *p2 = (const double *) b;
    if (*p1 == *p2)
        return 0;
    if (*p1 < *p2)
        return -1;
    return 1;
}

void
cmsxDsort(double *array, size_t n)
{
   qsort(array, n, sizeof(double), cmp_double);
}

double
cmsxMedian(const double *array, size_t n)
{
   if (n == 0) return 0;
   if (n & 1)  return array[n/2];
   return (array[n/2] + array[n/2-1]) / 2;
}

void
cmsxMad(const double *array, size_t n, double *median, double *mad)
{
   size_t i;
   double mid;
//   double deviation[n];
   double* deviation = alloca(sizeof( double ) * n);

   *median = mid = cmsxMedian(array, n);

   for (i = 0; i < n; i++) {
       deviation[i] = fabs(array[i] - mid);
   }

   cmsxDsort(deviation, n);
   *mad = cmsxMedian(deviation, n);
}

/****************************************************************************/

/*
 * Mersenne Twister random number generator.
 *
 * Derived from http://dirk.eddelbuettel.com/code/octave-mt.html
 * and modified for lprof.
 *
 * This is the ``Mersenne Twister'' random number generator MT19937, which
 * generates pseudorandom integers uniformly distributed in 0..(2^32 - 1)
 * starting from any odd seed in 0..(2^32 - 1). This version is a recode
 * by Shawn Cokus (Cokus@math.washington.edu) on March 8, 1998 of a version by
 * Takuji Nishimura (who had suggestions from Topher Cooper and Marc Rieffel in
 * July-August 1997).
 *
 * Effectiveness of the recoding (on Goedel2.math.washington.edu, a DEC Alpha
 * running OSF/1) using GCC -O3 as a compiler: before recoding: 51.6 sec. to
 * generate 300 million random numbers; after recoding: 24.0 sec. for the same
 * (i.e., 46.5% of original time), so speed is now about 12.5 million random
 * number generations per second on this machine.
 *
 * According to the URL <http://www.math.keio.ac.jp/~matumoto/emt.html>
 * (and paraphrasing a bit in places), the Mersenne Twister is ``designed
 * with consideration of the flaws of various existing generators,'' has
 * a period of 2^19937 - 1, gives a sequence that is 623-dimensionally
 * equidistributed, and ``has passed many stringent tests, including the
 * die-hard test of G. Marsaglia and the load test of P. Hellekalek and
 * S. Wegenkittl.''  It is efficient in memory usage (typically using 2506
 * to 5012 bytes of static data, depending on data type sizes, and the code
 * is quite short as well).  It generates random numbers in batches of 624
 * at a time, so the caching and pipelining of modern systems is exploited.
 * It is also divide- and mod-free.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation (either version 2 of the License or, at your
 * option, any later version).  This library is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY, without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU Library General Public License for more details.  You should have
 * received a copy of the GNU Library General Public License along with this
 * library; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * The code as Shawn received it included the following notice:
 *
 *   Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.  When
 *   you use this, send an e-mail to <matumoto@math.keio.ac.jp> with
 *   an appropriate reference to your work.
 *
 * It would be nice to CC: <Cokus@math.washington.edu> when you write.
 */

#include <stdio.h>
#include <stdlib.h>

/*
 * icUInt32Number must be an unsigned integer type capable of holding at least
 * 32 bits; exactly 32 should be fastest, but 64 is better on an Alpha with
 * GCC at -O3 optimization so try your options and see what's best for you
 */

#define N              (624)	// length of state vector
#define M              (397)	// a period parameter
#define K              (0x9908B0DFU)	// a magic constant
#define hiBit(u)       ((u) & 0x80000000U)	// mask all but highest   bit of u
#define loBit(u)       ((u) & 0x00000001U)	// mask all but lowest    bit of u
#define loBits(u)      ((u) & 0x7FFFFFFFU)	// mask     the highest   bit of u
#define mixBits(u, v)  (hiBit(u)|loBits(v))	// move hi bit of u to hi bit of v

static icUInt32Number state[N + 1];	// state vector + 1 extra to not violate ANSI C
static icUInt32Number *next;		// next random value is computed from here
static int left = -1;			// can *next++ this many times before reloading

void
cmsxRandSeed(icUInt32Number seed)
{
    /*
     * We initialize state[0..(N-1)] via the generator
     *
     *   x_new = (69069 * x_old) mod 2^32
     *
     * from Line 15 of Table 1, p. 106, Sec. 3.3.4 of Knuth's
     * _The Art of Computer Programming_, Volume 2, 3rd ed.
     *
     * Notes (SJC): I do not know what the initial state requirements
     * of the Mersenne Twister are, but it seems this seeding generator
     * could be better.  It achieves the maximum period for its modulus
     * (2^30) iff x_initial is odd (p. 20-21, Sec. 3.2.1.2, Knuth); if
     * x_initial can be even, you have sequences like 0, 0, 0, ...;
     * 2^31, 2^31, 2^31, ...; 2^30, 2^30, 2^30, ...; 2^29, 2^29 + 2^31,
     * 2^29, 2^29 + 2^31, ..., etc. so I force seed to be odd below.
     *
     * Even if x_initial is odd, if x_initial is 1 mod 4 then
     *
     *   the          lowest bit of x is always 1,
     *   the  next-to-lowest bit of x is always 0,
     *   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
     *   the 3rd-from-lowest bit of x 4-cycles        ... 0 1 1 0 0 1 1 0 ... ,
     *   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 0 1 1 1 1 0 ... ,
     *    ...
     *
     * and if x_initial is 3 mod 4 then
     *
     *   the          lowest bit of x is always 1,
     *   the  next-to-lowest bit of x is always 1,
     *   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
     *   the 3rd-from-lowest bit of x 4-cycles        ... 0 0 1 1 0 0 1 1 ... ,
     *   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 1 1 1 1 0 0 ... ,
     *    ...
     *
     * The generator's potency (min. s>=0 with (69069-1)^s = 0 mod 2^32) is
     * 16, which seems to be alright by p. 25, Sec. 3.2.1.3 of Knuth.  It
     * also does well in the dimension 2..5 spectral tests, but it could be
     * better in dimension 6 (Line 15, Table 1, p. 106, Sec. 3.3.4, Knuth).
     *
     * Note that the random number user does not see the values generated
     * here directly since reloadMT() will always munge them first, so maybe
     * none of all of this matters.  In fact, the seed values made here could
     * even be extra-special desirable if the Mersenne Twister theory says
     * so-- that's why the only change I made is to restrict to odd seeds.
     */

    int j;
    icUInt32Number x = (seed | 1U) & 0xFFFFFFFFU, *s = state;

    for (left = 0, *s++ = x, j = N; --j; *s++ = (x *= 69069U) & 0xFFFFFFFFU);
}

static icUInt32Number
reloadMT(void)
{
    int j;
    icUInt32Number *p0 = state, *p2 = state + 2, *pM = state + M, s0, s1;

    if (left < -1)
	cmsxRandSeed (4357U);

    left = N - 1, next = state + 1;

    for (s0 = state[0], s1 = state[1], j = N - M + 1; --j;
	 s0 = s1, s1 = *p2++)
	*p0++ = *pM++ ^ (mixBits (s0, s1) >> 1) ^ (loBit (s1) ? K : 0U);

    for (pM = state, j = M; --j; s0 = s1, s1 = *p2++)
	*p0++ = *pM++ ^ (mixBits (s0, s1) >> 1) ^ (loBit (s1) ? K : 0U);

    s1 = state[0], *p0 =
	*pM ^ (mixBits (s0, s1) >> 1) ^ (loBit (s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 << 7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return (s1 ^ (s1 >> 18));
}

/* generate 32-bit integer random number */

icUInt32Number
cmsxRandInt(void)
{
    icUInt32Number y;

    if (--left < 0)
	return (reloadMT ());

    y = *next++;
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return (y ^ (y >> 18));
}

/* generate [0,1) double random number */

double
cmsxRand(void)
{
    /* 2^32 */
    return cmsxRandInt () * (1.0 / 4294967296.0);
}

/* generate [0,1] double random number */
/* (closed interval) */

double
cmsxRandClosed(void)
{
    /* 2^32-1 */
    return cmsxRandInt () * (1.0 / 4294967295.0);
}

/****************************************************************************/

/*
 * generate normally distributed random numbers using the
 * polar method, see http://de.wikipedia.org/wiki/Polar-Methode
 */

static void
norm_rand (double *rns, int n)
{
    int i;
    double w, x1, x2;

    for (i = 0; i < n; i += 2)
    {
	do {
            x1 = 2.0 * cmsxRand() - 1.0;
            x2 = 2.0 * cmsxRand() - 1.0;
            w = (x1*x1) + (x2*x2);
	} while (w > 1.0);

        w = sqrt((-2.0 * log(w)) / w);
        rns[i]   = x1 * w;
        rns[i+1] = x2 * w;
    }
}

double
cmsxRandn(void)
{
    static double rns[1024];
    static int left = 0;

    if (left > 0)
        return rns[--left];

    norm_rand (rns, 1024);
    left = 1024;

    return rns[--left];
}

/****************************************************************************/

/* Convert from 0.0..65535.0 to 0.0..255.0 */

double _cmsxSaturate65535To255(double d)
{   
    double v;

    v = d / 257.0;

    if (v < 0)     return 0;
    if (v > 255.0) return 255.0;

    return v;
}


double _cmsxSaturate255To65535(double d)
{   
    double v;

    v = d * 257.0;

    if (v < 0)     return 0;
    if (v > 65535.0) return 65535.0;

    return v;
}



/* Cut off absurd values */

void _cmsxClampXYZ100(LPcmsCIEXYZ xyz)
{

    if (xyz->X > 199.996) xyz->X = 199.996;
                
    if (xyz->Y > 199.996) xyz->Y = 199.996;
        
    if (xyz->Z > 199.996) xyz->Z = 199.996;
        
    if (xyz->Y < 0) xyz->Y = 0;

    if (xyz->X < 0) xyz->X = 0;

    if (xyz->Z < 0) xyz->Z = 0;

}

static
int xfilelength(int fd)
{
#ifdef _MSC_VER
	return _filelength(fd);
#else
        struct stat sb;
        if (fstat(fd, &sb) < 0)
                return(-1);
        return(sb.st_size);
#endif


}


BOOL cmsxEmbedCharTarget(LPPROFILERCOMMONDATA hdr)
{
    LCMSHANDLE it8 = cmsxIT8Alloc();
    LPBYTE mem;
    size_t size, readed;
    FILE* f;
    BOOL lFreeOnExit = FALSE;

    if (!hdr->m.Patches) 
    {
        if (!hdr ->ReferenceSheet[0] && !hdr->MeasurementSheet[0]) return FALSE;

        if (cmsxPCollBuildMeasurement(&hdr ->m, 
                                      hdr->ReferenceSheet, 
                                      hdr->MeasurementSheet,
                                      PATCH_HAS_RGB|PATCH_HAS_XYZ) == FALSE) return FALSE;
        lFreeOnExit = TRUE;
    }

    cmsxIT8SetSheetType(it8,"LCMSEMBED");
    cmsxIT8SetProperty(it8, "ORIGINATOR",   (const char *) "Little cms");
    cmsxIT8SetProperty(it8, "DESCRIPTOR",   (const char *) hdr -> Description);
    cmsxIT8SetProperty(it8, "MANUFACTURER", (const char *) hdr ->Manufacturer);
    /* fix me segfaults someplace in here when being run 
       from a directory that the user does not have write 
       access.
    */			
    cmsxPCollSaveToSheet(&hdr->m, it8);
 
    cmsxIT8SaveToFile(it8, hdr->temp_file);
    cmsxIT8Free(it8);

    f = fopen(hdr->temp_file, "rb");
    size = xfilelength(fileno(f));
    mem = (char*) malloc(size + 1);
    readed = fread(mem, 1, size, f);
    fclose(f);

    mem[readed] = 0;
    unlink(hdr->temp_file);

    cmsAddTag(hdr->hProfile, icSigCharTargetTag, mem);
    free(mem);

    if (lFreeOnExit) 
    {
        cmsxPCollFreeMeasurements(&hdr->m);
    }

    return TRUE;
}


static
BOOL ComputeColorantMatrix(LPcmsCIEXYZTRIPLE Colorants, 
			   LPcmsCIExyY WhitePoint, 
			   LPcmsCIExyYTRIPLE Primaries)
{
    MAT3 MColorants;
 
    if (!cmsBuildRGB2XYZtransferMatrix(&MColorants, WhitePoint, Primaries))
    {
        return FALSE;
    }  

    cmsAdaptMatrixToD50(&MColorants, WhitePoint);

    Colorants->Red.X = MColorants.v[0].n[0];
    Colorants->Red.Y = MColorants.v[1].n[0];
    Colorants->Red.Z = MColorants.v[2].n[0];

    Colorants->Green.X = MColorants.v[0].n[1];
    Colorants->Green.Y = MColorants.v[1].n[1];
    Colorants->Green.Z = MColorants.v[2].n[1];
       
    Colorants->Blue.X = MColorants.v[0].n[2];
    Colorants->Blue.Y = MColorants.v[1].n[2];
    Colorants->Blue.Z = MColorants.v[2].n[2];

    return TRUE;
}


BOOL cmsxEmbedMatrixShaper(LPPROFILERCOMMONDATA hdr)
{
    cmsCIEXYZTRIPLE Colorant;
    cmsCIExyY MediaWhite;

    cmsXYZ2xyY(&MediaWhite, &hdr ->WhitePoint);

    if (ComputeColorantMatrix(&Colorant, &MediaWhite, &hdr ->Primaries)) 
    {
        cmsAddTag(hdr ->hProfile, icSigRedColorantTag, &Colorant.Red);
        cmsAddTag(hdr ->hProfile, icSigGreenColorantTag, &Colorant.Green);
        cmsAddTag(hdr ->hProfile, icSigBlueColorantTag, &Colorant.Blue);
    }
		
    cmsAddTag(hdr ->hProfile, icSigRedTRCTag, hdr ->Gamma[0]);
    cmsAddTag(hdr ->hProfile, icSigGreenTRCTag, hdr ->Gamma[1]);
    cmsAddTag(hdr ->hProfile, icSigBlueTRCTag, hdr ->Gamma[2]);

    return TRUE;
}


BOOL cmsxEmbedTextualInfo(LPPROFILERCOMMONDATA hdr)
{
    if (*hdr ->Description)
        cmsAddTag(hdr ->hProfile, icSigProfileDescriptionTag, hdr ->Description);
     
    if (*hdr ->Copyright)
        cmsAddTag(hdr ->hProfile, icSigCopyrightTag,          hdr ->Copyright);
     
    if (*hdr ->Manufacturer)
        cmsAddTag(hdr ->hProfile, icSigDeviceMfgDescTag,      hdr ->Manufacturer);
     
    if (*hdr ->Model)
        cmsAddTag(hdr ->hProfile, icSigDeviceModelDescTag,    hdr ->Model);

    return TRUE;
}



void cmsxChromaticAdaptationAndNormalization(LPPROFILERCOMMONDATA hdr, LPcmsCIEXYZ xyz, BOOL lReverse)
{
    
    if (hdr->lUseCIECAM97s) 
    {
        cmsJCh JCh;

       /* Let's CIECAM97s to do the adaptation to D50 */
			   
       xyz->X *= 100.;
       xyz->Y *= 100.;
       xyz->Z *= 100.;

       _cmsxClampXYZ100(xyz);

       if (lReverse) 
       {
           cmsCIECAM97sForward(hdr->hPCS, xyz, &JCh);
	   cmsCIECAM97sReverse(hdr->hDevice, &JCh, xyz);
       }
       else 
       {
           cmsCIECAM97sForward(hdr->hDevice, xyz, &JCh);
           cmsCIECAM97sReverse(hdr->hPCS, &JCh, xyz);
       }

       _cmsxClampXYZ100(xyz);

       xyz -> X /= 100.;
       xyz -> Y /= 100.;
       xyz -> Z /= 100.;
							   
   }
   else 
   { 
       /* Else, use Bradford */
       if (lReverse) 
       {
           cmsAdaptToIlluminant(xyz, cmsD50_XYZ(), &hdr->WhitePoint,  xyz);            
       }
       else 
       {
           cmsAdaptToIlluminant(xyz, &hdr->WhitePoint, cmsD50_XYZ(), xyz);            
       } 
   }
}


void cmsxInitPCSViewingConditions(LPPROFILERCOMMONDATA hdr)
{
    hdr->PCS.whitePoint.X = cmsD50_XYZ()->X * 100.;
    hdr->PCS.whitePoint.Y = cmsD50_XYZ()->Y * 100.;
    hdr->PCS.whitePoint.Z = cmsD50_XYZ()->Z * 100.;

    hdr->PCS.Yb = 20;                     /* 20% of surround */
    hdr->PCS.La = 20;                     /* Adapting field luminance */
    hdr->PCS.surround = AVG_SURROUND;
    hdr->PCS.D_value  = 1.0;			 /* Complete adaptation */
}


/* Build gamut hull by geometric means */
void cmsxComputeGamutHull(LPPROFILERCOMMONDATA hdr)
{	
    int i;
    int x0, y0, z0;
    int Inside, Outside, Boundaries;
    char code;

    hdr -> hRGBHull = cmsxHullInit();

    /* For all valid patches, mark RGB knots as 0 */
    for (i=0; i < hdr ->m.nPatches; i++) 
    {
        if (hdr ->m.Allowed[i]) 
        {
            LPPATCH p = hdr ->m.Patches + i;
            x0 = (int) floor(p->Colorant.RGB[0]  + .5);
            y0 = (int) floor(p->Colorant.RGB[1]  + .5);
            z0 = (int) floor(p->Colorant.RGB[2]  + .5);
            cmsxHullAddPoint(hdr->hRGBHull, x0, y0, z0);
        }
    }

    cmsxHullComputeHull(hdr ->hRGBHull);

/* #ifdef DEBUG */
        /* this should create a VRML file in the same directory as the profile */
 /*      char VRMLfile[256]; */
 /* strcpy(VRMLfile, hdr->OutputProfileFile); */
 /*      strcat(VRMLfile, ".wrl"); */
 /* cmsxHullDumpVRML(hdr -> hRGBHull, VRMLfile */ /*"rgbhull.wrl" */ /* ); */
/* #endif */



    /* A check */

    Inside = Outside = Boundaries = 0;
    /* For all valid patches, mark RGB knots as 0 */
    for (i=0; i < hdr ->m.nPatches; i++) 
    {
        if (hdr ->m.Allowed[i]) 
        {
            LPPATCH p = hdr ->m.Patches + i;
            x0 = (int) floor(p->Colorant.RGB[0]  + .5);
            y0 = (int) floor(p->Colorant.RGB[1]  + .5);
            z0 = (int) floor(p->Colorant.RGB[2]  + .5);

            code = cmsxHullCheckpoint(hdr -> hRGBHull, x0, y0, z0);

	    switch (code)
            {
                case 'i': Inside++; break;
                case 'o': Outside++; break;
                default:  Boundaries++;
             }
        }
    }
 
    if (hdr ->cmsprintf)
        hdr ->cmsprintf("Gamut hull: %1 inside, %2 outside, %3 on boundaries", 3, 'd', 
                        Inside, Outside, Boundaries);
}

BOOL cmsxChoosePCS(LPPROFILERCOMMONDATA hdr)
{

    double gamma_r, gamma_g, gamma_b;        
    cmsCIExyY SourceWhite;
    // int parms;
    char temp[40];
    char temp2[30];
    int i;

    /* At first, compute approximation on matrix-shaper */
    if (!cmsxComputeMatrixShaper(hdr ->ReferenceSheet,
                                 hdr ->MeasurementSheet,
                                 hdr -> Medium,
                                 hdr ->Gamma,
                                 &hdr ->WhitePoint,
                                 &hdr ->BlackPoint,
                                 &hdr ->Primaries)) return FALSE;

        		

    cmsXYZ2xyY(&SourceWhite,   &hdr ->WhitePoint);	
		    
    gamma_r = cmsEstimateGamma(hdr ->Gamma[0]);
    gamma_g = cmsEstimateGamma(hdr ->Gamma[1]);
    gamma_b = cmsEstimateGamma(hdr ->Gamma[2]);
	
    if (gamma_r > 1.8 || gamma_g > 1.8 || gamma_b > 1.8 ||
        gamma_r == -1 || gamma_g == -1 || gamma_b == -1) 
    {

       hdr ->PCSType = PT_Lab;

       if (hdr ->cmsprintf)
           hdr ->cmsprintf("I have chosen CIE Lab as PCS", 0, 'c');                        

    }
    else 
    {
        // hdr ->PCSType = PT_XYZ;
	// XXX fuer
        hdr ->PCSType = PT_Lab;
        /* message is incorrect */
        /* fix me */
        if (hdr ->cmsprintf)
            hdr ->cmsprintf("I have chosen CIE XYZ as PCS", 0, 'c');                        
     }

     if (hdr ->cmsprintf) 
     {
         char Buffer[256] = "Inferred ";
         /* The white point string is not translatable yet */
         /* fix me */
         _cmsIdentifyWhitePoint(Buffer, &hdr ->WhitePoint);
                   
         /* parse Buffer to make it translatable */
         if (strncmp(Buffer, "WhitePoint", 10) ==0)
         { 
             strcpy(temp, "White point near");
             strcat(temp, " %1");
             strcpy(temp2, strtok(Buffer, ":"));
             strcpy(temp2, strtok(NULL, "\0"));
         }
         else
         {
            strcpy(temp, strtok(Buffer, " "));
            strcat(temp, " ");
                   
            for (i=2; i<=3; i++)
            {
                strcat(temp, strtok(NULL, " "));
                strcat(temp, " ");
            }
            strcat(temp, "%1");
            if (strncmp(Buffer, "White", 5)==0)
            {
                strcpy(temp2, strtok(NULL, "K")); 
                strcat(temp2, "K");
            }
            else
            {
                strcpy(temp2, strtok(NULL, ")")); 
                strcat(temp2, ")");
            }
        } 

        hdr ->cmsprintf(temp, 1, 's', temp2); 
        hdr ->cmsprintf("Primaries (x-y): [Red: %1, %2] [Green: %3, %4] [Blue: %5, %6]", 6, 'f',
                        hdr ->Primaries.Red.x, hdr ->Primaries.Red.y, 
                        hdr ->Primaries.Green.x, hdr ->Primaries.Green.y,
                        hdr ->Primaries.Blue.x, hdr ->Primaries.Blue.y);

       if ((gamma_r != -1) && (gamma_g != -1) && (gamma_b != -1)) 
       {
           hdr ->cmsprintf("Estimated gamma: [Red: %1] [Green: %2] [Blue: %3]", 3, 'f',
                            gamma_r, gamma_g, gamma_b);
       }

    }
		
    return TRUE;
}
