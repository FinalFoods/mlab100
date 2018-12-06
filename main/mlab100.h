/*
 * MLAB100 device application software.
 *
 * Copyright (C) 2019, Microbiota Labs Inc. and the MLAB100 contributors.
 *
 */
//=============================================================================

#if !defined(__mlab100_h)
# define __mlab100_h (1)

//-----------------------------------------------------------------------------

#define MLAB_MODEL	"100"

//-----------------------------------------------------------------------------
// Useful (standard) helper macros:

#if !defined(STRINGIFY)
# define STRINGIFY(_x) STRINGIFY2(_x)
# define STRINGIFY2(_x) #_x
#endif // !STRINGIFY

#if !defined(NUMOF)
# define NUMOF(t) (sizeof(t) / sizeof(t[0]))
#endif // !NUMOF

// BIT(x) defined by "soc/soc.h"
#if !defined(BMASK)
# define BMASK(__n,__s)  (((1 << (__s)) - 1) << (__n))
#endif // !BMASK

#if !defined(BVALUE)
# define BVALUE(__n,__v) ((__v) << (__n))
#endif // !BVALUE

#if !defined(BVALUE_GET)
# define BVALUE_GET(__n,__s,__v) (((__v) >> (__n)) & BMASK(0,__s))
#endif // !BVALUE_GET

#define POW2(__n) (1ULL << (__n))

//-----------------------------------------------------------------------------

#endif // !__mlab100_h

//=============================================================================
// EOF mlab100.h
