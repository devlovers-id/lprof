#ifndef RSPL_H
#define RSPL_H
/* $Id: rspl.h,v 1.2 2006/02/19 23:59:12 hvengel Exp $ */
/* 
 * Argyll Color Correction System
 * Multi-dimensional regularized spline data structure
 *
 * Author: Graeme W. Gill
 * Date:   2000/10/29
 *
 * Copyright 1996 - 2004 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

#include "numsup.h"

/** Configuration **/

/** General Limits **/

#define MXDI 8			/* Maximum input dimensionality */
#define MXDO 8			/* Maximum output dimensionality (Is not fully tested!!!) */
#define LOG2MXDI 3		/* log2 MXDI */
#define DEF2MXDI 16		/* Default allocation size for 2^di */
#define DEF3MXDI 81		/* Default allocation size for 3^di */

#if MXDI > MXDO		/* Maximum of either DI or DO */
# define MXDIDO MXDI
#else
# define MXDIDO MXDO
#endif

/* RESTRICTED SIZE Limits, used for reverse, spline and scattered interpolation */

#define MXRI 4			/* Maximum input dimensionality */
#define MXRO 8			/* Maximum output dimensionality (Is not fully tested!!!) */
#define LOG2MXRI 2		/* log2 MXRI */
#define POW2MXRI 16		/* 2 ^ MXRI */
#define POW3MXRI 81		/* 3 ^ MXRI */
#define HACOMPS ((POW3MXRI + 2 * MXRI + 1)/2) /* Maximum number of array components */

#if MXRI > MXRO		/* Maximum of either RI or RO */
# define MXRIRO MXRI
#else
# define MXRIRO MXRO
#endif


/** Definitions **/

/* General data point position/value structure */
/* This is mean't to be compatible with color structure */
/* when MXDI and MXDO == 4 */
typedef double datai[MXDI];
typedef double datao[MXDO];
typedef float dati[MXDI];
typedef float dato[MXDO];

/* Restricted size versions */
typedef double ratai[MXRI];
typedef double ratao[MXRO];
typedef float rati[MXRI];
typedef float rato[MXRO];

/* Interface coordinate value */
typedef struct {
	double p[MXDI];		/* coordinate position */
	double v[MXDO];		/* function values */
} co;

/* Interface coordinate value + weighting */
typedef struct {
	double p[MXDI];		/* coordinate position */
	double v[MXDO];		/* function values */
	double w;			/* Weight to give this point, nominally 1.0 */ 
} cow;

/* Scattered data Per data point data */
struct _rpnts {
	double p[MXRI];		/* Data position [di] */
	double v[MXRO];		/* Data value    [fdi] */
	double k;			/* Weight factor (nominally 1.0, less for lower confidence data point) */
}; typedef struct _rpnts rpnts;

/* Hermite interpolation magic data */
typedef struct {
	int p;			/* The parameter power combination */
	int i;			/* The surrounding cube vertex index */
	int j;			/* The dimension combination */
	float wgt;
} magic_data;

#include "rev.h"			/* Reverse interpolation defintions */

/* Structure for final resolution multi-dimensional regularized spline data */
struct _rspl {

	/* Global rspl state */
	int debug;		/* 0 = no debug */
	int verbose;	/* 0 = no verbose */
	double smooth;	/* Smoothness factor */
	int symdom;		/* 0 = non-symetric smoothness with different grid resolutions, */
	           		/* 1 = symetric smoothness with different grid resolutions, */
	double avgdev;	/* Average Deviation of input values as proportion of input range. */

	int di;			/* Input dimensionality */
	int fdi;		/* Output function dimensionality */

	/* Weak default function related information */
	double weak;	/* Weak total weighting, nominal = 1.0 */
	void *dfctx;	/* Opaque function context */
	void (*dfunc)(void *cbntx, double *out, double *in);
					/* Function to set from */

