/* $Id: rand.c,v 1.2 2006/02/19 23:59:12 hvengel Exp $ */
/* Integer and floating point random number generator routines */
/*
 * Copyright 2000 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU GENERAL PUBLIC LICENCE :-
 * see the Licence.txt file for licencing details.
 */

#include "numsup.h"
#include "rand.h"

/* 32 bit pseudo random sequencer based on XOR feedback */
/* generates number between 1 and 4294967295 */
#define PSRAND32(S) (((S) & 0x80000000) ? (((S) << 1) ^ 0xa398655d) : ((S) << 1))

/* 32 bit linear congruent generator */
/* generates number between 0 and 4294967295 */
/* (From Knuth & H.W.Lewis) */
#define PSRAND32L(S) ((S) * 1664525L + 1013904223L)

/* Return a 32 bit number between 0 and 4294967295 */
/* Use Knuth shuffle to improve PSRAND32 sequence */
unsigned long
rand32(					/* Return 32 bit random number */
unsigned long seed		/* Optional seed. Non-zero re-initialized with that seed */
) {
#define TSIZE 2843		/* Prime */
	static unsigned long last, ran = 0x12345678;	/* Default seed */
	static unsigned long pvs[TSIZE];
	static int pvs_inited = 0;
	int i;

	if (seed != 0)
		{
		pvs_inited = 0;
		ran = seed;;
		}

	/* Init random storage locations */
	if (pvs_inited == 0)
		{
		for (i = 0; i < TSIZE; i++)
  			pvs[i] = ran = PSRAND32(ran);
		last = ran;
		pvs_inited = 1;
		}
	i = last % TSIZE;		/* New location */
	last = pvs[i];			/* Value generated */
  	pvs[i] = ran = PSRAND32(ran);		/* New value */

	return last-1;
}

/* return a random number between 0.0 and 1.0 */
/* based on rand32 */
double ranno(void) {
	return rand32(0) / 4294967295.0;
}

/* Return a random double in the range min to max */
double
d_rand(double min, double max) {
	double tt;
	tt = ranno();
	return min + (max - min) * tt;
}

/* Return a random integer in the range min to max inclusive */
int
i_rand(int min, int max) {
	double tt;
	tt = ranno();
	return min + floor(0.5 + ((double)(max - min)) * tt);
}


/* Return a random floating point number with a gausian/normal */
/* distribution, centered about 0.0, with standard deviation 1.0 */
double norm_rand(void) {
	static r2 = 0;		/* Can use 2nd number generated */
	static nr2;

	if (r2 == 0) {
		double v1, v2, t1, t2, r1;
		do {
			v1 = d_rand(-1.0, 1.0);
			v2 = d_rand(-1.0, 1.0);
			t1 = v1 * v1 + v2 * v2;
		} while (t1 == 0.0 || t1 >= 1.0);
		t2 = sqrt(-4.0 * log(t1)/t1);
		r2 = v2 * t2;			/* One for next time */
		r2 = 1;
		r1 = v1 * t2;
		return r1;
	} else {
		r2 = 0;
		return nr2;
	}
}







