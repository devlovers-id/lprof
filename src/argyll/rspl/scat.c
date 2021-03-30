/* $Id: scat.c,v 1.6 2006/04/05 02:11:59 jpizzi Exp $ */
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized splines data fitter
 *
 * Author: Graeme W. Gill
 * Date:   2004/8/14
 *
 * Copyright 1996 - 2005 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

/*
 * This file contains the scattered data solution specific code.
 *
 * The regular spline implementation was inspired by the following technical reports:
 *
 * D.J. Bone, "Adaptive Multi-Dimensional Interpolation Using Regularized Linear Splines,"
 * Proc. SPIE Vol. 1902, p.243-253, Nonlinear Image Processing IV, Edward R. Dougherty;
 * Jaakko T. Astola; Harold G. Longbotham;(Eds)(1993).
 * 
 * D.J. Bone, "Adaptive Colour Printer Modeling using regularized linear splines,"
 * Proc. SPIE Vol. 1909, p. 104-115, Device-Independent Color Imaging and Imaging
 * Systems Integration, Ricardo J. Motta; Hapet A. Berberian; Eds.(1993)
 *
 * Don Bone and Duncan Stevenson, "Modelling of Colour Hard Copy Devices Using Regularised
 * Linear Splines," Proceedings of the APRS workshop on Colour Imaging and Applications,
 * Canberra (1994)
 *
 * see <http://www.cmis.csiro.au/Don.Bone/>
 *
 * Also of interest was:
 * 
 * "Discrete Smooth Interpolation", Jean-Laurent Mallet, ACM Transactions on Graphics, 
 * Volume 8, Number 2, April 1989, Pages 121-144.
 * 
 */

/* TTBD:
 *
 *  Speedup that skips recomputing all of A to add new points seems OK. (nothing uses
 *  incremental currently.)
 *
 *  Is there any way of speeding up incremental recalculation ????
 *
 * Add optional simplex point interpolation to
 * solve setup.
 *
 * Get rid of error() calls - return status instead
 */

/*
	Scattered data fit related smoothness control.

	We adjust the curve/data point weighting to account for the
	grid resolution (to make it resolution independent), as well
	as allow for the dimensionality (each dimension contributes
	a curvature error).

	The default assumption is that the grid resolution matches
	the input data range for that dimension, eg. if a sub range
	of input space is all that is needed, then a smaller grid
	resolution can should be used if smoothness is expected
	to remain symetric in relation to the input domain.

	eg. Input range 0.0 - 1.0 and 0.0 - 0.5
	   matching res 50        and 25

	The alternative is to set the RSPL_SYMDOMAIN flag,
	in which case the grid resolution is not taken to
	be a measure of the dimension scale, and is assumed
	to be just a lower resolution sampling of the domain.

	eg. Input range 0.0 - 1.0 and 0.0 - 1.0
	   with res.    50        and 25

	still has symetrical smoothness in relation
	to the input domain.


	NOTE :- that both input and output values are normalised
	by the ranges given during rspl construction. The ranges
	set the significance between the input and output values.

	eg. Input ranges 0.0 - 1.0 and 0.0 - 100.0
	(with equal grid resolution)
	will have symetry when measured against the the
	same % change in the input domain, but will
	appear non-symetric if measured against the
	same numerical change.

 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#if defined(__IBMC__) && defined(_M_IX86)
#include <float.h>
#endif

#include "rspl_imp.h"
#include "numlib.h"

extern void error(char *fmt, ...), warning(char *fmt, ...);

#undef DEBUG

/* algorithm parameters */
#undef POINTWEIGHT		/* Increas smoothness weighting proportional to number of points */

/* Tuning parameters */
#ifdef NEVER		/* More accurate for extrapolation: */
#define TOL 1e-6		/* Tollerance of result - usually 1e-5 is best. */
#define TOL_IMP 1.0		/* Minimum error improvement to continue - reduces accuracy */
#else				/* Speeds things up for a slight loss of acuracy: */
#define TOL 1e-5		/* Tollerance of result - usually 1e-5 is best. */
#define TOL_IMP 0.98	/* Minimum error improvement to continue - reduces accuracy */
#endif
#undef GRADUATED_TOL	/* Speedup attemp - use reduced tollerance for prior grids. */
#define GRATIO 2.0		/* Multi-grid ratio */
#define OVERRLX 		/* Use over relaxation factor when progress slows - improves accuracy */
#define JITTERS 0		/* Number of 1D conjugate solve itters */
#define CONJ_TOL 1.0	/* Extra tolereance on 1D conjugate solution times TOL. */
#define MAXNI 16		/* Maximum itteration without checking progress */
/* #define SMOOTH 0.000100 */	/* Set nominal smoothing (1.0) */
#define WEAKW  0.1		/* Weak default function nominal effect (1.0) */

#define NME_TARGET 0.5	/* nme error target */

#undef DEBUG_PROGRESS	/* Print progress of acheiving tollerance target */

#undef NEVER
#define ALWAYS

/* Implemented in rspl.c: */
extern void alloc_grid(rspl *s);

extern int is_mono(rspl *s);

/* Convention is to use:
   i to index grid points u.a
   n to index data points d.a
   e to index position dimension di
   f to index output function dimension fdi
   j misc and cube corners
   k misc
 */

/* ================================================= */
/* Structure to hold temporary data for multi-grid calculations */
/* One is created for each resolution. Only used in this file. */
struct _mgtmp {
	rspl *s;	/* Associated rspl */
	int f;		/* Output dimension being calculated */
	int xfe;	/* Extra fitting factors activated in the mgtmp */
	int i2;		/* Incremental 2nd or more round */

	/* Weak default function stuff */
	double wdfw;			/* Weight per grid point */

	/* Scattered data fit stuff */
	struct {
		double cw[MXDI];	/* Curvature weight factor */
	} sf;

	/* Grid points data */
	struct {
		int res[MXDI];	/* Single dimension grid resolution */
		int bres, brix;	/* Biggest resolution and its index */
		int mres;		/* Geometric mean res[] */
		int no;			/* Total number of points in grid = res ^ di */
		ratai l,h,w;	/* Grid low, high, grid cell width */

		/* Grid array offset lookups */
		int ci[MXRI];		/* Grid coordinate increments for each dimension */
		int hi[POW2MXRI];	/* Combination offset for sequence through cube. */
	} g;

	/* Data point grid dependent information */
	struct mgdat {
		int b;				/* Index for associated base grid point, in grid points */
		double w[POW2MXRI];	/* Weight for surrounding gridpoints [2^di] */
	
		double kx;			/* Extra k to fix non-monotonicity */
		double nme;			/* Nonmon Error value of associated grid point */
		double nmd;			/* Distance to nearest nme squared */
		double nmw;			/* Non-mono correction weight */

		/* Extra fit information */
		double ierr;		/* Data point fitting initial error */
	} *d;
	/* Extra fit information */
	double aierr;			/* Average absolute initial error */

	/* Non-monotonic correction stuff */
	struct {
		double cf;		/* Non-mono correction factor */
		double max;		/* Maximum nme discoverd in check_monotonic */
		double tol;		/* Tollerance parameter for solver */
		int final;		/* Final flag for solver */
	} nm;

	/* Equation Solution related (Grid point solutions) */
	struct {
		double **A;			/* A matrix of interpoint weights A[g.no][q.acols] */
		int acols;			/* A matrix columns needed */
		int xcol[HACOMPS+8];/* A array column translation from packed to sparse index */ 
		int *ixcol;			/* A array column translation from sparse to packed index */ 
		double *b;			/* b vector for RHS of simultabeous equation b[g.no] */
		double normb;		/* normal of b vector */
		double *x;			/* x solution to A . x = b */
	} q;

}; typedef struct _mgtmp mgtmp;


/* ================================================= */
static int add_rspl_imp(rspl *s, int flags, void *d, int dtp, int dno);
static void fix_nme(mgtmp  *m, double tol, int final);
static mgtmp *new_mgtmp(rspl *s, int gres[MXDI], int f);
static void reinit_mgtmp(mgtmp *m);
static void free_mgtmp(mgtmp *m);
static void setup_solve(mgtmp *m);
static void solve_gres(mgtmp *m, double tol, int final);
static void init_soln(mgtmp  *m1, mgtmp  *m2);
static void comp_extrafit_err(mgtmp *m);