	/* Scattered Data point related information */
	int nm;			/* Enforce non-monotonicity flag (Doesn't work reliably) */
	int xf;			/* Extra fitting flag - relax smoothness near poor fitting points */
	int inc;		/* Flag - Incremental scattered data mode */
	int sinit;		/* Flag - Grid has been initialised from scattered data */
	struct {
		int no;			/* Number of data points in array */
		int ano;		/* Number of data points added last time around (incremental) */
		rpnts *a;		/* Array of data points */
		datao vl,vw;	/* Data value low/width - used to normalize smoothness values */
		datao va;		/* Data value averages */
	} d;
	int niters;		/* Number of multigrid itterations needed */
	int titers;		/* Total multigrid itterations including extra fit itterations */
	int **ires; 	/* Resolution for each itteration and dimension */
	void **mgtmps[MXRO]; /* Store pointers to re-usable mgtmp when incremental */


	/* Grid points data */
	struct {
		int res[MXDI];	/* Single dimension grid resolution for each axis */
		int bres, brix;	/* Biggest resolution and its index */
		int mres;		/* Geometric mean res[] */
		int no;			/* Total number of points in grid = res[0] * res[1] * .. res[di-1] */
		datai l,h,w;	/* Grid low, high, grid cell width */
						/* This is used to map from the input domain to the grid */

		datao fmin, fmax;	/* Min & max values of grid output (function) variables */
		double fscale;		/* Overall magnitude of output values */
		int fminmax_valid;	/* Min/max/scale cached values valid flag. */
		int limitv_cached;	/* Flag: Ink limit values have been set in the grid array */

#define G_XTRA 3		/* Extra floats per grid point */
		float *alloc;	/* Grid points allocated address */
		float *a;		/* Grid point flags + data */
						/* Array is res ^ di entries float[fdi+G_XTRA], offset by G_XTRA */
						/* (But is expanded when spline interpolaton is active) */
						/* float[-1] contains the ink limit function value, L_UNINIT if not initd */
						/* float[-2] contains the edge flag values. */
						/* float[-3] contains the touched flag generation count. */
						/* (k value for non-linear fit would be another entry.) */
						/* Flag values are 3 bits for each dimension. Bits 1,0 form */
						/* 2 bit distance from edge: 0 for on edge of grid, */
	                    /* 1 for next row, 2 for 3rd row and beyond. If bit 2 is set, */
						/* then we are on the lower edge. This limits di to 10 or less, */
						/* with the two MS bits spare. */
		int pss;		/* Grid point structure size = fdi+G_XTRA */

		/* Uninigialised limit value */
#define L_UNINIT ((float)-1e38)

		/* Macros to access flags. Arguments are a pointer to base grid point and  */
		/* Flag value is distance from edge in bottom 2 bits, values 0, 1 or 2 maximum. */
		/* bit 2 is set if the distance is to the lower edge. */
#define FLV(fp) (*((unsigned int *)((fp)-2)))
		/* Init the flag values to 0 */
#define I_FL(fp) (FLV(fp) = 0)
		/* Return 3 bit flag data */
#define G_FL(fp,di) ((FLV(fp) >> (3 * (di))) & 7)
		/* Set 3 bit flag data */
#define S_FL(fp,di,v) (FLV(fp) = (FLV(fp) & ~(7 << (3 * (di)))) | (((v) & 7) << (3 * (di))))

		/* Macro to access touched flag. Arguments are a pointer to base grid point. */
#define TOUCHF(fp) (*((unsigned int *)((fp)-3)))

		/* Grid array offset lookups - in floats */
		int ci[MXDI];		/* Grid coordinate increments for each dimension */
		int fci[MXDI];		/* Grid coordinate increments for each dimension in floats */
		int *hi;			/* 2^di Combination offset for sequence through cube. */
		int a_hi[DEF2MXDI];	/* Default allocation for *hi */
		int *fhi;			/* Combination offset for sequence through cube of */
							/* 2^di points, starting at base, in floats */
		int a_fhi[DEF2MXDI];/* Default allocation for *hi */

		unsigned int touch;	/* Cell touched flag count */
	} g;

