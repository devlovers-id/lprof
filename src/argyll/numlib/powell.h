#ifndef POWELL_H
#define POWELL_H
/* $Id: powell.h,v 1.2 2006/02/19 23:59:12 hvengel Exp $ */
/* Powell multivariate minimiser */

/*
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

/* Standard interface for powell function */
/* return err on sucess, -1.0 on failure */
/* Result will be in cp */
/* Arrays start at 0 */
double powell(
int di,					/* Dimentionality */
double cp[],			/* Initial starting point */
double s[],				/* Size of initial search area */
double ftol,			/* Tollerance of error change to stop on */
int maxit,				/* Maximum iterations allowed */
double (*funk)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata);			/* Opaque data needed by function */

/* Conjugate Gradient optimiser */
/* return err on sucess, -1.0 on failure */
/* Result will be in cp */
double conjgrad(
int di,					/* Dimentionality */
double cp[],			/* Initial starting point */
double s[],				/* Size of initial search area */
double ftol,			/* Tollerance of error change to stop on */
int maxit,				/* Maximum iterations allowed */
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
double (*dfunc)(void *fdata, double dp[], double tp[]),		/* Gradient function to evaluate */
void *fdata				/* Opaque data needed by function */
);

/* Example user function declarations */
double powell_funk(		/* Return function value */
	void *fdata,		/* Opaque data pointer */
	double tp[]);		/* Multivriate input value */

/* Line in multi-dimensional space minimiser */
double brentnd(			/* vector multiplier return value */
double ax,				/* Minimum of multiplier range */
double bx,				/* Starting point multiplier of search */
double cx,				/* Maximum of multiplier range */
double ftol,			/* Tollerance to stop search */
double *xmin,			/* Return value of multiplier at minimum */		
int n,					/* Dimensionality */
double (*func)(void *fdata, double tp[]),		/* Error function to evaluate */
void *fdata,			/* Opaque data */
double pcom[],			/* Base vector point */
double xicom[]);		/* Vector that will be multiplied and added to pcom[] */

#endif /* POWELL_H */