/* Initialise the regular spline from scattered data */
/* Return non-zero if non-monotonic */
static int
fit_rspl_imp(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	void *d,		/* Array holding position and function values of data points */
	int dtp,		/* Flag indicating data type, 0 = (co *), 1 = (cow *) */
	int dno,		/* Number of data points */
	ratai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	ratai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution */
	ratao vlow,		/* Data value low normalize, NULL = default 0.0 */
	ratao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, nominal = 1.0 */
					/* (if -ve, overides optimised smoothing, and sets raw smoothing */
					/*  typically between 1e-7 .. 1e-1) */
	double avgdev,	/* Average Deviation of input values as proportion of input range, */
					/* typical value 0.005 ? (aprox. = 0.564 times the standard deviation) */
	double weak,	/* Weak weighting, nominal = 1.0 */
	void *dfctx,	/* Opaque weak default function context */
	void (*dfunc)(void *cbntx, double *out, double *in)		/* Function to set from, NULL if none. */
) {
	int di = s->di, fdi = s->fdi;
	int i, /* n, */ e, f;
	/* int nigc; */

#if defined(__IBMC__) && defined(_M_IX86)
	_control87(EM_UNDERFLOW, EM_UNDERFLOW);
#endif

	/* This is a restricted size function */
	if (di > MXRI)
		error("rspl: fit can't handle di = %d",di);
	if (fdi > MXRO)
		error("rspl: fit can't handle fdi = %d",fdi);

	/* set debug level */
	s->debug = (flags >> 24);

	/* Init other flags */
	s->nm = (flags & RSPL_NONMON) ? 1 : 0;			/* Enable elimination of non-monoticities */
	s->xf = (flags & RSPL_EXTRAFIT) ? 1 : 0;		/* Enable extra fitting effort */
	s->symdom = (flags & RSPL_SYMDOMAIN) ? 1 : 0;	/* Turn on symetric smoothness with gres */
	s->inc = (flags & RSPL_INCREMENTAL) ? 1 : 0;	/* Enable incremental scattered mode */
	s->verbose = (flags & RSPL_VERBOSE) ? 1 : 0;	/* Turn on progress messages to stdout */

	/* Save smoothing factor and Average Deviation */
	s->smooth = smooth;
	s->avgdev = avgdev;

	/* Save weak default function information */
	s->weak = weak;
	s->dfctx = dfctx;
	s->dfunc = dfunc;

	/* Grid hasn't been initialised from scattered data yet */
	s->sinit = 0;

	/* Init data point storage to zero */
	s->d.no = 0;
	s->d.ano = 0;
	s->d.a = NULL;

	/* record low and high grid range */
	s->g.mres = 1.0;
	s->g.bres = 0;
	for (e = 0; e < s->di; e++) {
		if (gres[e] < 2)
			error("rspl: grid res must be >= 2!");
		s->g.res[e] = gres[e]; /* record the desired resolution of the grid */
		s->g.mres *= gres[e];
		if (gres[e] > s->g.bres) {
			s->g.bres = gres[e];
			s->g.brix = e;
		}

		if (glow == NULL)
			s->g.l[e] = 0.0;
		else
			s->g.l[e] = glow[e];

		if (ghigh == NULL)
			s->g.h[e] = 1.0;
		else
			s->g.h[e] = ghigh[e];
	}
	s->g.mres = pow(s->g.mres, 1.0/e);		/* geometric mean */

	/* record low and high data normalizing factors */
	for (f = 0; f < s->fdi; f++) {
		if (vlow == NULL)
			s->d.vl[f] = 0.0;
		else
			s->d.vl[f] = vlow[f];

		if (vhigh == NULL)
			s->d.vw[f] = 1.0;
		else
			s->d.vw[f] = vhigh[f];
	}

	/* If we are supplied initial data points, expand the */
	/* grid range to be able to cover it. */
	/* Also compute average data value. */
	for (f = 0; f < s->fdi; f++)
		s->d.va[f] = 0.5;	/* default average */
	if (dtp == 0) {			/* Default weight */
		co *dp = (co *)d;

		for (i = 0; i < dno; i++) {
			for (e = 0; e < s->di; e++) {
				if (dp[i].p[e] > s->g.h[e])
					s->g.h[e] = dp[i].p[e];
				if (dp[i].p[e] < s->g.l[e])
					s->g.l[e] = dp[i].p[e];
			}
			for (f = 0; f < s->fdi; f++) {
				if (dp[i].v[f] > s->d.vw[f])
					s->d.vw[f] = dp[i].v[f];
				if (dp[i].v[f] < s->d.vl[f])
					s->d.vl[f] = dp[i].v[f];
				s->d.va[f] += dp[i].v[f];
			}
		}
	} else {				/* Per data point weight */
		cow *dp = (cow *)d;

		for (i = 0; i < dno; i++) {
			for (e = 0; e < s->di; e++) {
				if (dp[i].p[e] > s->g.h[e])
					s->g.h[e] = dp[i].p[e];
				if (dp[i].p[e] < s->g.l[e])
					s->g.l[e] = dp[i].p[e];
			}
			for (f = 0; f < s->fdi; f++) {
				if (dp[i].v[f] > s->d.vw[f])
					s->d.vw[f] = dp[i].v[f];
				if (dp[i].v[f] < s->d.vl[f])
					s->d.vl[f] = dp[i].v[f];
				s->d.va[f] += dp[i].v[f];
			}
		}
	}
	if (dno > 0) {		/* Complete the average */
		for (f = 0; f < s->fdi; f++)
			s->d.va[f] = (s->d.va[f] - 0.5)/((double)dno);
	}

	/* compute width of each grid cell */
	for (e = 0; e < s->di; e++) {
		s->g.w[e] = (s->g.h[e] - s->g.l[e])/(double)(s->g.res[e]-1);
	}

	/* Convert low and high to low and width data range */
	for (f = 0; f < s->fdi; f++) {
		s->d.vw[f] -= s->d.vl[f];
	}

	/* Allocate the grid data */
	alloc_grid(s);
	
	/* Zero out the re-usable mgtmps */
	for (f = 0; f < s->fdi; f++) {
		s->mgtmps[f] = NULL;
	}

	{
		int sres;		/* Starting resolution */
		double res;
		double gratio;

		/* Figure out how many multigrid steps to use */
		sres = 4;		/* Else start at minimum grid res of 4 */

		/* Calculate the resolution scaling ratio and number of itters. */
		gratio = GRATIO;
		if (((double)s->g.bres/(double)sres) <= gratio) {
			s->niters = 2;
			gratio = (double)s->g.bres/(double)sres;
		} else {	/* More than one needed */
			s->niters = (int)((log((double)s->g.bres) - log((double)sres))/log(gratio) + 0.5);
			gratio = exp((log((double)s->g.bres) - log((double)sres))/(double)s->niters);
			s->niters++;
		}
		
		s->titers = s->niters;
		if (s->xf) {			/* Extra fit itterations needed */
			if (s->niters == 1)
				s->titers = 2;				/* One extra fit */
			else
				s->titers = s->niters + 2;	/* Else two extra fit */
		}

		/* Allocate space for resolutions and mgtmps pointers */
		if ((s->ires = imatrix(0, s->titers, 0, s->di)) == NULL)
			error("rspl: malloc failed - ires[][]");

		for (f = 0; f < s->fdi; f++) {
			if ((s->mgtmps[f] = (void *) calloc(s->titers, sizeof(void *))) == NULL)
				error("rspl: malloc failed - mgtmps[]");
		}

		/* Fill in the resolution values for each itteration */
		for (res = (double)sres, i = 0; i < s->niters; i++) {
			int ires;

			ires = (int)(res + 0.5);
			for (e = 0; e < s->di; e++) {
				if ((ires + 1) >= s->g.res[e])	/* If close enough biger than target res. */
					s->ires[i][e] = s->g.res[e];
				else
					s->ires[i][e] = ires;
			}
			res *= gratio;
		}

		if (s->titers > s->niters) {		/* Extra fit itterations */
			if (s->titers == 2) {
				for (e = 0; e < s->di; e++)
					s->ires[1][e] = s->ires[0][e];		/* One itteration again */
			} else {
				for (i = s->niters; i < s->titers; i++) {	/* Repeat last two itterations */
					for (e = 0; e < s->di; e++)
						s->ires[i][e] = s->ires[i-2][e];
				}
			}
		}
		
		/* Assert */
		for (e = 0; e < s->di; e++) {
			if (s->ires[s->niters-1][e] != s->g.res[e])
				error("rspl: internal error, final res %d != intended res %d\n",
				       s->ires[s->niters-1][e], s->g.res[e]);
		}

	}

	/* Do the data point fitting */
	return add_rspl_imp(s, 0, d, dtp, dno);
}

/* Do the work of initialising from initial or extra data points. */
/* Return non-zero if non-monotonic */
static int
add_rspl_imp(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	void *d,		/* Array holding position and function values of data points */
	int dtp,		/* Flag indicating data type, 0 = (co *), 1 = (cow *) */
	int dno			/* Number of data points */
) {
	int /* di = s->di, */ fdi = s->fdi;
	int i, n, e, f;
	/* int nigc; */
	int first = 0;	/* Flag, first fitting */
	int last = 0;	/* Flag, last fitting */

	if (s->sinit == 0)
		first = 1;

	if ((flags & RSPL_FINAL) || !s->inc)
		last = 1;

	if (s->d.no == 0 && dno == 0) {	/* There are no points to initialise from */
		return 0;
	}

	if (dno == 0) {		/* There is nothing new to do */
		if (last) {
			/* Free up mgtmps */
			for (f = 0; f < s->fdi; f++) {
				if (s->mgtmps[f] != NULL) {
					for (i = 0; i < s->titers; i++) {
						if (s->mgtmps[f][i] != NULL) {
							free_mgtmp(s->mgtmps[f][i]);
						}
					}
					free(s->mgtmps[f]);
					s->mgtmps[f] = NULL;
				}
			}
		}
		return is_mono(s);
	}

	s->d.ano = dno;		/* number of last added points */

	/* Allocate space for points */
	if (s->d.no == 0) {	/* First points */

		/* Allocate the scattered data space */
		if ((s->d.a = (rpnts *) malloc(sizeof(rpnts) * dno)) == NULL)
			error("rspl malloc failed - data points");

	} else if (dno > 0) {

		/* Reallocate the scattered data space */
		if ((s->d.a = (rpnts *) realloc(s->d.a, sizeof(rpnts) * (s->d.no + dno))) == NULL)
			error("rspl realloc failed - data points");
	}

	/* Add more points */
	if (dtp == 0) {			/* Default weight */
		co *dp = (co *)d;

		/* Append the list into data points */
		for (i = 0, n = s->d.no; i < dno; i++, n++) {
			for (e = 0; e < s->di; e++)
				s->d.a[n].p[e] = dp[i].p[e];
			for (f = 0; f < s->fdi; f++)
				s->d.a[n].v[f] = dp[i].v[f];
			s->d.a[n].k = 1.0;		/* Assume all data points have same weight */
		}
	} else {				/* Per data point weight */
		cow *dp = (cow *)d;

		/* Append the list into data points */
		for (i = 0, n = s->d.no; i < dno; i++, n++) {
			for (e = 0; e < s->di; e++)
				s->d.a[n].p[e] = dp[i].p[e];
			for (f = 0; f < s->fdi; f++)
				s->d.a[n].v[f] = dp[i].v[f];
			s->d.a[n].k = dp[n].w; /* Weight specified */
		}
	}
	s->d.no += dno;

	/* Do fit or re-fit of grid to data for each output dimension */
	for (f = 0; f < fdi; f++) {
		int nn;
		float *gp;
		mgtmp *m;
		int doclean = 0;		/* Do fit from scratch in incremental */

		/* For each itteration */
		for (nn = 0; nn < s->titers; nn++) {

			if (s->mgtmps[f][nn] == NULL) {	/* No mgtmp for this resolution */
				m = new_mgtmp(s, s->ires[nn], f);
				s->mgtmps[f][nn] = (void *)m;
				if (nn >= s->niters)	/* Is an extra fit itteration */
					m->xfe = 1;			/* Mark it */
				setup_solve(m);
			} else {					/* Incremental and 2nd or more times through */
				m = s->mgtmps[f][nn];	/* Re-use previous one */
				m->i2 = 1;				/* Set flag indicating incremental 2nd round */
				reinit_mgtmp(m);		/* Add in new data points to mgtmp */
				setup_solve(m);			/* to account for new data */
			}

			if (nn == 0) {				/* Make sure we have an initial x[] for end detection */
                for (i = 0; i <  m->g.no; i++)
					m->q.x[i] = s->d.va[f];		/* Start with average data value */
			} else {
				init_soln(m, s->mgtmps[f][nn-1]);	/* Scale from previous resolution */
				if (doclean || s->inc == 0) {
					free_mgtmp(s->mgtmps[f][nn-1]);	/* Free previous grid res solution */
					s->mgtmps[f][nn-1] = NULL;		/* So we know it's gone */
				}
			}
			solve_gres(m,
#if defined(GRADUATED_TOL)
			              TOL * s->g.res[s->g.brix]/s->ires[nn][s->g.brix],
#else
			              TOL,
#endif
			              s->ires[nn][s->g.brix] >= s->g.res[s->g.brix]);	/* Use itterative */

			if (s->nm)				/* Check and fix nme */
				fix_nme(m, TOL * s->g.res[s->g.brix]/s->ires[nn][s->g.brix],
				                 s->ires[nn][s->g.brix] >= s->g.res[s->g.brix]);

			if (s->xf && nn == (s->niters-1)) {	/* End of non-extra fit itterations */
				/* Determine the fit errors at each data point to pass on */
				/* to the extra fit itterations. */
				comp_extrafit_err(m);
			}
		}

		/* Transfer result in x[] to appropriate grid point value */
		for (gp = s->g.a, i = 0; i < s->g.no; gp += s->g.pss, i++)
			gp[f] = m->q.x[i];

		if (doclean || s->inc == 0) {	/* Not incremental */
			free_mgtmp(s->mgtmps[f][nn-1]);		/* Free final resolution entry */
			s->mgtmps[f][nn-1] = NULL;
		}
	}

	/* We have initialised the grid from scattered data now */
	s->sinit = 1;

	/* Return non-mono check */
	return is_mono(s);
}