	/* Hermite spline interpolation support */
	struct {
		magic_data *magic;	/* Magic matrix - non-zero elements only, Non-NULL if splining */
		int nm;			/* number in magic data list */
		int spline;		/* Non-zero if spline data is present in g.a */
						/* Changes from float g.a[res ^ di][fdi+G_XTRA], offset by G_XTRA, */
						/* to float g.a[res ^ di][(2^di * fdi)+G_XTRA], offset by G_XTRA, */
	} spline;

	/* Reverse Interpolation support */
	rev_struct rev;		/* See rev.h */

	/* Methods */

	/* Free ourselves */
	void (*del)(struct _rspl *ss);

	/* Combination lags used by various functions */
#define RSPL_NONMON      0x0001		/* Enable elimination of non-monoticities */
#define RSPL_EXTRAFIT    0x0002		/* Enable extra fitting effort by relaxing smoothing */
#define RSPL_SYMDOMAIN   0x0004		/* Maintain symetric smoothness with nonsym. resolution */
#define RSPL_INCREMENTAL 0x0008		/* Enable adding more data points */ 
#define RSPL_FINAL       0x0010		/* Signal to add_rspl() that this is the last points */
#define RSPL_VERBOSE     0x8000		/* Print progress messages */

	/* Initialise from scattered data. RESTRICTED SIZE */
	/* Return non-zero if result is non-monotonic */
	int
	(*fit_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags */
		co *d,			/* Array holding position and function values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, nominal = 1.0 */
		double avgdev	/* Average Deviation of input values as proportion of input range, */
						/* typical value 0.0025 ? (aprox. = 0.564 times the standard deviation) */
	);

