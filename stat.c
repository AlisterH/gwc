/*
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2001 Jeffrey J. Welty
	FILE stat.c
	PURPOSE - linear regression
	CONTENTS -
*/

#include <stdio.h>

#include "stat.h"

void matrix_solve(MAT *) ;

#define abs(x) ((x) > 0 ? (x) : -(x))
#define tiny 1e-200

static MAT *coef = MNULL ;
static VEC *b = VNULL ;
static VEC *answer = VNULL ;
static int row, col, N, i ;
static int failed ;

/* LUsolve -- given an LU factorisation in A, solve Ax=b */
VEC	*myLUsolve(A,pivot,b,x)
MAT	*A;
PERM	*pivot;
VEC	*b,*x;
{
	if ( A==(MAT *)NULL || b==(VEC *)NULL || pivot==(PERM *)NULL )
		error(E_NULL,"LUsolve");
	if ( A->m != A->n || A->n != b->dim )
		error(E_SIZES,"LUsolve");

	x = v_resize(x,b->dim);
	px_vec(pivot,b,x);	/* x := P.b */
	Lsolve(A,x,x,1.0);	/* implicit diagonal = 1 */
	Usolve(A,x,x,0.0);	/* explicit diagonal */

	return (x);
}

void matrix_solve(MAT *A)
{
    PERM *pivot ;

    failed = 0 ;
    count_errs(0) ;

    pivot = px_get(A->m) ;

    catchall(
    LUfactor(coef, pivot) ;
    answer = v_get(b->dim);
    answer = myLUsolve(A,pivot,b,answer) ;
    , failed=1 );

    if(failed == 1) {
	answer = v_resize(answer,N) ;
	v_zero(answer) ;
    }

    PX_FREE(pivot) ;
}

void init_reg(int n)
{
    N = n ;

    coef = m_resize(coef, N, N) ;
    b = v_resize(b, N) ;

    /** zero the coef array which will hold the sums **/
    m_zero(coef) ;
    v_zero(b) ;

}

/* Paul Sanders' speedup 1/12/2007 works a little better */
#define PAUL_SANDERS

#ifdef JW_NEW
void sum_reg(double x[], double y)
{
    for(row = 0 ; row < N ; row++) {

	for(col = 0 ; col < N ; col++)
	    coef->me[row][col] += x[row] * x[col] ;

	b->ve[row] += y * x[row] ;
    }
}
#endif

#ifdef PAUL_SANDERS
void sum_reg(double x[], double y)
{
    double *pbve = b->ve ;

    for(row = 0 ; row < N ; row++) {
	double *coef_row = coef->me[row] ;

	for(col = 0 ; col < N ; col++)
	    coef_row[col] += x[row] * x[col] ;

	*pbve += x[row] * y ;

	pbve++ ;
    }
}
#endif

int estimate_reg(double *b_solution)
{
    matrix_solve(coef) ;

    for(row = 0 ; row < N ; row++)
	b_solution[row] = answer->ve[row] ;

    V_FREE(answer) ;
    M_FREE(coef) ;
    V_FREE(b) ;

    return failed ;
}
