/*
 *  s_helpers.h
 *  s-buffer
 *
 *  Created by Emre Akı on 2026-01-28.
 *
 *  SYNOPSIS:
 *      Common data structures used in the S-Buffer demo app as well as the test
 *      suite
 */

#ifndef s_helpers_h

#define S_BUFFER_DEFS_ONLY
#include "s_buffer.h"

#define s_helpers_h
#define s_helpers_h_color_t color_t
#define s_helpers_h_vec2_t vec2_t
#define s_helpers_h_seg2_t seg2_t
#define s_helpers_h_S_ZToScreenSpace S_ZToScreenSpace
#define s_helpers_h_S_ToScreenSpace S_ToScreenSpace

typedef unsigned int color_t;

typedef struct {
    int x, y;
} vec2_t;

typedef struct {
    vec2_t src, dst;
    color_t color;
} seg2_t;

//
// S_ZToScreenSpace
// Transforms z-coordinates in view space into lerpable screen space
// coordinates.
//
// The eye is situated at `y = height`, looking toward (0, -1)
//
float S_ZToScreenSpace (const int z, const int height)
{
    return 1.0f / (height - z);
}

//
// S_ToScreenSpace
// Transforms 2-D world space coordinates into screen space x-coordinates.
//
// The eye is situated at (`halfwidth`, `height`), looking toward (0, -1)
//
float
S_ToScreenSpace
( const vec2_t* in,
  const int     halfwidth,
  const int     height,
  const float   z_near )
{
    const vec2_t eye = { halfwidth, height };
    const int view_x = in->x - eye.x;
    const int view_y = eye.y - in->y;

    return (float) halfwidth + view_x * z_near / view_y;
}

#endif