	/* Initialise from scattered data, with per point weighting. RESTRICTED SIZE */
	/* Return non-zero if result is non-monotonic */
	int
	(*fit_rspl_w)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags */
		cow *d,			/* Array holding position, function and weight values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, nominal = 1.0 */
		double avgdev	/* Average Deviation of input values as proportion of input range, */
						/* typical value 0.0025 ? (aprox. = 0.564 times the standard deviation) */
	);

	/* Initialise from scattered data, with weak default function. */
	/* RESTRICTED SIZE */
	/* Return non-zero if result is non-monotonic */
	int
	(*fit_rspl_df)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags */
		co *d,			/* Array holding position and function values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, nominal = 1.0 */
		double avgdev,	/* Average Deviation of input values as proportion of input range, */
						/* typical value 0.0025 ? (aprox. = 0.564 times the standard deviation) */
		double weak,	/* Weak weighting, nominal = 1.0 */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in)		/* Function to set from */
	);

	/* Initialise from scattered data, with per point weighting and weak default function. */
	/* RESTRICTED SIZE */
	/* Return non-zero if result is non-monotonic */
	int
	(*fit_rspl_w_df)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags */
		cow *d,			/* Array holding position, function and weight values of data points */
		int ndp,		/* Number of data points */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution, ncells = gres-1 */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh,	/* Data value high normalize - NULL = default 1.0 */
		double smooth,	/* Smoothing factor, nominal = 1.0 */
		double avgdev,	/* Average Deviation of input values as proportion of input range, */
						/* typical value 0.0025 ? (aprox. = 0.564 times the standard deviation) */
		double weak,	/* Weak weighting, nominal = 1.0 */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in)		/* Function to set from */
	);

	/* If the incremental flag was set on the fit_rspl call, add more points */
	/* to the rspl and update it. */
	int
	(*add_rspl)(
		struct _rspl *s,/* this */
		int flags,		/* Combination of flags */
		co *d,			/* Array holding position and function values of data points */
		int ndp			/* Number of data points to add */
	);

	/* Set values from a function. Grid index values are supplied */
	/* "under" in[] at *((int*)&in[-e-1]) */
	/* Return non-monotonic status */
	int
	(*set_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags (not used) */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in),		/* Function to set from */
		datai glow,		/* Grid low scale - will expand to enclose data, NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - will expand to enclose data, NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution */
		datao vlow,		/* Data value low normalize, NULL = default 0.0 */
		datao vhigh		/* Data value high normalize - NULL = default 1.0 */
	);

	/* Re-set values from a function. Grid index values are supplied */
	/* "under" in[] at *((int*)&iv[-e-1]) */
	/* Return non-monotonic status. Clears all the reverse lookup information. */
	/* It is assumed that the output range remains unchanged. */
	/* Existing output values are supplied in out[] */
	int
	(*re_set_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags (not used) */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in) /* Function to set from */
	);

	/* Scan the rspl grid point locations and values. Grid index values are */
	/* supplied "under" in[] at *((int*)&iv[-e-1]) */
	/* Return non-monotonic status. */
	void
	(*scan_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags (not used) */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, double *out, double *in) /* Function that gets given values */
	);

	/* Set values by multi-grid optimisation using the provided function. */
	int (*opt_rspl)(
		struct _rspl *s,/* this */
		int flags,		/* Combination of flags */
		int tdi,		/* Dimensionality of target data */
		int adi,		/* Additional grid point data allowance */
		double **vdata,	/* di^2 array of function, target and additional values to init */
						/* array corners with. */
		double (*func)(void *fdata, double *inout, double *surav, int first, double *cw),
						/* Optimisation function */
		void *fdata,	/* Opaque data needed by function */
		datai glow,		/* Grid low scale - NULL = default 0.0 */
		datai ghigh,	/* Grid high scale - NULL = default 1.0 */
		int gres[MXDI],	/* Spline grid resolution */
		datao vlow,		/* Data value low normalize - NULL = default 0.0 */
		datao vhigh		/* Data value high normalize - NULL = default 1.0 */
	);

	/* Filter the existing values. Grid index values are supplied "under" in[] */
	void
	(*filter_rspl)(
		struct _rspl *s,	/* this */
		int flags,		/* Combination of flags (not used) */
		void *cbntx,	/* Opaque function context */
		void (*func)(void *cbntx, float **out, double *in, int cvi) /* Function to set from */
	);

	/* Do forward interpolation */
	/* Return 0 if OK, 1 if input was clipped to grid */
	int (*interp)(
		struct _rspl *s,	/* this */
		co *p);				/* Input and output values */

	/* Do splined forward interpolation. RESTRICTED SIZE */
	/* Return 0 if OK, 1 if input was clipped to grid */
	int (*spline_interp)(
		struct _rspl *s,	/* this */
		co *p);				/* Input and output values */


	/* Set the ink limit information for any reverse interpolation. RESTRICTED SIZE */
	/* Calling this will clear the reverse interpolaton cache. */
	void (*rev_set_limit)(
		struct _rspl *s,	/* this */
		double (*limit)(void *lcntx, double *in),	/* Optional input space limit function. */
		                	/* Function should evaluate in[0..di-1], and return number */
		                	/* that is not to exceed limitv. NULL if not used. */
		void *lcntx,		/* Context passed to limit() */
		double limitv		/* Value that limit() is not to exceed */
	);

	/* Get the ink limit information for any reverse interpolation. RESTRICTED SIZE */
	void (*rev_get_limit)(
		struct _rspl *s,	/* this */
		double (**limit)(void *lcntx, double *in),
							/* Return pointer to function of NULL if not set */
		void **lcntx,		/* return context pointer */
		double *limitv		/* Return limit value */
	);

	/* Possible reverse hint flags */
#define RSPL_WILLCLIP 0x0001		/* Hint that clipping will be needed */
#define RSPL_EXACTAUX 0x0002		/* Hint that auxiliary target will be matched exactly */
#define RSPL_AUXLOCUS 0x0004		/* Auxiliary target is proportion of locus, not */
									/* absolute. Implies EXACTAUX hint. */
#define RSPL_NEARCLIP 0x0008		/* If clipping occurs, return the nearest solution, */
									/* rather than the one in the clip direction. */
	/* Return value masks */
