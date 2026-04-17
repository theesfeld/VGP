/* SPDX-License-Identifier: MIT */
/* NanoVG implementation -- compile core + GLES3 backend in one translation unit */
#ifdef VGP_HAS_GPU_BACKEND

#include <GLES3/gl3.h>

/* Compile nanovg core */
#include "nanovg.c"

/* Compile GLES3 GL backend */
#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg_gl.h"

#endif