/* Initialise the regular spline from scattered data */
/* Return non-zero if non-monotonic */
static int
fit_rspl(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	co *d,			/* Array holding position and function values of data points */
	int dno,		/* Number of data points */
	ratai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	ratai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution */
	ratao vlow,		/* Data value low normalize, NULL = default 0.0 */
	ratao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, nominal = 1.0 */
	double avgdev	/* Average Deviation of input values as proportion of input range. */
) {
	/* Call implementation with (co *) data */
	return fit_rspl_imp(s, flags, (void *)d, 0, dno, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, 1.0, NULL, NULL);
}

/* Initialise the regular spline from scattered data with weights */
/* Return non-zero if non-monotonic */
static int
fit_rspl_w(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	cow *d,			/* Array holding position, function and weight values of data points */
	int dno,		/* Number of data points */
	ratai glow,		/* Grid low scale - will be expanded to enclose data, NULL = default 0.0 */
	ratai ghigh,	/* Grid high scale - will be expanded to enclose data, NULL = default 1.0 */
	int gres[MXDI],		/* Spline grid resolution */
	ratao vlow,		/* Data value low normalize, NULL = default 0.0 */
	ratao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, nominal = 1.0 */
	double avgdev	/* Average Deviation of input values as proportion of input range. */
) {
	/* Call implementation with (cow *) data */
	return fit_rspl_imp(s, flags, (void *)d, 1, dno, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, 1.0, NULL, NULL);
}

/* Initialise from scattered data, with weak default function. */
/* Return non-zero if result is non-monotonic */
static int
fit_rspl_df(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	co *d,			/* Array holding position and function values of data points */
	int dno,		/* Number of data points */
	datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, nominal = 1.0 */
	double avgdev,	/* Average Deviation of input values as proportion of input range. */
	double weak,	/* Weak weighting, nominal = 1.0 */
	void *cbntx,	/* Opaque function context */
	void (*func)(void *cbntx, double *out, double *in)		/* Function to set from */
) {
	/* Call implementation with (co *) data */
	return fit_rspl_imp(s, flags, (void *)d, 0, dno, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, weak, cbntx, func);
}

/* Initialise from scattered data, with per point weighting and weak default function. */
/* Return non-zero if result is non-monotonic */
static int
fit_rspl_w_df(
	rspl *s,	/* this */
	int flags,		/* Combination of flags */
	cow *d,			/* Array holding position, function and weight values of data points */
	int dno,		/* Number of data points */
	datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
	datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
	int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
	datao vlow,		/* Data value low normalize, NULL = default 0.0 */
	datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
	double smooth,	/* Smoothing factor, nominal = 1.0 */
	double avgdev,	/* Average Deviation of input values as proportion of input range. */
	double weak,	/* Weak weighting, nominal = 1.0 */
	void *cbntx,	/* Opaque function context */
	void (*func)(void *cbntx, double *out, double *in)		/* Function to set from */
) {
	/* Call implementation with (cow *) data */
	return fit_rspl_imp(s, flags, (void *)d, 1, dno, glow, ghigh, gres, vlow, vhigh,
	                    smooth, avgdev, weak, cbntx, func);
}

/* Add extra data points and re-fit the rspl */
/* Return non-zero if non-monotonic */
int
add_rspl(
	rspl *s,		/* this */
	int flags,		/* Combination of flags */
	co *d,			/* Array holding position and function values of data points */
	int dno		/* Number of data points */
) {
	/* Call implementation with (co *) data */
	if (s->inc == 0)
		error("rspl: add_rspl called on non-incremental rspl");
	return add_rspl_imp(s, flags, (void *)d, 0, dno);
}

/* Init scattered data elements in rspl */
void
init_data(rspl *s) {
	s->d.no = 0;
	s->d.a = NULL;
	s->fit_rspl      = fit_rspl;
	s->fit_rspl_w    = fit_rspl_w;
	s->fit_rspl_df   = fit_rspl_df;
	s->fit_rspl_w_df = fit_rspl_w_df;
	s->add_rspl = add_rspl;
}