#define RSPL_DIDCLIP 0x8000		/* If this bit is set, at least one soln. and clipping occured */
#define RSPL_NOSOLNS 0x7fff		/* And return value with this mask to get number of solutions */

	/* Do reverse interpolation given target output values and (optional) auxiliary target */
	/* input values. Return number of results and clip flag. If return value == mxsoln, then */
	/* there might be more results. RESTRICTED SIZE */
	int (*rev_interp)(
		struct _rspl *s,	/* this */
		int flags,			/* Hint flag */
		int mxsoln,			/* Maximum number of solutions allowed for */
		int *auxm,			/* Array of di mask flags, !=0 for valid auxliaries (NULL if no aux) */
		double cdir[MXRO],	/* Clip vector direction and length - NULL if not used */
		co *p);				/* Given target output space value in cpp[0].v[] +  */
							/* target input space auxiliaries in cpp[0].p[], return */
							/* input space solutions in cpp[0..retval-1].p[], and */
							/* (possibly) clipped target values in cpp[0].v[] */

	/* Do reverse search for the locus of the auxiliary input values given a target output. */
	/* Return 1 on finding a valid solution, and 0 if no solutions are found. RESTRICTED SIZE */
	int (*rev_locus)(
		struct _rspl *s,/* this */
		int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no aux) */
		co *cpp,		/* Input target value in cpp[0].v[] */
		double min[MXRI],/* Return minimum auxiliary values */
		double max[MXRI]); /* Return maximum auxiliary values */

	/* Do reverse search for the auxiliary min/max ranges of the solution locus for the */
	/* given target output values. RESTRICTED SIZE */
	/* Return number of locus segments found, up to mxsoln. 0 will be returned if no solutions */
	/* are found. */
	int (*rev_locus_segs)(
		struct _rspl *s,/* this */
		int *auxm,		/* Array of di mask flags, !=0 for valid auxliaries (NULL if no aux) */
		co *cpp,		/* Input value in cpp[0].v[] */
		int mxsoln,		/* Maximum number of solutions allowed for */
		double min[][MXRI],	/* Array of min[MXRI] to hold return segment minimum values. */
		double max[][MXRI]	/* Array of max[MXRI] to hold return segment maximum values. */
	);

	/* return the min and max of the input values valid in the grid */
	void (*get_in_range)(
		struct _rspl *s,			/* this */
		double *min, double *max);	/* Return min/max values */

	/* return the min and max of the output values contained in the grid */
	void (*get_out_range)(
		struct _rspl *s,			/* this */
		double *min, double *max);	/* Return min/max values */

	/* return the overall scale of the output values contained in the grid */
	double (*get_out_scale)(struct _rspl *s);

	/* return the next touched flag count value. */
	/* Whenever this rolls over, all the flags in the grid array will be reset */
	unsigned int (*get_next_touch)(
		struct _rspl *s);			/* this */

	/* Return non-zero if this rspl can be */
	/* used with Restricted Size functions. */
	int (*within_restrictedsize)(
		struct _rspl *s);			/* this */

}; typedef struct _rspl rspl;

/* Create a new, empty rspl object */
rspl *new_rspl(int di, int fdi);	/* Input and output dimentiality */

/* Utility functions */

/* The multi-dimensional access sequence is a distributed */
/* Gray code sequence, with direction reversal */
/* on every alternate power of 2 scale. */
/* It is intended to aid cache access locality in multi-dimensional */
/* regular sampling. It approximates the Hilbert curve sequence. */

/* Structure to hold sequencer info */
struct _rpsh {
	int      di;	/* Dimensionality */
	unsigned res[MXDI];	 /* Resolution per coordinate */
	unsigned bits[MXDI]; /* Bits per coordinate */
	unsigned tbits;	/* Total bits */
	unsigned ix;	/* Current binary index */
	unsigned tmask;	/* Total 2^n count mask */
	unsigned count;	/* Usable count */
}; typedef struct _rpsh rpsh;

/* Initialise, returns total usable count */
unsigned
rpsh_init(rpsh *p, int di, unsigned res[], int co[]);

/* Reset the counter */
void rpsh_reset(rpsh *p);

/* Increment pseudo-hilbert coordinates */
/* Return non-zero if count rolls over to 0 */
int rpsh_inc(rpsh *p, int co[]);

#endif /* RSPL_H */























