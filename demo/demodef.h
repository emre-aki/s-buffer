/*
 *  demodef.h
 *  s-buffer
 *
 *  Created by Emre Akı on 2025-09-12.
 *
 *  SYNOPSIS:
 *      Internally used data structures and glyph tables for the S-Buffer demo
 *      app
 */

#ifndef demodef_h

#define S_BUFFER_DEFS_ONLY
#include "s_buffer.h"

#define demodef_h
#define demodef_h_GLYPH_TABLE GLYPH_TABLE;
#define demodef_h_mouse_state_t mouse_state_t

#define FONT_HEIGHT 6
#define FONT_WIDTH 5

typedef struct {
    int x, y;
    byte_t pressed;
} mouse_state_t;

extern const byte_t GLYPH_TABLE[128][FONT_WIDTH * FONT_HEIGHT];

#endif