/* Free the scattered data allocation */
void
free_data(rspl *s) {
	int i, f;

	if (s->ires != NULL) {
		free_imatrix(s->ires, 0, s->titers, 0, s->di);
		s->ires = NULL;
	}

	/* Free up mgtmps */
	for (f = 0; f < s->fdi; f++) {
		if (s->mgtmps[f] != NULL) {
			for (i = 0; i < s->titers; i++) {
				if (s->mgtmps[f][i] != NULL) {
					free_mgtmp(s->mgtmps[f][i]);
				}
			}
			free(s->mgtmps[f]);
			s->mgtmps[f] = NULL;
		}
	}

	if (s->d.a != NULL) {	/* Free up the data point data */
		free((void *)s->d.a);
		s->d.a = NULL;
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Non-monotonic reduction (NME) routines */
/* These seem to work in 1D, but aren't effective in higher D */

static double calc_monotonic(mgtmp  *m, int udnmd);
static double fcn(void *fdata, double nmcf);

/* Do one outputs itterations and non-monotonic elimination */
static void
fix_nme(
mgtmp  *m,		/* Multi-grid temporary structure */
double tol,		/* Tollerance parameter for solver */
int final		/* Final flag for solver */
) {
	/* rspl *s = m->s; */
	double x1,x2;
	double new_nmcf;
	
	/* See if it is non-mono */
	if ((x1 = calc_monotonic(m,1)) < NME_TARGET)  {
		return;		/* Not */
	}

	/* Pass some parameters to solver */
	m->nm.tol = tol;
	m->nm.final = final;

	/* Search for 0.0 <= nmcf <= 1.0 that gives us a maximum nme of NME_TARGET */
	/* Find a suitable bracket */
/*	printf("#### top bracked seach\n"); */
	for (x1 = 0.0, x2 = 50.0; x2 < 12800; x2 *= 2.0) {
		if (fcn((void *)m, x2) < 0.0)
			break;			/* Found an x2 */
	}
	if (x2 >= 12800) {
/*		printf("#### zbrac failed\n"); */
	} else {
/*		printf("~~Found bracket %f to %f\n",x1,x2); */

		if (zbrent(&new_nmcf,x1, x2, 0.01, fcn, (void *)m) == 0) {
/*			printf("#### zbrent returned %f\n",new_nmcf); */
			m->nm.cf = new_nmcf;
			fcn((void *)m, new_nmcf);	/* Put value in place */
		} else {
			printf("#### zbrent failed\n");
		}
/*		printf("~~zebrent done\n"); */
	}

	/* Robustness enhancement:
	Check that non-monoticity is cured.

	If not, try again, increasing base cweight ?
	*/

	return;
}


/* Function called by brent to evaluate things current nmcf value */
static double fcn(
void *fdata,	/* Opaque data pointer */
double nmcf		/* Non-mono correction factor to evaluate */
) {
	mgtmp  *m = (mgtmp *) fdata;	/* Data is mgtmp structure */
	rspl *s = m->s;
	int dno = s->d.no;
	int n;

	if (nmcf < 0.0)		/* Protect against sillies */
		nmcf = 0.0;

	/* Set kx values on data points for new nmcf */
	for (n = 0; n < dno; n++) {
		/* Scale extra data point spring factor kx by distance weight */
		/* from non-mono areas (nmw), by overall correction factor (nmcf) */
		m->d[n].kx = 1.0/(1.0 + nmcf * m->d[n].nmw);

#ifdef NEVER
printf("d[%d].kx = %f from nmcf = %f and nmw = %f\n",n,m->d[n].kx,nmcf,m->d[n].nmw);
#endif
	}
		
	/* Do a full itteration */
	setup_solve(m);
	solve_gres(m, m->nm.tol, m->nm.final);	/* Use itterative */

	/* Calculate current non-monoticity error */
	calc_monotonic(m,0);

	/* return the maximum nme factor minus our target value NME_TARGET */
	/* Note that since calc_monotonic returns 1.0 for a grid that is on */
	/* the threshold, and we aim for a worst case NME_TARGET of 0.5, this should */
	/* give the grid a margine above the threshold used by the is_monotonic() */
	/* function. */
/*	printf("~~fcn() called with value %f, returning %f\n",nmcf,m->nm.max - NME_TARGET); */
	return m->nm.max - NME_TARGET;
}

/* Check non-monotonic status of current grid. */

/* Calculate non-monotonicity of the current grid resolution */
/* Return the MCINC normalised maximum nme found. */
/* This means that an nme == MCINC returns 1.0, nme == 2 * MCINC returms 0.0 */
/* nme == 0 returns 4.0 */
static double
calc_monotonic(
mgtmp  *m,		/* Multi-grid temporary structure */
int udnmd		/* non-zero to update data point nme data */
) {
	int e,/* i, */ n;

	rspl *s = m->s;
	int di    = s->di;
	/* int fdi   = s->fdi; */
	/* int gno   = m->g.no; */
	int gres_1[MXDI];
	int *ci   = m->g.ci;	/* Strength reduction */
	double *gp;
	double mcinc = MCINC / (m->g.mres-1);	/* Scaled version of MCINC */
	ECOUNT(gc, di, m->g.res);				/* Grid coordinate counter */
	double min = 1e20;						/* Track min for diagnostics */

	m->nm.max = 0.0;
	for (e = 0; e < di; e++)
		gres_1[e] = m->g.res[e]-1;

	/* Init the data points nme data */
	for (n = 0; n < s->d.no; n++) {
		struct mgdat *ndp = &m->d[n];

		ndp->nme = 0.0;
		if (udnmd)
			ndp->nmd = 1e100;
	}

	/* Check that its monotonic */
	EC_INIT(gc);
	for (gp = m->q.x; !EC_DONE(gc) ; gp++) {
		int e;
		double e1,e2;		/* Smallest/largest surrounting point */
		double u;			/* Current output value we are considering */

		u = *gp;
		/* Find smallest and largest surrounding points */
		/* In +/- 1 dimension directions */
		e1 = 1e20; e2 = -1e20;
		for (e = 0; e < di; e++) {
			int dof;
			float vv;

			if (gc[e] <= 0 || gc[e] >= gres_1[e])
				break;		/* Skip to next grid point if on edge */
			dof = ci[e];
			vv = gp[dof];
			if (vv < e1)	/* update min and max */
				e1 = vv;
			if (vv > e2)
				e2 = vv;
			vv = gp[-dof];
			if (vv < e1)
				e1 = vv;
			if (vv > e2)
				e2 = vv;
		}
		if (e >= di) {	/* Not an edge point */
			double ce;

			e1 = u - e1;
			e2 = e2 - u;
			ce = (e1 < e2 ? e1 : e2);		/* Smallest step */

			if (ce < min)
				min = ce;

			if (ce > (2.0 * mcinc))
				ce = 0.0;
			else {
				ce = ((2.0 * mcinc) - ce)/mcinc;	/* max = 1.0 at ce == mcinc */
				ce *= ce;							/* Square it so max = 4.0 at ce == 0.0 */
			}

#ifdef NEVER
if (ce > 0.000001)
printf("~~~ u[%d].nme = %f\n",gp - m->q.x, ce);
#endif /* NEVER */

			if (ce > m->nm.max)	/* Track maximum */
				m->nm.max = ce;

			if (udnmd && ce > 0.0) {		/* If we should upate data points */
				double gg[MXRI];			/* Actual grid coordinate */
				for (e = 0; e < di; e++)	/* Compute it */
					gg[e] = m->g.l[e] + m->g.w[e] * gc[e];

				/* See if this is the closest non-mono to each data point */
				/* (Could this be made more efficient ???? ~~~) */
				for (n = 0; n < s->d.no; n++) {
					double dst = 0.0;
					struct mgdat *ndp = &m->d[n];		/* data point nme stuff */
					rpnts *dp  = &s->d.a[n]; 			/* data point stuff */

					if (dp->k == 0.0)		/* Skip data points that can't contribute */
						continue;

					for (e = 0; e < di; e++) {	/* Compute squared distance */
						double aa = gg[e] - dp->p[e];
						dst += aa * aa;
					}
					if (dst < ndp->nmd) {
						ndp->nmd = dst;	/* New closest nme */
						ndp->nme = ce;
#ifdef NEVER
printf("New dist %f at d[%d]\n",dst,n);
#endif
					}
				}
			}
		}
		/* Increment grid index */
		EC_INC(gc);
	}

	/* Convert distance squared to weighting factor */
	if (udnmd && m->nm.max > 0.0) {
		for (n = 0; n < s->d.no; n++) {		/* Go through all the data points */
			/* int j; */
			struct mgdat *ndp = &m->d[n];
			ndp->nmw = (RADF * RADF)/(RADF * RADF + ndp->nmd);
		}
	}
/* printf("~1 returning nme of %f, min of %e\n",m->nm.max, min); */
	return m->nm.max;
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* In theory, the smoothness should increase proportional to the square of the */
/* overall average sample deviation. (Or the weight of each individual data point */
/* could be made inversely proportional to the square of its average sample */
/* deviation, or square of its standard deviation, or its variance, etc.) */
/* In practice, other factors also seem to come into play, so we use a */
/* table to lookup an "optimal" smoothing factor for each combination */
/* of the parameters dimension, sample count and average sample deviation. */
/* The contents of the table were created by simulating random device characteristics, */
/* and locating the optimal smoothing factor for each parameter. The behaviour */
/* was then cross validated with real device test sets. */
/* If the instrument variance is assumed to be a constant factor */
/* in the sensors, then it would be appropriate to modify the */
/* data weighting rather than the overall smoothness, */
/* since a constant XYZ variance could be transformed into a */
/* per data point L*a*b* variance. */
/* The optimal smoothness factor doesn't appear to have any significant */
/* dependence on the RSPL resolution. */

/* Return an appropriate smoothing factor for the combination of final parameters. */
/* The "Average sample deviation" is a measure of its randomness. */
/* For instance, values that had a +/- 0.1 uniform random error added */
/* to them, would have an average sample deviation of 0.05. */
/* For normally distributed errors, the average deviation is */
/* aproximately 0.564 times the standard deviation. (0.564 * sqrt(variance)) */
/* SMOOTH */
static double opt_smooth(
	int di,		/* Dimensions */
	int ndp,	/* Number of data points */
	double ad	/* Average sample deviation (proportion of input range) */
) {
	double nc;		/* Normalised sample count */
	int nncixv[4] = { 6, 6, 10, 11 };		/* Number in ncixv rows */
	double ncixv[4][11] = {				/* nc to smf index */
	   { 5.0, 10.0, 20.0, 50.0, 100.0, 200.0 },
	   { 5.0, 10.0, 20.0, 50.0, 100.0, 200.0 },
	   { 2.92, 3.68, 4.22,  5.0, 6.3, 7.94, 10.0, 12.6,  20.0, 50.0 },
	   { 2.66, 3.16, 3.76, 4.61, 5.0, 5.48,  6.51, 7.75, 10.0, 20.0, 31.62 }
	};
	int ncixN;
	int ncix;		/* Normalised sample count index */
	double ncw;		/* Weight of [ncix], 1-weight of [ncix+1] */ 

	double adixv[6] = { 0.0001, 0.0025, 0.005, 0.0125, 0.025, 0.05 };
					/* ad to smf index */
	int adix;		/* Average deviation count index */
	double adw;		/* Weight of [adix], 1-weight of [adix+1] */ 
	
	double lsm, sm;

	/* Main lookup table, by [di][ncix][adix]: */
	/* Values are log of smoothness value. */
	/* Derived from simulations of synthetic functions */
	double smf[4][11][6] = {
		/* 1D: */
		{
/* -r value:   0     0.25% 0.5%  1.25% 2.5%  5%	  */		
/* Total white 0%    1%    2%    5%    10%   20% */
/* 5 */		{ -3.0, -3.0, -3.0, -3.0, -2.7, -2.5 },
/* 10 */	{ -4.5, -4.5, -4.2, -3.1, -4.0, -3.5 },
/* 20 */	{ -6.5, -6.5, -5.7, -4.7, -4.0, -3.5 },
/* 50 */	{ -6.5, -6.5, -6.0, -5.0, -4.5, -3.5 },
/* 100 */	{ -6.5, -6.5, -5.5, -4.5, -4.0, -3.5 },
/* 200 */	{ -6.5, -5.4, -5.0, -4.2, -4.0, -3.5 }
		},
		/* 2D: */
		{
			/* 0%    1%    2%    5%    10%   20% */
/* 5 */		{ -5.7, -4.7, -4.9, -3.2, -2.4, -2.2 },
/* 10 */	{ -5.7, -4.7, -4.0, -3.2, -2.5, -2.0 },
/* 20 */	{ -5.7, -4.0, -3.6, -3.2, -2.0, -2.0 },
/* 50 */	{ -6.2, -4.0, -3.6, -3.0, -2.0, -2.0 },
/* 100 */	{ -6.2, -4.0, -3.6, -3.0, -2.0, -2.0 },
/* 200 */	{ -6.2, -4.0, -3.6, -3.0, -2.0, -2.0 }
		},
		/* 3D: */
		{
			/* 0%    1%    2%    5%    10%   20% */
/* 2.92 */	{ -3.5, -3.5, -3.5, -3.0, -2.5, -1.5 },
/* 3.68 */	{ -3.5, -3.5, -3.5, -3.0, -2.0, -1.0 },
/* 4.22 */	{ -3.5, -4.0, -3.5, -2.7, -2.0, -1.0 },
/* 5.00 */	{ -4.0, -4.2, -3.7, -2.5, -2.0, -1.0 },
/* 6.30 */	{ -4.0, -4.2, -4.0, -2.2, -2.0, -1.0 },
/* 7.94 */	{ -4.1, -4.2, -4.0, -2.2, -2.0, -1.0 },
/* 10.0 */	{ -4.2, -4.2, -4.0, -2.1, -2.0, -1.0 },
/* 12.6 */	{ -4.4, -4.2, -4.0, -2.0, -2.0, -1.0 },
/* 20.0 */	{ -4.4, -4.1, -3.5, -1.7, -1.5, -1.0 },
/* 50.0 */	{ -4.4, -2.5, -2.0, -1.0, -1.0, -1.0 }
		},
		/* 4D: */
		{
			/* 0%    1%    2%    5%    10%   20% */
/* 2.66 */	{ -2.5, -2.5, -2.5, -3.0, -3.0, -2.0 },
/* 3.16 */	{ -3.5, -3.5, -3.5, -3.0, -3.0, -2.0 },
/* 3.76 */	{ -3.5, -3.5, -3.2, -3.5, -2.5, -2.0 },
/* 4.61 */	{ -3.0, -3.5, -3.0, -3.0, -2.0, -1.0 },
/* 5.00 */	{ -3.0, -3.5, -3.0, -2.5, -1.0, -1.0 },
/* 5.48 */	{ -3.0, -3.5, -3.0, -2.0, -1.0, -1.0 },
/* 6.51 */	{ -3.0, -3.5, -3.0, -2.0, -1.0, -1.0 },
/* 7.75 */	{ -3.0, -3.4, -3.0, -2.0, -1.0, -1.0 },
/* 10.00 */	{ -3.0, -3.0, -2.5, -1.0, -1.0, -1.0 },
/* 20.00 */	{ -2.5, -2.0, -1.0, -1.0, -1.0, -1.0 },
/* 31.62 */	{ -2.0, -1.0, -1.0, -1.0, -1.0, -1.0 }
		}
	};

	/* Real world correction factors - */
	/* correct for real world device smoothnesses. */
	double rwf[4] = { 1.0, 1.0, 1.0, 1.0 };

/* printf("~1 opt_smooth called with di = %d, ndp = %d, ad = %f\n",di,ndp,ad); */
	if (di < 1)
		di = 1;
	nc = pow((double)ndp, 1.0/(double)di);		/* Normalised sample count */
	if (di > 4)
		di = 4;
	di--;			/* Make di 0..3 */

	/* Convert the two input parameters into appropriate */
	/* indexes and weights for interpolation. We assume ratiometric scaling. */

	ncixN = nncixv[di];
	if (nc <= ncixv[di][0]) {
		ncix = 0;
		ncw = 1.0;
	} else if (nc >= ncixv[di][ncixN-1]) {
		ncix = ncixN-2;
		ncw = 0.0;
	} else {
		for (ncix = 0; ncix < ncixN; ncix++) {
			if (nc >= ncixv[di][ncix] && nc <= ncixv[di][ncix+1])
				break;
			
		}
		ncw = 1.0 - (log(nc) - log(ncixv[di][ncix]))
		           /(log(ncixv[di][ncix+1]) - log(ncixv[di][ncix]));
	}

	if (ad <= adixv[0]) {
		adix = 0;
		adw = 1.0;
	} else if (ad >= adixv[5]) {
		adix = 4;
		adw = 0.0;
	} else {
		for (adix = 0; adix < 5; adix++) {
			if (ad >= adixv[adix] && ad <= adixv[adix+1])
				break;
		}
		adw = 1.0 - (log(ad) - log(adixv[adix]))/(log(adixv[adix+1]) - log(adixv[adix]));
	}

	/* Lookup & interpolate the log smoothness factor */
	lsm  = smf[di][ncix][adix]    * ncw          * adw;
	lsm += smf[di][ncix][adix+1]  * ncw          * (1.0 - adw);
	lsm += smf[di][ncix+1][adix]  * (1.0 - ncw)  * adw;
	lsm += smf[di][ncix+1][adix+1] * (1.0 - ncw) * (1.0 - adw);

	sm = pow(10.0, lsm);

	/* and correct for the real world */
	sm *= rwf[di];

/* printf("Got log smth %f, returning %1.9f from ncix %d, ncw %f, adix %d, adw %f\n",
lsm, sm, ncix, ncw, adix, adw); */
	return sm;
}
/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Multi-grid temp structure (mgtmp) routines */

/* Create a new mgtmp. */
/* Solution matricies will be NULL */
static mgtmp *new_mgtmp(
	rspl *s,		/* associated rspl */
	int gres[MXDI],	/* resolution to create */
	int f			/* output dimension */
) {
	mgtmp *m;
	int di = s->di; /*, fdi = s->fdi; */
	int dno = s->d.no;
	int gno, nigc;
	int gres_1[MXDI];
	int e, g, n, i; /*, j , k; */

	/* Allocate a structure */
	if ((m = (mgtmp *) calloc(1, sizeof(mgtmp))) == NULL)
		error("rspl: malloc failed - mgtmp");

	/* General stuff */
	m->s = s;
	m->f = f;

	/* Grid related */
	for (gno = 1, e = 0; e < di; gno *= gres[e], e++)
		;
	m->g.no = gno;

	/* record high, low limits, and width of each grid cell */
	m->g.mres = 1.0;
	m->g.bres = 0;
	for (e = 0; e < s->di; e++) {
		m->g.res[e] = gres[e];
		gres_1[e] = gres[e] - 1;
		m->g.mres *= gres[e];
		if (gres[e] > m->g.bres) {
			m->g.bres = gres[e];
			m->g.brix = e;
		}
		m->g.l[e] = s->g.l[e];
		m->g.h[e] = s->g.h[e];
		m->g.w[e] = (s->g.h[e] - s->g.l[e])/(double)(gres[e]-1);
	}
	m->g.mres = pow(m->g.mres, 1.0/e);		/* geometric mean */

	/* Compute index coordinate increments into linear grid for each dimension */
	/* ie. 1, gres, gres^2, gres^3 */
	for (m->g.ci[0] = 1, e = 1; e < di; e++)
		m->g.ci[e]  = m->g.ci[e-1] * gres[e-1];		/* In grid points */

	/* Compute index offsets from base of cube to other corners */
	for (m->g.hi[0] = 0, e = 0, g = 1; e < di; g *= 2, e++) {
		for (i = 0; i < g; i++)
			m->g.hi[g+i] = m->g.hi[i] + m->g.ci[e];		/* In grid points */
	}

	/* Number grid cells that contribute to smoothness error */
	for (nigc = 1, e = 0; e < di; e++) {
		nigc *= gres[e]-2;
	}

	/* Compute curvature weighting for matching intermediate resolutions for */
	/* the number of grid points curvature that is accuumulated, as well as the */
	/* geometric effects of a finer fit to the target surface. */
	/* This is all to keep the ratio of sum of smoothness error squared */
	/* constant in relationship to the sum of data point error squared. */
	for (e = 0; e < di; e++) {
		double rsm;				/* Resolution smoothness factor */
		double smooth;

		if (s->symdom)
			rsm = m->g.res[e];	/* Relative final grid size  */
		else
			rsm = m->g.mres;	/* Relative mean final grid size */

		/* Compensate for geometric and grid numeric factors */
		rsm = pow((rsm-1.0), 4.0);	/* Geometric resolution factor for smooth surfaces */
		rsm /= nigc;				/* Average squared non-smoothness */

		if (s->smooth >= 0.0) {
			/* Table lookup for optimum smoothing factor */
			smooth = opt_smooth(di, s->d.no, s->avgdev);
			m->sf.cw[e] = s->smooth * smooth * rsm;

		} else {	/* Special used to calibrate table */
			m->sf.cw[e] = -s->smooth * rsm;
		}
	}

	/* Compute weighting for weak default function grid value */
	/* We are trying to keep the effect of the wdf constant with */
	/* changes in grid resolution and dimensionality. */
	m->wdfw = s->weak * WEAKW/(m->g.no * (double)di);

	/* Allocate space for auiliary data point related info */
	if ((m->d = (struct mgdat *) calloc(dno, sizeof(struct mgdat))) == NULL)
		error("rspl: malloc failed - mgtmp");

	/* fill in the aux data point info */
	for (n = 0; n < dno; n++) {
		double we[MXRI];	/* 1.0 - Weight in each dimension */
		int ix = 0;			/* Index to base corner of surrounding cube in grid points */

		/* Figure out which grid cell the point falls into */
		for (e = 0; e < di; e++) {
			double t;
			int mi;
			if (s->d.a[n].p[e] < m->g.l[e] || s->d.a[n].p[e] > m->g.h[e]) {
				error("rspl: Data point %d outside grid %e <= %e <= %e",
				                            n,m->g.l[e],s->d.a[n].p[e],m->g.h[e]);
			}
			t = (s->d.a[n].p[e] - m->g.l[e])/m->g.w[e];
			mi = (int)floor(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi >= gres_1[e])	/* Make sure outer point can't be base */
				mi = gres_1[e]-1;
			ix += mi * m->g.ci[e];		/* Add Index offset for grid cube base in dimen */
			we[e] = t - (double)mi;		/* 1.0 - weight */
		}
		m->d[n].b = ix;

		/* Compute corner weights needed for interpolation */
		m->d[n].w[0] = 1.0;
		for (e = 0, g = 1; e < di; g *= 2, e++) {
			for (i = 0; i < g; i++) {
				m->d[n].w[g+i] = m->d[n].w[i] * we[e];
				m->d[n].w[i] *= (1.0 - we[e]);
			}
		}
#ifdef DEBUG
		printf("Data point %d weighting factors = \n",n);
		for (e = 0; e < (1 << di); e++) {
			printf("%d: %f\n",e,m->d[n].w[e]);
		}
#endif /* DEBUG */

		/* NME related stuff */
		m->d[n].kx = 1.0;		/* Non-mono correction */
		m->d[n].nme = 0.0;		/* Non-mono error */
		m->d[n].nmd = 0.0;		/* Non-mono distance */
		m->d[n].nmw = 0.0;		/* Non-mono weight */

		/* Extra fit related stuff */
		m->d[n].ierr = 0.0;		/* Inital data point error */
	}
	m->aierr = 0.0;		/* Average inital data point error */

	/* Set the solution matricies to unalocated */
	m->q.A = NULL;
	m->q.ixcol = NULL;
	m->q.b = NULL;
	m->q.x = NULL;

	/* Global nme stuff */
	m->nm.cf = 0.0;		/* Non-mono correction factor */
	m->nm.max = 0.0;	/* Maximum nme discoverd in check_monotonic */

	return m;
}

/* re-initialise a mgtmp just to account for added data points */
static void reinit_mgtmp(
mgtmp *m
) {
	rspl *s = m->s;
	int di = s->di; /* , fdi = s->fdi; */
	int dno = s->d.no;
	int gres_1[MXDI];
	int e, g, n, i; /* , j, k; */

	/* Reallocate space for auiliary data point related info */
	if ((m->d = (struct mgdat *) realloc(m->d, dno * sizeof(struct mgdat))) == NULL)
		error("rspl: malloc failed - mgtmp");

	for (e = 0; e < di; e++)
		gres_1[e] = m->g.res[e]-1;

	/* fill in the aux data info for the new points */
	for (n = dno - s->d.ano; n < dno; n++) {
		double we[MXRI];	/* 1.0 - Weight in each dimension */
		int ix = 0;			/* Index to base corner of surrounding cube in grid points */

		/* Figure out which grid cell the point falls into */
		for (e = 0; e < di; e++) {
			double t;
			int mi;
			if (s->d.a[n].p[e] < m->g.l[e] || s->d.a[n].p[e] > m->g.h[e])
				error("rspl: Added data point outside grid %e <= %e <= %e",
				                            m->g.l[e],s->d.a[n].p[e],m->g.h[e]);
			t = (s->d.a[n].p[e] - m->g.l[e])/m->g.w[e];
			mi = (int)floor(t);			/* Grid coordinate */
			if (mi < 0)					/* Limit to valid cube base index range */
				mi = 0;
			else if (mi >= gres_1[e])		/* Make sure outer point can't be base */
				mi = gres_1[e]-1;
			ix += mi * m->g.ci[e];		/* Add Index offset for grid cube base in dimen */
			we[e] = t - (double)mi;		/* 1.0 - weight */
		}
		m->d[n].b = ix;

		/* Compute corner weights needed for interpolation */
		m->d[n].w[0] = 1.0;
		for (e = 0, g = 1; e < di; g *= 2, e++) {
			for (i = 0; i < g; i++) {
				m->d[n].w[g+i] = m->d[n].w[i] * we[e];
				m->d[n].w[i] *= (1.0 - we[e]);
			}
		}
#ifdef DEBUG
		printf("Data point %d weighting factors = \n",n);
		for (e = 0; e < (1 << di); e++) {
			printf("%d: %f\n",e,m->d[n].w[e]);
		}
#endif /* DEBUG */

		/* NME related stuff */
		m->d[n].kx = 1.0;		/* Non-mono correction */
		m->d[n].nme = 0.0;		/* Non-mono error */
		m->d[n].nmd = 0.0;		/* Non-mono distance */
		m->d[n].nmw = 0.0;		/* Non-mono weight */

		/* Extra fit related stuff */
		m->d[n].ierr = 0.0;		/* Inital data point error */
	}
}

/* Completely free an mgtmp */
static void free_mgtmp(mgtmp  *m) {
	int gno = m->g.no;

	free_dvector(m->q.x,0,gno-1);
	free_dvector(m->q.b,0,gno-1);
	free((void *)m->q.ixcol);
	free_dmatrix(m->q.A,0,gno-1,0,m->q.acols-1);
	free((void *)m->d);
	free((void *)m);
}

static double comp_extrafit_weight(mgtmp *m, int i);

/* Initialise the A[][] and b[] matricies ready to solve, given f */
/* (Can be used to re-initialize an mgtmp for changing curve/nme/extra fit factors) */
static void setup_solve(
	mgtmp  *m		/* initialized grid temp structure */
) {
	rspl *s = m->s;
	int di   = s->di; /* ,     fdi = s->fdi; */
	int gno  = m->g.no,   dno = s->d.no;
	int *gres = m->g.res, *gci = m->g.ci;
	int f = m->f;				/* Output dimensions being worked on */

	double **A = m->q.A;		/* A matrix of interpoint weights */
	int acols  = m->q.acols;	/* A matrix packed columns needed */
	int *xcol  = m->q.xcol;		/* A array column translation from packed to sparse index */ 
	int *ixcol = m->q.ixcol;	/* A array column translation from sparse to packed index */ 
	double *b  = m->q.b;		/* b vector for RHS of simultabeous equation */
	double *x  = m->q.x;		/* x vector for LHS of simultabeous equation */
	int e, n,i,k;
	double nbsum;				/* normb sum */

	/* Allocate and init the A array column sparse packing lookup and inverse */
	/* Note that this only works for a minumum grid resolution of 4 */
	if (A == NULL) {			/* Not been allocated previously */
		DCOUNT(gc, di, -2, -2, 3);	/* 0..2 Surounder */
		int ix;						/* Grid point offset in grid points */
		acols = 0;
	
		DC_INIT(gc);
	
		while (!DC_DONE(gc)) {
			int n2 = 0, nz = 0;
	
			/* Detect 2 and 0 elements */
			for (k = 0; k < di; k++) {
				if (gc[k] == 2 || gc[k] == -2)
					n2++;
				if (gc[k] == 0)
					nz++;
			}

			/* Accept only if doesn't have a +/-2, */
			/* or if it has exactly one +/-2 and otherwise 0 */
			if (n2 == 0 || (n2 == 1 && nz == (di-1))) {
				for (ix = 0, k = 0; k < di; k++)
					ix += gc[k] * gci[k];
				if (ix >= 0) {
					xcol[acols++] = ix;
				}
			}
	
			DC_INC(gc);
		}

		ix = xcol[acols-1] + 1;	/* Number of expanded rows */

		/* Create inverse lookup */
		if (ixcol == NULL) {
			if ((ixcol = (int *) malloc(sizeof(int) * ix)) == NULL)
				error("rspl malloc failed - ixcol");
		}

		for (k = 0; k < acols; k++)
			ixcol[xcol[k]] = k;		/* Set inverse lookup */

#ifdef DEBUG
		printf("Sparse array expansion = \n");
		for (k = 0; k < acols; k++) {
			printf("%d: %d\n",k,xcol[k]);
		}
		printf("\nSparse array encoding = \n");
		for (k = 0; k < ix; k++) {
			printf("%d: %d\n",k,ixcol[k]);
		}
#endif /* DEBUG */

		/* We store the packed diagonals of the sparse A matrix */
		/* If re-initializing, zero matrices, else allocate zero'd matricies */
		if ((A = dmatrixz(0,gno-1,0,acols-1)) == NULL) {
			error("Malloc of A[][] failed with [%d][%d]",gno,acols);
		}
		if ((b = dvectorz(0,gno)) == NULL) {
			free_dmatrix(A,0,gno-1,0,acols-1);
			error("Malloc of b[] failed");
		}
		if ((x = dvector(0,gno-1)) == NULL) {
			free_dmatrix(A,0,gno-1,0,acols-1);
			free_dvector(b,0,gno-1);
			error("Malloc of x[] failed");
		}

		/* Stash in the mgtmp */
		m->q.A = A;
		m->q.b = b;
		m->q.x = x;
		m->q.acols = acols;
		m->q.ixcol = ixcol;
	} else if (m->i2 == 0) {		/* Not incremental 2nd round update */
		/* If re-initializing, zero matrices */
		for (i = 0; i < gno; i++)
			for (k = 0; k < acols; k++) {
				A[i][k] = 0.0;
			}
		for (i = 0; i < gno; i++)
			b[i] = 0.0;
	}

#ifdef ALWAYS
	/* Accumulate curvature dependent factors */
	if (m->i2 == 0) {		/* If setting this up from scratch */
		ECOUNT(gc, di, gres);
		EC_INIT(gc);
		for (i = 0; i < gno; i++) {

			for (e = 0; e < di; e++) {
				double cw = 2.0 * m->sf.cw[e];	/* Curvature weight */
				cw *= s->d.vw[f];				/* Scale curvature weight for data range */

				/* If at least two above lower edge in this dimension */
				/* Add influence on Curvature of cell below */
				if ((gc[e]-2) >= 0) {
					/* double kw = cw * gp[UO_C(e,1)].k; */	/* Cell bellow k value */
					double kw = cw;

					A[i][ixcol[0]] += kw;
				}
				/* If not one from upper or lower edge in this dimension */
				/* Add influence on Curvature of cell below */
				if ((gc[e]-1) >= 0 && (gc[e]+1) < gres[e]) {
					/* double kw = cw * gp->k;  */
					double kw = cw; 

					A[i][ixcol[0]]      += 4.0 * kw;
					A[i][ixcol[gci[e]]] += -2.0 * kw;
				}
				/* If at least two below the upper edge in this dimension */
				/* Add influence on Curvature of cell above */
				if ((gc[e]+2) < gres[e]) {
					/* double kw = cw * gp[UO_C(e,2)].k;	*/ /* Cell above k value */
					double kw = cw;

					A[i][ixcol[0]]          += kw;
					A[i][ixcol[gci[e]]]     += -2.0 * kw;
					A[i][ixcol[2 * gci[e]]] += kw;
				}
			}
			EC_INC(gc);
		}
	}
#endif /* ALWAYS */

#ifdef DEBUG
	printf("\n");
	for (i = 0; i < gno; i++) {
		printf("b[%d] = %f\n",i,b[i]);
		for (k = 0; k < acols; k++) {
			printf("A[%d][%d] = %f\n",i,k,A[i][k]);
		}
		printf("\n");
	}
#endif /* DEBUG */

	nbsum = 0.0;	/* Zero sum of b[] squared */

#ifdef ALWAYS
	/* Accumulate weak default function factors. These are effectively a */
	/* weak "data point" exactly at each grid point. */
	/* (Note we're not currently doing this in a cache friendly order,   */
	/*  and we're calling the function once for each output component..) */
	if (m->i2 == 0 && s->dfunc != NULL) {		/* If setting this up from scratch */
		double iv[MXDI], ov[MXDO];
		ECOUNT(gc, di, gres);
		EC_INIT(gc);
		for (i = 0; i < gno; i++) {
			double d, tt;

			/* Get weak default function value for this grid point */
			for (e = 0; e < s->di; e++)
				iv[e] = m->g.l[e] + gc[e] * m->g.w[e];	/* Input sample values */
			s->dfunc(s->dfctx, ov, iv);

			/* Compute values added to matrix */
			d = 2.0 * m->wdfw;
			tt = d * ov[f];			/* Change in data component */
			nbsum += (2.0 * b[i] + tt) * tt;	/* += (b[i] + tt)^2 - b[i]^2 */
			b[i] += tt;				/* New data component value */
			A[i][0] += d;			/* dui component to itself */

			EC_INC(gc);
		}
	}
#endif /* ALWAYS */

#ifdef DEBUG
	printf("\n");
	for (i = 0; i < gno; i++) {
		printf("b[%d] = %f\n",i,b[i]);
		for (k = 0; k < acols; k++) {
			printf("A[%d][%d] = %f\n",i,k,A[i][k]);
		}
		printf("\n");
	}
#endif /* DEBUG */

#ifdef ALWAYS
	/* Accumulate data point dependent factors */
	for (n = 0; n < dno; n++) {	/* Go through all the data points */
		int j,k;
		int bp = m->d[n].b; 		/* index to base grid point in grid points */
		double kx;					/* Non-monotonicity fix reducing weight */
		double xfw = 1.0;			/* Extra fitting weight */

		if (m->xfe)
			xfw = comp_extrafit_weight(m, n);	/* Compute extra weight for this data point */

		kx = m->d[n].kx;

		/* For each point in the cube as the base grid point, */
		/* add in the appropriate weighting for its weighted neighbors. */
		for (j = 0; j < (1 << di); j++) {	/* Binary sequence */
			double d, w, tt;
			int ai;

			w = m->d[n].w[j];				/* Base point weight */
			d = 2.0 * s->d.a[n].k * xfw * kx * w;
			ai = bp + m->g.hi[j];			/* A matrix index */

			tt = d * s->d.a[n].v[f];	/* Change in data component */

			/* If setting all data point factors for the first time, */
			/* or just adding additional data points: */
			if (m->i2 == 0 || n >= (dno - s->d.ano)) {
				nbsum += (2.0 * b[ai] + tt) * tt;	/* += (b[ai] + tt)^2 - b[ai]^2 */
				b[ai] += tt;					/* New data component value */
				A[ai][0] += d * w;				/* dui component to itself */
	
				/* For all the other simplex points ahead of this one, */
				/* add in linear interpolation weightings */
				for (k = j+1; k < (1 << di); k++) {	/* Binary sequence */
					int ii;
					ii = ixcol[m->g.hi[k] - m->g.hi[j]];	/* A matrix column index */
					A[ai][ii] += d * m->d[n].w[k];			/* dui component due to ui+1 */
				}
			}
		}
	}

	/* Compute norm of b[] from sum of squares */
	nbsum = sqrt(nbsum);
	if (nbsum < 1e-4) 
		nbsum = 1e-4;
	m->q.normb = nbsum;

#endif /* ALWAYS */ 

#ifdef DEBUG
	printf("\n");
	for (i = 0; i < gno; i++) {
		printf("b[%d] = %f\n",i,b[i]);
		for (k = 0; k < acols; k++) {
			printf("A[%d][%d] = %f\n",i,k,A[i][k]);
		}
		printf("\n");
	}
#endif /* DEBUG */
}

/* Given that we've done a complete fit up to the final resolution, */
/* compute the error for each data point */
static void comp_extrafit_err(
	mgtmp *m		/* Final resolution mgtmp */
) {
	rspl *s = m->s;
	int n;
	int dno = s->d.no;
	int di = s->di;
	double *x = m->q.x;		/* Grid solution values */
	int f = m->f;			/* Output dimensions being worked on */
#ifdef NEVER
	double mine = 1e6, maxe = -1e6, aerr = 0.0;
#endif

	/* Compute error for each data point */
	/* (Should data point k factor be taken into account ? */
	m->aierr = 0.0;
	for (n = 0; n < dno; n++) {
		int j;
		int bp = m->d[n].b; 		/* index to base grid point in grid points */
		double val = 0.0;			/* Current interpolated value */
		double err;

		/* Compute the interpolated grid value for this data point */
		for (j = 0; j < (1 << di); j++) {	/* Binary sequence */
			val += m->d[n].w[j] * x[bp + m->g.hi[j]];
		}

		err = (s->d.a[n].v[f] - val)/s->d.vw[f];		/* Normalised error */

		m->d[n].ierr = err;
		m->aierr += fabs(m->d[n].ierr);

#ifdef NEVER
		printf("~1 Data point %d err = %f, ierr = %f\n",n,err,m->d[n].ierr);
		if (err < mine)
			mine = err;
		if (err > maxe)
			maxe = err;
		aerr = fabs(err);
#endif
	}

	m->aierr /= (double)dno;
	if (m->aierr < 0.0001)
		m->aierr = 0.0001;		/* Prevent silliness */

#ifdef NEVER
	printf("~1 This pass extra fit 0x%x avg init abs err = %f\n",p,m->aierr);
	printf("~1 This pass extra fit avg abs err = %f, min = %f, max = %f\n",aerr,mine,maxe);
#endif
}

/* Given that extra fitting is being done, compute the anount to increase the */
/* data point spring factor, to try and improve fit near poorly fitted data points. */
static double comp_extrafit_weight(
	mgtmp *m,		/* Final temporary grid structure */
	int dix			/* Data intex */
) {
	/* int e, n;
	rspl *s = m->s;
	int di  = s->di;
	int dno = s->d.no; 
	double werr = 0.0, wden = 0.0; */	/* Weighted error and weight denominator */
	double xwt;					/* De weighting */

	if (fabs(m->d[dix].ierr) > m->aierr) {
		xwt = (fabs(m->d[dix].ierr))/m->aierr;
		if (xwt > 50.0)
			xwt = 50.0;		/* Prevent silliness */
	} else
		xwt = 1.0;

#ifdef NEVER
printf("~1 data point %d returned extra weighting %f\n",dix,xwt);
#endif

	return xwt;
}

/* Transfer a solution from one mgtmp to another */
/* (We assume that they are for the same problem) */
static void init_soln(
	mgtmp  *m1,		/* Destination */
	mgtmp  *m2		/* Source */
) {
	rspl *s = m1->s;
	int di  = s->di;
	/* int fdi = s->fdi; */
	int gno = m1->g.no;
	int dno = s->d.no;
	int gres1_1[MXDI];
	int gres2_1[MXDI];
	int e, n;
	ECOUNT(gc, di, m1->g.res);	/* Counter for output points */

	for (e = 0; e < di; e++) {
		gres1_1[e] = m1->g.res[e]-1;
		gres2_1[e] = m2->g.res[e]-1;
	}

	/* For all output grid points */
	EC_INIT(gc);
	for (n = 0; n < gno; n++) {
		double we[MXRI];		/* 1.0 - Weight in each dimension */
		double gw[POW2MXRI];	/* weight for each grid cube corner */
		double *gp;				/* Pointer to x2[] grid cube base */
		
		/* Figure out which grid cell the point falls into */
		{
			double t;
			int mi;
			gp = m2->q.x;					/* Base of solution array */
			for (e = 0; e < di; e++) {
				t = ((double)gc[e]/(double)gres1_1[e]) * (double)gres2_1[e];
				mi = (int)floor(t);			/* Grid coordinate */
				if (mi < 0)					/* Limit to valid cube base index range */
					mi = 0;
				else if (mi >= gres2_1[e])
					mi = gres2_1[e]-1;
				gp += mi * m2->g.ci[e];		/* Add Index offset for grid cube base in dimen */
				we[e] = t - (double)mi;		/* 1.0 - weight */
			}
		}

		/* Compute corner weights needed for interpolation */
		{
			int i, g;
			gw[0] = 1.0;
			for (e = 0, g = 1; e < di; g *= 2, e++) {
				for (i = 0; i < g; i++) {
					gw[g+i] = gw[i] * we[e];
					gw[i] *= (1.0 - we[e]);
				}
			}
		}

		/* Compute the output values */
		{
			int i;
			m1->q.x[n] = 0.0;					/* Zero output value */
			for (i = 0; i < (1 << di); i++) {	/* For all corners of cube */
				m1->q.x[n] += gw[i] * gp[m2->g.hi[i]];
			}
		}
		EC_INC(gc);
	}

	/* Transfer other stuff from one resolution to the next */
	m1->nm.cf = m2->nm.cf;	/* Non-mono correction factor */

	/* Transfer per data point extra fit information */
	for (n = 0; n < dno; n++) {
		m1->d[n].ierr = m2->d[n].ierr;
	}
	m1->aierr = m2->aierr;
}

/* - - - - - - - - - - - - - - - - - - - -*/
static double one_itter1(double **A, double *x, double *b, double normb, int gno, int acols,
                 int *xcol, int di, int *gres, int *gci, int max_it, double tol);
static void one_itter2(double **A, double *x, double *b, int gno, int acols, int *xcol,
                 int di, int *gres, int *gci, double ovsh);
static double soln_err(double **A, double *x, double *b, double normb, int gno, int acols, int *xcol);
static double cj_line(double **A, double *x, double *b, int gno, int acols, int *xcol,
               int sof, int nid, int inc, int max_it, double tol);

/* Solve scattered data to grid point fit */
static void
solve_gres(mgtmp *m, double tol, int final)
{
	rspl *s = m->s;
	int di = s->di; /*, fdi = s->fdi, dno = s->d.no; */
	int gno = m->g.no, *gres = m->g.res, *gci = m->g.ci;
	int i; /*e, f, n, k; */
	double **A = m->q.A;		/* A matrix of interpoint weights */
	int acols  = m->q.acols;	/* A matrix columns needed */
	int *xcol  = m->q.xcol;		/* A array column translation from packed to sparse index */ 
	/* int *ixcol = m->q.ixcol; */	/* A array column translation from sparse to packed index */ 
	double *b  = m->q.b;		/* b vector for RHS of simultabeous equation */
	double *x  = m->q.x;		/* x vector for result */
    int ni;                     /* number of iterations */

	/*
	 * The regular spline fitting problem to be solved here strongly
	 * resembles those involved in solving partial differential equation
	 * problems. The scattered data points equate to boundary conditions,
	 * while the smoothness criteria equate to partial differential equations.
	 */

	/*
	 * There are many approaches that can be used to solve the
	 * symetric positive-definite system Ax = b, where A is a
	 * sparse diagonal matrix with fringes. A direct method
	 * would be Cholesky decomposition, and this works well for
	 * the 1D case (no fringes), but for more than 1D, it generates
	 * fill-ins between the fringes. Given that the widest spaced
	 * fringes are at 2 * gres ^ (dim-1) spacing, this leads
	 * to an unacceptable storage requirement for A, at the resolutions
	 * and dimensions needed in color correction.
	 *
	 * The approaches that minimise A storage are itterative schemes,
	 * such as Gauss-Seidel relaxation, or conjugate-gradient methods.
	 * 
     * There are two methods allowed for below, depending on the
	 * value of JITTERS.
     * If JITTERS is non-zero, then there will be JITTERS passes of
	 * a combination of multi-grid, Gauss-Seidel relaxation,
	 * and conjugate gradient.
	 *
	 * The outermost loop will use a series of grid resolutions that
	 * approach the final resolution. Each solution gives us a close
	 * starting point for the next higher resolution. 
	 *
	 * The middle loop, uses Gauss-Seidel relaxation to approach
	 * the desired solution at a given grid resolution.
	 *
	 * The inner loop, uses the conjugate-gradient method to solve
	 * a line of values simultaniously in a particular dimension. 
  	 * All the lines in each dimension are processed in red/black order
  	 * to optimise convergence rate.
  	 *
  	 * (conjugate gradient seems to be slower than pure relaxation, so
	 * it is not currently used.)
	 *
  	 * If JITTERS is zero, then a pure Gauss-Seidel relaxation approach
  	 * is used, with the solution elements being updated in RED-BLACK
	 * order. Experimentation seems to prove that this is the overall
  	 * fastest approach.
  	 * 
  	 * The equation Ax = b solves the fitting for the derivative of 
  	 * the fit error == 0. The error metric used is the norm(b - A * x)/norm(b).
  	 * I'm not sure if that is the best metric for the problem at hand though.
  	 * b[] is only non-zero where there are scattered data points (or a weak 
  	 * default function), so the error metric is being normalised to number
	 * of scattered data points. Perhaps normb should always be == 1.0 ?
	 * 
	 * The norm(b - A * x) is effectively the RMS error of the derivative
	 * fit, so it balances average error and peak error, but another
	 * approach might be to work on peak error, and apply Gauss-Seidel relaxation
	 * to grid points in peak error order (ie. relax the top 10% of grid
	 * points each itteration round) ??
	 *
	 */

	/* Note that we process the A[][] sparse columns in compact form */

#ifdef DEBUG_PROGRESS
	printf("Target tol = %f\n",tol);
#endif
	/* If the number of point is small, or it is just one */
	/* dimensional, solve it more directly. */
	if (m->g.bres <= 4) {	/* Don't want to multigrid below this */
		/* Solve using just conjugate-gradient */
		cj_line(A, x, b, gno, acols, xcol, 0, gno, 1, 10 * gno, tol);
	} else {	/* Try relax till done */
		double lerr = 1.0, err = tol * 10.0, derr, ovsh = 1.0;
		int jitters = JITTERS;

		/* Compute an initial error */
		err = soln_err(A, x, b, m->q.normb, gno, acols, xcol);
#ifdef DEBUG_PROGRESS
		printf("Initial error res %d is %f\n",gres[0],err);
#endif

		for (i = 0; i < 500; i++) {
			if (i < jitters) {	/* conjugate-gradient and relaxation */
				lerr = err;
				err = one_itter1(A, x, b, m->q.normb, gno, acols, xcol, di, gres, gci, m->g.mres, tol * CONJ_TOL);
			
				derr = err/lerr;
				if (derr > 0.8)			/* We're not improving using itter1() fast enough */
					jitters = i-1;		/* Move to just relaxation */
#ifdef DEBUG_PROGRESS
				printf("one_itter1 at res %d has err %f, derr %f\n",gres[0],err,derr);
#endif
			} else {	/* Use just relaxation */
				int j;
				if (i == jitters) {	/* Never done a relaxation itter before */
					ni = 1;		/* Just do one, to get estimate */
				} else {
					ni = (int)(((log(tol) - log(err)) * (double)ni)/(log(err) - log(lerr)));
					if (ni < 1)
						ni = 1;			/* Minimum of 1 at a time */
					else if (ni > MAXNI)
						ni = MAXNI;		/* Maximum of MAXNI at a time */
				}
				for (j = 0; j < ni; j++)	/* Do them in groups for efficiency */
					one_itter2(A, x, b, gno, acols, xcol, di, gres, gci, ovsh);
				lerr = err;
				err = soln_err(A, x, b, m->q.normb, gno, acols, xcol);
				derr = pow(err/lerr, 1.0/ni);
#ifdef DEBUG_PROGRESS
				printf("%d * one_itter2 at res %d has err %f, derr %f\n",ni,gres[0],err,derr);
#endif
	/* 			if (s->verbose) {
    printf("*"); fflush(stdout);  
   }*/
			}
#ifdef OVERRLX
			if (derr > 0.7 && derr < 1.0) {
				ovsh = 1.0 * derr/0.7;
			}
#endif /* OVERRLX */
			if (err < tol || (derr <= 1.0 && derr > TOL_IMP))	/* within tol or < tol_improvement */
				break;
		}
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Do one relaxation itteration of applying       */
/* cj_line to solve each line of x[] values, in   */
/* each line of each dimension. Return the        */
/* current solution error.                        */
static double
one_itter1(
	double **A,		/* Sparse A[][] matrix */
	double *x,		/* x[] matrix */
	double *b,		/* b[] matrix */
	double normb,	/* Norm of b[] */
	int gno,		/* Total number of unknowns */
	int acols,		/* Use colums in A[][] */
	int *xcol,		/* sparse expansion lookup array */
	int di,			/* number of dimensions */
	int *gres,		/* Grid resolution */
	int *gci,		/* Array increment for each dimension */
	int max_it,		/* maximum number of itterations to use (min gres) */
	double tol		/* Tollerance to solve line */
) {
	int e,d;
	
	/* For each dimension */
	for (d = 0; d < di; d++) {
		int ld = d == 0 ? 1 : 0;	/* lowest dim */
		int sof, gc[MXRI];

		for (sof = 0, e = 0; e < di; e++)
			gc[e] = 0;	/* init coords */
	
		/* Until we've done all lines in direction d, */
		/* processed in red/black order */
		for (e = 0; e < di;) {

			/* Solve a line */
/* printf("~~solve line start %d, inc %d, len %d\n",sof,gci[d],gres[e]); */
			cj_line(A, x, b, gno, acols, xcol, sof, gres[e], gci[d], max_it, tol);

			/* Increment index */
			for (e = 0; e < di; e++) {
				if (e == d)		/* Don't go in direction d */
					continue;
				if (e == ld) {
					gc[e] += 2;	/* Inc coordinate */
					sof += 2 * gci[e];	/* Track start point */
				} else {
					gc[e] += 1;	/* Inc coordinate */
					sof += 1 * gci[e];	/* Track start point */
				}
				if (gc[e] < gres[e])
					break;	/* No carry */
				gc[e] -= gres[e];			/* Reset coord */
				sof -= gres[e] * gci[e];	/* Track start point */

				if ((gres[e] & 1) == 0) {	/* Compensate for odd grid */
				    if ((gc[ld] & 1) == 1) {
						gc[ld] -= 1;		/* XOR lsb */
						sof -= gci[ld];
					} else {
						gc[ld] += 1;
						sof += gci[ld];
					}
				}
			}
			/* Stop on reaching 0 */
			for(e = 0; e < di; e++)
				if (gc[e] != 0)
					break;
		}
	}

	return soln_err(A, x, b, normb, gno, acols, xcol);
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Do one relaxation itteration of applying       */
/* direct relaxation to x[] values, in   */
/* red/black order */
static void
one_itter2(
	double **A,		/* Sparse A[][] matrix */
	double *x,		/* x[] matrix */
	double *b,		/* b[] matrix */
	int gno,		/* Total number of unknowns */
	int acols,		/* Use colums in A[][] */
	int *xcol,		/* sparse expansion lookup array */
	int di,			/* number of dimensions */
	int *gres,		/* Grid resolution */
	int *gci,		/* Array increment for each dimension */
	double ovsh		/* Overshoot to use, 1.0 for none */
) {
	int e,i,k;
	int gc[MXRI];

	for (i = e = 0; e < di; e++)
		gc[e] = 0;	/* init coords */

	for (e = 0; e < di;) {
		int k0,k1,k2,k3;
		double sm = 0.0;

		/* Right of diagonal in 4's */
		for (k = 1, k3 = i+xcol[k+3]; (k+3) < acols && k3 < gno; k += 4, k3 = i+xcol[k+3]) {
			k0 = i + xcol[k+0];
			k1 = i + xcol[k+1];
			k2 = i + xcol[k+2];
			sm += A[i][k+0] * x[k0];
			sm += A[i][k+1] * x[k1];
			sm += A[i][k+2] * x[k2];
			sm += A[i][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = i + xcol[k]; k < acols && k3 < gno; k++, k3 = i + xcol[k])
			sm += A[i][k] * x[k3];

		/* Left of diagonal in 4's */
		for (k = 1, k3 = i-xcol[k+3]; (k+3) < acols && k3 >= 0; k += 4, k3 = i-xcol[k+3]) {
			k0 = i-xcol[k+0];
			k1 = i-xcol[k+1];
			k2 = i-xcol[k+2];
			sm += A[k0][k+0] * x[k0];
			sm += A[k1][k+1] * x[k1];
			sm += A[k2][k+2] * x[k2];
			sm += A[k3][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = i-xcol[k]; k < acols && k3 >= 0; k++, k3 = i-xcol[k])
			sm += A[k3][k] * x[k3];

/*		x[i] = (b[i] - sm)/A[i][0]; */
		x[i] += ovsh * ((b[i] - sm)/A[i][0] - x[i]);

#ifdef RED_BLACK
		/* Increment index */
		for (e = 0; e < di; e++) {
			if (e == 0) {
				gc[0] += 2;	/* Inc coordinate by 2 */
				i += 2;		/* Track start point */
			} else {
				gc[e] += 1;		/* Inc coordinate */
				i += gci[e];	/* Track start point */
			}
			if (gc[e] < gres[e])
				break;	/* No carry */
			gc[e] -= gres[e];				/* Reset coord */
			i -= gres[e] * gci[e];			/* Track start point */

			if ((gres[e] & 1) == 0) {		/* Compensate for odd grid */
				gc[0] ^= 1; 			/* XOR lsb */
				i ^= 1;
			}
		}
		/* Stop on reaching 0 */
		for(e = 0; e < di; e++)
			if (gc[e] != 0)
				break;
#else
		if (++i >= gno)
			break;
#endif
	}
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* This function returns the current solution error. */
static double
soln_err(
	double **A,		/* Sparse A[][] matrix */
	double *x,		/* x[] matrix */
	double *b,		/* b[] matrix */
	double normb,	/* Norm of b[] */
	int gno,		/* Total number of unknowns */
	int acols,		/* Use colums in A[][] */
	int *xcol		/* sparse expansion lookup array */
) {
	int i, k;
	/* double sm; */
	double resid;

	/* Compute norm of b - A * x */
	resid = 0.0;
	for (i = 0; i < gno; i++) {
		int k0,k1,k2,k3;
		double sm = 0.0;

		/* Diagonal and to right in 4's */
		for (k = 0, k3 = i+xcol[k+3]; (k+3) < acols && k3 < gno; k += 4, k3 = i+xcol[k+3]) {
			k0 = i + xcol[k+0];
			k1 = i + xcol[k+1];
			k2 = i + xcol[k+2];
			sm += A[i][k+0] * x[k0];
			sm += A[i][k+1] * x[k1];
			sm += A[i][k+2] * x[k2];
			sm += A[i][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = i + xcol[k]; k < acols && k3 < gno; k++, k3 = i + xcol[k])
			sm += A[i][k] * x[k3];

		/* Left of diagonal in 4's */
		for (k = 1, k3 = i-xcol[k+3]; (k+3) < acols && k3 >= 0; k += 4, k3 = i-xcol[k+3]) {
			k0 = i-xcol[k+0];
			k1 = i-xcol[k+1];
			k2 = i-xcol[k+2];
			sm += A[k0][k+0] * x[k0];
			sm += A[k1][k+1] * x[k1];
			sm += A[k2][k+2] * x[k2];
			sm += A[k3][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = i-xcol[k]; k < acols && k3 >= 0; k++, k3 = i-xcol[k])
			sm += A[k3][k] * x[k3];

		sm = b[i] - sm;
		resid += sm * sm;
	}
	resid = sqrt(resid);

	return resid/normb;
}

/* - - - - - - - - - - - - - - - - - - - - - - - -*/
/* This function applies the conjugate gradient   */
/* algorithm to completely solve a line of values */
/* in one of the dimensions of the grid.          */
/* Return the normalised tollerance achieved.     */
/* This is used by an outer relaxation algorithm  */
static double
cj_line(
	double **A,		/* Sparse A[][] matrix */
	double *x,		/* x[] matrix */
	double *b,		/* b[] matrix */
	int gno,		/* Total number of unknowns */
	int acols,		/* Use colums in A[][] */
	int *xcol,		/* sparse expansion lookup array */
	int sof,		/* start offset of x[] to be found */
	int nid,		/* Number in dimension */
	int inc,		/* Increment to move in lines dimension */
	int max_it,		/* maximum number of itterations to use (min nid) */
	double tol		/* Normalised tollerance to stop on */
) {
	int i, ii, k, it;
	double sm;
	double resid;
	double alpha, rho, rho_1;
	double normb;
	int eof = sof + nid * inc;	/* End offset */

	static double *z = NULL, *xx = NULL, *q = NULL, *r = NULL;
	static double *n = NULL;
	static int l_nid = 0;

	/* Alloc, or re-alloc temporary vectors */
	if (nid > l_nid) {
		if (l_nid > 0) {
			free_dvector(z,0,l_nid);
			free_dvector(r,0,l_nid);
			free_dvector(q,0,l_nid);
			free_dvector(xx,0,l_nid);
			free_dvector(n,0,l_nid);
		}
		if ((n = dvector(0,nid)) == NULL)
			error("Malloc of n[] failed");
		if ((z = dvector(0,nid)) == NULL)
			error("Malloc of z[] failed");
		if ((xx = dvector(0,nid)) == NULL)
			error("Malloc of xx[] failed");
		if ((q = dvector(0,nid)) == NULL)
			error("Malloc of q[] failed");
		if ((r = dvector(0,nid)) == NULL)
			error("Malloc of r[] failed");
		l_nid = nid;
	}

	/* Compute initial norm of b[] */
	for (sm = 0.0, ii = sof; ii < eof; ii += inc)
		sm += b[ii] * b[ii];
	normb = sqrt(sm);
	if (normb == 0.0) 
		normb = 1.0;

	/* Compute r = b - A * x */
	for (i = 0, ii = sof; i < nid; i++, ii += inc) {
		int k0,k1,k2,k3;
		sm = 0.0;

		/* Diagonal and to right in 4's */
		for (k = 0, k3 = ii+xcol[k+3]; (k+3) < acols && k3 < gno; k += 4, k3 = ii+xcol[k+3]) {
			k0 = ii + xcol[k+0];
			k1 = ii + xcol[k+1];
			k2 = ii + xcol[k+2];
			sm += A[ii][k+0] * x[k0];
			sm += A[ii][k+1] * x[k1];
			sm += A[ii][k+2] * x[k2];
			sm += A[ii][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = ii + xcol[k]; k < acols && k3 < gno; k++, k3 = ii + xcol[k])
			sm += A[ii][k] * x[k3];

		/* Left of diagonal in 4's */
		for (k = 1, k3 = ii-xcol[k+3]; (k+3) < acols && k3 >= 0; k += 4, k3 = ii-xcol[k+3]) {
			k0 = ii-xcol[k+0];
			k1 = ii-xcol[k+1];
			k2 = ii-xcol[k+2];
			sm += A[k0][k+0] * x[k0];
			sm += A[k1][k+1] * x[k1];
			sm += A[k2][k+2] * x[k2];
			sm += A[k3][k+3] * x[k3];
		}
		/* Finish any remaining */
		for (k3 = ii-xcol[k]; k < acols && k3 >= 0; k++, k3 = ii-xcol[k])
			sm += A[k3][k] * x[k3];

		r[i] = b[ii] - sm;
	}

	/* Transfer the x[] values we are trying to solve into */
	/* temporary xx[]. The values of interest in x[] will be */
	/* used to hold the p[] values, so that q = A * p can be */
	/* computed in the context of the x[] values we are not */
	/* trying to solve. */
	/* We also zero out p[] (== x[] in range), to compute n[]. */
	/* n[] is used to normalize the q = A * p calculation. If we */
	/* were solving all x[], then q = A * p will be 0 for p = 0. */
	/* Since we are only solving some x[], this will not be true. */
	/* We compensate for this by computing q = A * p - n */
	/* (Note that n[] could probably be combined with b[]) */

	for (i = 0, ii = sof; i < nid; i++, ii += inc) {
		xx[i] = x[ii];
		x[ii] = 0.0;
	}
	/* Compute n = A * 0 */
	for (i = 0, ii = sof; i < nid; i++, ii += inc) {
		sm = 0.0;
		for (k = 0; k < acols && (ii+xcol[k]) < gno; k++)
			sm += A[ii][k] * x[ii+xcol[k]];			/* Diagonal and to right */
		for (k = 1; k < acols && (ii-xcol[k]) >= 0; k++)
			sm += A[ii-xcol[k]][k] * x[ii-xcol[k]];	/* Left of diagonal */
		n[i] = sm;
	}

	/* Compute initial error = norm of r[] */
	for (sm = 0.0, i = 0; i < nid; i++)
		sm += r[i] * r[i];
	resid = sqrt(sm)/normb;

	/* Initial conditions don't need improvement */
	if (resid <= tol) {
		tol = resid;
		max_it = 0;
	}

	for (it = 1; it <= max_it; it++) {

		/* Aproximately solve for z[] given r[], */
		/* and also compute rho = r.z */
		for (rho = 0.0, i = 0, ii = sof; i < nid; i++, ii += inc) {
			sm = A[ii][0];
			z[i] = sm != 0.0 ? r[i] / sm : r[i]; 	/* Simple aprox soln. */
			rho += r[i] * z[i];
		}

		if (it == 1) {
			for (i = 0, ii = sof; i < nid; i++, ii += inc)
				x[ii] = z[i];
		} else {
			sm = rho / rho_1;
			for (i = 0, ii = sof; i < nid; i++, ii += inc)
				x[ii] = z[i] + sm * x[ii];
		}
		/* Compute q = A * p  - n, */
		/* and also alpha = p.q */
		for (alpha = 0.0, i = 0, ii = sof; i < nid; i++, ii += inc) {
			sm = A[ii][0] * x[ii];
			for (k = 1; k < acols; k++) {
				int pxk = xcol[k];
				int nxk = ii-pxk;
				pxk += ii;
				if (pxk < gno)
					sm += A[ii][k] * x[pxk];
				if (nxk >= 0)
					sm += A[nxk][k] * x[nxk];
			}
			q[i] = sm - n[i];
			alpha += q[i] * x[ii];
		}

		if (alpha != 0.0)
			alpha = rho / alpha;
		else
			alpha = 0.5;	/* ?????? */
		    
		/* Adjust soln and residual vectors, */
		/* and also norm of r[] */
		for (resid = 0.0, i = 0, ii = sof; i < nid; i++, ii += inc) {
			xx[i] += alpha * x[ii];
			r[i]  -= alpha * q[i];
			resid += r[i] * r[i];
		}
		resid = sqrt(resid)/normb;

		/* If we're done as far as we want */
		if (resid <= tol) {
			tol = resid;
			max_it = it;
			break;
		}
		rho_1 = rho;
	}
	/* Substitute solution xx[] back into x[] */
	for (i = 0, ii = sof; i < nid; i++, ii += inc)
		x[ii] = xx[i];

/*	printf("~~ CJ Itters = %d, tol = %f\n",max_it,tol); */
	return tol;
}

/* ============================================ */















