// Disable SIMD on GCC due to UB when compiling with optimizations.

#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define HLSLPP_SCALAR
#endif

#include "hlsl++.h"