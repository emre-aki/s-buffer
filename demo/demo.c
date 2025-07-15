/*
 *  demo.c
 *  s-buffer
 *
 *  Created by Emre Akı on 2025-01-19.
 *
 *  SYNOPSIS:
 *      An interactive demo designed to showcase the features and functionality
 *      of the S-Buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <SDL2/SDL.h>

#define S_BUFFER_DEFS_ONLY
#include "s_buffer.h"

#define SIGN(x) (((x) >> 31 << 1) + 1)
#define MIN(a, b) ((((a) <= (b)) * (a)) + (((b) < (a)) * (b)))
#define CLAMP(x, lo, hi) (MIN(SB_MAX(x, lo), hi))

#define WIN_TITLE "s-buffer"

#define S_BUFFER_REPR_H 32
#define BUFFER_W 800
#define BUFFER_H (BUFFER_W + S_BUFFER_REPR_H)
#define WIN_H (BUFFER_H - S_BUFFER_REPR_H)
#define GRID_SIZE 4
#define PROJ_PLANE_Y 704
#define Z_NEAR (WIN_H - PROJ_PLANE_Y)

#define S_BUFFER_MAX_DEPTH 1024
#define MAX_SEGS 128

#define KEY_ESC 27
#define KEY_D 100

#define FONT_HEIGHT 6
#define FONT_WIDTH 5
#define LETTER_SPACING 1

const size_t BUFFER_W_2 = BUFFER_W >> 1;
const size_t GRID = 1 << GRID_SIZE;
byte_t ID = 65;

typedef struct {
    int x, y;
} vec2_t;

typedef struct {
    vec2_t src, dst;
    unsigned int color;
} seg2_t;

typedef struct {
    span_t* span;
    byte_t prev_visited, next_visited;
} dfs_t;

typedef struct {
    int x, y;
    byte_t pressed;
} mouse_state_t;

static const byte_t GLYPH_TABLE[128][FONT_WIDTH * FONT_HEIGHT] = {
    ['a'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
    },
    ['b'] = {
        1, 0, 0, 0, 0,
        1, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 1, 1, 0, 0,
    },
    ['c'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['d'] = {
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
    },
    ['e'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 1, 1, 1, 0,
        1, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
    },
    ['f'] = {
        0, 0, 1, 1, 0,
        0, 1, 0, 0, 0,
        1, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
    },
    ['g'] = {
        0, 1, 1, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['h'] = {
        1, 0, 0, 0, 0,
        1, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
    },
    ['i'] = {
        0, 0, 1, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
    },
    ['j'] = {
        0, 0, 1, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        1, 0, 1, 0, 0,
        0, 1, 1, 0, 0,
    },
    ['k'] = {
        1, 0, 0, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 1, 0, 0,
        1, 1, 0, 0, 0,
        1, 0, 1, 0, 0,
        1, 0, 0, 1, 0,
    },
    ['l'] = {
        0, 1, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 1, 1, 1, 0,
    },
    ['m'] = {
        0, 0, 0, 0, 0,
        0, 1, 0, 1, 1,
        1, 0, 1, 0, 1,
        1, 0, 1, 0, 1,
        1, 0, 1, 0, 1,
        1, 0, 1, 0, 1,
    },
    ['n'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
    },
    ['o'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['p'] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 1, 1, 0, 0,
        1, 0, 0, 0, 0,
    },
    ['q'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
    },
    ['r'] = {
        0, 0, 0, 0, 0,
        1, 0, 1, 1, 0,
        1, 1, 0, 0, 1,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
    },
    ['s'] = {
        0, 0, 0, 0, 0,
        0, 1, 1, 1, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        1, 1, 1, 0, 0,
    },
    ['t'] = {
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
        1, 1, 1, 0, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
    },
    ['u'] = {
        0, 0, 0, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
    },
    ['v'] = {
        0, 0, 0, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['w'] = {
        0, 0, 0, 0, 0,
        1, 0, 0, 0, 1,
        1, 0, 1, 0, 1,
        1, 0, 1, 0, 1,
        1, 0, 1, 0, 1,
        0, 1, 1, 1, 1,
    },
    ['x'] = {
        0, 0, 0, 0, 0,
        1, 0, 1, 0, 0,
        1, 0, 1, 0, 0,
        0, 1, 0, 0, 0,
        1, 0, 1, 0, 0,
        1, 0, 1, 0, 0,
    },
    ['y'] = {
        0, 0, 0, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['z'] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 1, 0,
    },
    ['A'] = { 0 },
    ['B'] = { 0 },
    ['C'] = { 0 },
    ['D'] = { 0 },
    ['E'] = { 0 },
    ['F'] = { 0 },
    ['G'] = { 0 },
    ['H'] = { 0 },
    ['I'] = { 0 },
    ['J'] = { 0 },
    ['K'] = { 0 },
    ['L'] = { 0 },
    ['M'] = { 0 },
    ['N'] = { 0 },
    ['O'] = { 0 },
    ['P'] = { 0 },
    ['Q'] = { 0 },
    ['R'] = { 0 },
    ['S'] = { 0 },
    ['T'] = { 0 },
    ['U'] = { 0 },
    ['V'] = { 0 },
    ['W'] = { 0 },
    ['X'] = { 0 },
    ['Y'] = { 0 },
    ['Z'] = { 0 },
    ['0'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['1'] = {
        0, 0, 1, 0, 0,
        0, 1, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 1, 1, 1, 0,
    },
    ['2'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 1, 0,
    },
    ['3'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['4'] = {
        0, 0, 1, 1, 0,
        0, 1, 0, 1, 0,
        1, 0, 0, 1, 0,
        1, 1, 1, 1, 1,
        0, 0, 0, 1, 0,
        0, 0, 0, 1, 0,
    },
    ['5'] = {
        1, 1, 1, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 0, 0,
        0, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['6'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 0, 0,
        1, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
    ['7'] = {
        1, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 0, 1, 0, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
    },
    ['8'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 0, 0,

    },
    ['9'] = {
        0, 1, 1, 0, 0,
        1, 0, 0, 1, 0,
        1, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },

    [','] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 1, 0, 0,
    },

    ['.'] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
    },
    ['-'] = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
    },
    [':'] = {
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 0, 0,
    },
    ['%'] = {
        1, 0, 0, 0, 1,
        0, 0, 0, 1, 0,
        0, 0, 1, 0, 0,
        0, 1, 0, 0, 0,
        1, 0, 0, 0, 0,
        0, 0, 0, 0, 1,
    },
    ['('] = {
        0, 0, 1, 1, 0,
        0, 1, 0, 0, 0,
        1, 0, 0, 0, 0,
        1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 1, 0,
    },
    [')'] = {
        0, 1, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1,
        0, 0, 0, 0, 1,
        0, 0, 0, 1, 0,
        0, 1, 1, 0, 0,
    },
};

const size_t SBUFFER_SIZE = sizeof(sbuffer_t);
const size_t SPAN_SIZE = sizeof(span_t);

int FastAbs (int x)
{
    const int sign_mask = x >> 31;

    return (x ^ sign_mask) + (1 & sign_mask);
}

float ZToScreenSpace (int z)
{
    return 1.0f / (WIN_H - z);
}

void ToScreenSpace (const vec2_t* in, float* out)
{
    const vec2_t eye = { BUFFER_W_2, WIN_H };
    const int view_x = in->x - eye.x, view_y = eye.y - in->y;
    float screen_x = (float) view_x * Z_NEAR / view_y;
    *out = screen_x + BUFFER_W_2;
}

SDL_Window*
CreateWindow
( const char* title,
  int x, int y,
  int w, int h,
  Uint32 flags )
{
    SDL_Window* window = SDL_CreateWindow(title, x, y, w, h, flags);

    if (!window)
    {
        printf("[CreateWindow] Could not create window!: %s\n", SDL_GetError());

        return 0;
    }

    return window;
}

SDL_Renderer* CreateCtx (SDL_Window* window)
{
    SDL_Renderer* ctx = SDL_CreateRenderer(window,
                                           -1,
                                           SDL_RENDERER_SOFTWARE);

    if (!ctx)
    {
        printf("[CreateCtx] Error while creating a rendering context: %s\n",
               SDL_GetError());

        return 0;
    }

    return ctx;
}

void Destroy (SDL_Window* window, SDL_Renderer* ctx)
{
    if (ctx) SDL_DestroyRenderer(ctx);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void FrameFlush (SDL_Renderer* ctx)
{
    SDL_RenderPresent(ctx);
    SDL_SetRenderDrawColor(ctx, 0, 0, 0, 0);
    SDL_RenderClear(ctx);
}

void GetMouseCoords (int* x, int* y)
{
    SDL_GetMouseState(x, y);

    /* snap to a grid of 16x16 blocks */
    *x >>= GRID_SIZE; *y >>= GRID_SIZE;
    *x <<= GRID_SIZE; *y <<= GRID_SIZE;
}

void HandleEventSync (byte_t* do_run, mouse_state_t* ms, byte_t* ks)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                *do_run = 0;

                break;
            case SDL_MOUSEBUTTONDOWN:
                ms->pressed = event.button.button;
                ms->x = event.button.x; ms->y = event.button.y;
                /* snap to a grid of 16x16 blocks */
                ms->x >>= GRID_SIZE; ms->y >>= GRID_SIZE;
                ms->x <<= GRID_SIZE; ms->y <<= GRID_SIZE;

                break;
            case SDL_MOUSEBUTTONUP:
                ms->pressed = 0;

                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        *(ks + KEY_ESC) = 0xff;
                        ms->pressed = 0;

                        break;
                    case SDLK_d:
                        *(ks + KEY_D) = 0xff;

                        break;
                    default:
                        break;
                }

                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        *(ks + KEY_ESC) = 0;
                        ms->pressed = 0;

                        break;
                    case SDLK_d:
                        *(ks + KEY_D) = 0;

                        break;
                    default:
                        break;
                }

                break;
            default:
                break;
        }
    }
}

int RandomColor ()
{
    return (rand() & 0xff000000) |
           (rand() & 0xff0000) |
           (rand() & 0xff00) |
           0xff;
}

//
// DrawLineBresenham
// Why not use `SDL_RenderDrawLine` you degenerate slime!
//
void
DrawLineBresenham
( SDL_Renderer* ctx,
  float x0, float y0,
  float x1, float y1,
  int color )
{
    byte_t r, g, b, a;
    SDL_GetRenderDrawColor(ctx, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(ctx,
                           (color & 0xff000000) >> 24,
                           (color & 0xff0000) >> 16,
                           (color & 0xff00) >> 8,
                           0xff);

    int X0 = ceil(x0 - 0.5), Y0 = ceil(y0 - 0.5);
    int X1 = ceil(x1 - 0.5), Y1 = ceil(y1 - 0.5);

    /* TODO: crude raster clipping for the time being.
     * maybe copy over the `R_ClampLine` subroutine from `tmp3d`??
     */
    X0 = CLAMP(X0, 0, BUFFER_W); Y0 = CLAMP(Y0, 0, WIN_H);
    X1 = CLAMP(X1, 0, BUFFER_W); Y1 = CLAMP(Y1, 0, WIN_H);

    /* horizontal sweep */
    if (FastAbs(Y0 - Y1) <= FastAbs(X0 - X1))
    {
        const byte_t x0_min = X0 <= X1;
        const int sx = x0_min * X0 + !x0_min * X1;
        const int sy = x0_min * Y0 + !x0_min * Y1;
        const int dx = x0_min * X1 + !x0_min * X0;
        const int dy = x0_min * Y1 + !x0_min * Y0;
        const int delta_x = dx - sx, delta_y = dy - sy;
        const int sign_delta_y = SIGN(delta_y);
        const int px = sign_delta_y * (delta_y + delta_y);
        const int py = -delta_x - delta_x;
        int p = px + py + delta_x;
        int x = sx, y = sy;

        for (size_t i = 0; i <= delta_x; ++i)
        {
            SDL_RenderDrawPoint(ctx, x, y);

            const int decision = ~(p >> 31);
            p += px + (py & decision);
            ++x;
            y += sign_delta_y & decision;
        }
    }
    /* vertical sweep */
    else
    {
        const byte_t y0_min = Y0 <= Y1;
        const int sx = y0_min * X0 + !y0_min * X1;
        const int sy = y0_min * Y0 + !y0_min * Y1;
        const int dx = y0_min * X1 + !y0_min * X0;
        const int dy = y0_min * Y1 + !y0_min * Y0;
        const int delta_x = dx - sx, delta_y = dy - sy;
        const int sign_delta_x = SIGN(dx - sx);
        const int px = -delta_y - delta_y;
        const int py = sign_delta_x * (delta_x + delta_x);
        int p = px + py + delta_y;
        int x = sx, y = sy;

        for (size_t i = 0; i <= delta_y; ++i)
        {
            SDL_RenderDrawPoint(ctx, x, y);

            const int decision = ~(p >> 31);
            p += (px & decision) + py;
            x += sign_delta_x & decision;
            ++y;
        }
    }

    SDL_SetRenderDrawColor(ctx, r, g, b, a);
}

void
FillText
( SDL_Renderer* ctx,
  byte_t* str,
  int x,
  int y,
  int scale,
  int color )
{
    const size_t spacing = scale * LETTER_SPACING;
    int px_x = x, px_y = y, g_x = px_x;

    SDL_SetRenderDrawColor(ctx, (color & 0xff000000) >> 24,
                                (color & 0xff0000) >> 16,
                                (color & 0xff00) >> 8,
                                0xff);

    for (size_t i = 0; *(str + i); ++i)
    {
        /* render glyph */
        byte_t chr = *(str + i);
        const byte_t* glyph = *(GLYPH_TABLE + chr);
        size_t offset = 0;

        if (chr != 32)
        {
            for (size_t r = 0; r < FONT_HEIGHT; ++r)
            {
                px_x = g_x;

                for (size_t c = 0; c < FONT_WIDTH; ++c)
                {
                    byte_t px = *(glyph + offset++);

                    if (px)
                    {
                        SDL_Rect screen_rect = { px_x, px_y, scale, scale };
                        SDL_RenderFillRect(ctx, &screen_rect);
                    }

                    px_x += scale;
                }

                px_y += scale;
            }
        }
        else
        {
            px_x += FONT_WIDTH * scale;
        }

        px_x += spacing;
        px_y = y;
        g_x = px_x;
    }
}

void DrawGrid (SDL_Renderer* ctx)
{
    const size_t rows = WIN_H >> GRID_SIZE, cols = BUFFER_W >> GRID_SIZE;
    size_t x = 0, y = 0;
    SDL_SetRenderDrawColor(ctx, 127, 127, 127, 255);

    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t c = 0; c < cols; ++c)
        {
            SDL_RenderDrawPoint(ctx, x, y);
            x += GRID;
        }

        x = 0;
        y += GRID;
    }
}

void DrawFrustum (SDL_Renderer* ctx)
{
    DrawLineBresenham(ctx,                      //
                      0, PROJ_PLANE_Y,          // projection plane
                      BUFFER_W, PROJ_PLANE_Y,   //
                      0xffffffff);
    DrawLineBresenham(ctx,                      //
                      0, PROJ_PLANE_Y,          // left "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(ctx,                      //
                      BUFFER_W, PROJ_PLANE_Y,   // right "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(ctx,                      //
                      BUFFER_W_2, PROJ_PLANE_Y, // near distance
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
}

void DrawAxes (SDL_Renderer* ctx)
{
    const size_t ox = GRID, oy = WIN_H - GRID;
    const size_t x = ox + GRID, z = oy - GRID;
    DrawLineBresenham(ctx, ox, oy, x, oy, 0xffffffff);                // x-
    DrawLineBresenham(ctx, x + 5, oy - 2, x + 9, oy + 2, 0xffffffff); // axis
    DrawLineBresenham(ctx, x + 9, oy - 2, x + 5, oy + 2, 0xffffffff); //

    DrawLineBresenham(ctx, ox, oy, ox, z, 0xffffffff);                //
    DrawLineBresenham(ctx, ox - 2, z - 9, ox + 2, z - 9, 0xffffffff); // z-
    DrawLineBresenham(ctx, ox + 2, z - 9, ox - 2, z - 5, 0xffffffff); // axis
    DrawLineBresenham(ctx, ox - 2, z - 5, ox + 2, z - 5, 0xffffffff); //
}

void DrawSpan (SDL_Renderer* ctx, span_t* span)
{
    /* fill out the "S-buffer representation" */
    const int screen_x0 = ceil(span->x0 - 0.5);
    const int screen_width = ceil(span->x1 - 0.5) - screen_x0;
    SDL_Rect screen_rect = { screen_x0, WIN_H, screen_width, S_BUFFER_REPR_H };
    SDL_SetRenderDrawColor(ctx,
                           (span->color & 0xff000000) >> 24,
                           (span->color & 0xff0000) >> 16,
                           (span->color & 0xff00) >> 8,
                           0xff);
    SDL_RenderFillRect(ctx, &screen_rect);

    /* draw the segment in "screen space", i.e., onto the projection plane */
    screen_rect.y = PROJ_PLANE_Y; screen_rect.h = 1;
    SDL_RenderFillRect(ctx, &screen_rect);
}

size_t
DrawSBufferDfs
( SDL_Renderer* ctx,
  sbuffer_t* sbuffer,
  void (*drawhook) (SDL_Renderer* ctx, span_t* span) )
{
    /* draw the background for the "S-Buffer representation" */
    SDL_Rect sbuffer_rect = { 0, WIN_H, BUFFER_W, S_BUFFER_REPR_H };
    SDL_SetRenderDrawColor(ctx, 255, 255, 255, 255);
    SDL_RenderFillRect(ctx, &sbuffer_rect);

    span_t* curr = sbuffer->root;
    dfs_t stack[sbuffer->max_depth + 1];
    int sp = 0;
    size_t count = 0;
    memset(stack, 0, sizeof(stack));

    while (curr)
    {
        span_t* parent;

        if ((stack + sp)->prev_visited)
        {
            if ((stack + sp)->next_visited)
            {
                if (sp-- > 0)
                {
                    dfs_t* grandparent = stack + sp;

                    if (curr->x0 < grandparent->span->x0)
                        grandparent->prev_visited = 0xff;
                    else
                        grandparent->next_visited = 0xff;

                    curr = grandparent->span;
                }
                else
                {
                    curr = 0;
                }
            }
            else
            {
                parent = curr;
                curr = parent->next;
                (stack + ++sp)->prev_visited = 0;
                (stack + sp)->next_visited = 0;
                drawhook(ctx, parent);
                ++count;
            }
        }
        else
        {
            while (curr)
            {
                parent = curr;
                curr = parent->prev;
                (stack + sp)->span = parent;
                (stack + sp)->prev_visited = 0;
                (stack + sp++)->next_visited = 0;
            }

            curr = parent->next; // try the `next` sub-tree
            (stack + sp - 1)->prev_visited = 0xff;
            drawhook(ctx, parent);
            ++count;

            if (curr)
            {                                   // have to reset the "next"
                (stack + sp)->prev_visited = 0; // position in the stack as the
                (stack + sp)->next_visited = 0; // next iteration relies on its
            }                                   // up-to-date value
        }

        if (!curr && --sp > 0) // reached a leaf node, need to backtrack
        {
            dfs_t* grandparent = stack + --sp;

            if (parent->x0 < grandparent->span->x0)
                grandparent->prev_visited = 0xff;
            else
                grandparent->next_visited = 0xff;

            curr = grandparent->span;
        }
    }

    return count;
}

void
DrawSegments
( SDL_Renderer* ctx,
  byte_t*       ks,
  seg2_t*       segs,
  size_t        seg_head )
{
    /* go over all segments stored in the queue */
    for (size_t i = 0; i < seg_head; ++i)
    {
        /* draw the segment in world space */
        const seg2_t seg = *(segs + i);
        const vec2_t src = seg.src;
        const vec2_t dst = seg.dst;
        const int color = seg.color;
        DrawLineBresenham(ctx, src.x, src.y, dst.x, dst.y, color);

        /* render projective debug lines */
        if (*(ks + KEY_D))
        {
            DrawLineBresenham(ctx, src.x, src.y, BUFFER_W_2, WIN_H, color);
            DrawLineBresenham(ctx, dst.x, dst.y, BUFFER_W_2, WIN_H, color);
        }
    }
}

int Setup (SDL_Window** window, SDL_Renderer** ctx)
{
    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (res)
    {
        printf("[Setup] Initializing SDL failed: %s.\n", SDL_GetError());
        Destroy(0, 0);

        return 1;
    }

    *window = CreateWindow(WIN_TITLE,
                           SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED,
                           BUFFER_W, BUFFER_H,
                           0);
    if (!window)
    {
        Destroy(*window, 0);

        return 1;
    }

    *ctx = CreateCtx(*window);
    if (!ctx)
    {
        Destroy(*window, *ctx);

        return 1;
    }

    srand(time(0)); // random seed

    SDL_version sdl_version;
    SDL_RendererInfo info;

    SDL_GetVersion(&sdl_version);
    SDL_GetRendererInfo(*ctx, &info);
    printf("[SDL_Init] Done (v%d.%d.%d, renderer: %s) \n",
           sdl_version.major, sdl_version.minor, sdl_version.patch,
           info.name);

    return 0;
}

void
Update
( sbuffer_t*     sbuffer,
  SDL_Window*    window,
  SDL_Renderer*  ctx,
  byte_t*        do_run,
  mouse_state_t* ms,
  byte_t*        ks,
  seg2_t*        segs,
  size_t*        seg_head,
  double*        push_time_millis,
  int*           disappear_ticks )
{
    size_t span_count;
    size_t head = *seg_head;
    vec2_t dst;
    byte_t prev_holding_mouse_button = ms->pressed == SDL_BUTTON_LEFT;
    byte_t holding_esc;
    byte_t did_push = 0;
    HandleEventSync(do_run, ms, ks);
    DrawGrid(ctx);
    DrawFrustum(ctx);
    DrawAxes(ctx);
    DrawSegments(ctx, ks, segs, *seg_head);
    span_count = DrawSBufferDfs(ctx, sbuffer, DrawSpan);
    holding_esc = *(ks + KEY_ESC);

    /* left mouse button is being held down */
    if (ms->pressed == SDL_BUTTON_LEFT)
    {
        vec2_t src = { ms->x, ms->y };

        GetMouseCoords(&(dst.x), &(dst.y));
        src.x = CLAMP(src.x, 0, BUFFER_W);
        src.y = CLAMP(src.y, 0, PROJ_PLANE_Y);
        dst.x = CLAMP(dst.x, 0, BUFFER_W);
        dst.y = CLAMP(dst.y, 0, PROJ_PLANE_Y);
        DrawLineBresenham(ctx, src.x, src.y, dst.x, dst.y, 0xff0000ff);
    }
    /* left mouse button is released */
    else if (!holding_esc && prev_holding_mouse_button && !ms->pressed)
    {
        did_push = 0xff;
        vec2_t src = { ms->x, ms->y };
        seg2_t seg = { src, dst, RandomColor() };

        GetMouseCoords(&(dst.x), &(dst.y));
        seg.src.x = CLAMP(seg.src.x, 0, BUFFER_W);
        seg.src.y = CLAMP(seg.src.y, 0, PROJ_PLANE_Y);
        seg.dst.x = CLAMP(seg.dst.x, 0, BUFFER_W);
        seg.dst.y = CLAMP(seg.dst.y, 0, PROJ_PLANE_Y);
        *(segs + head++) = seg; // store in the world space segments list

        /* store the segment in the s-buffer for any potential clipping to take
         * place appropriately
         */
        float screen_src, screen_dst;
        ToScreenSpace(&seg.src, &screen_src);
        ToScreenSpace(&seg.dst, &screen_dst);
        /* sort the endpoints in ascending screen space x before pushing the
         * segment onto the buffer
         */
        float screen_x0, screen_x1, screen_w0, screen_w1;
        const byte_t src_min = screen_src <= screen_dst;
        screen_x0 = src_min * screen_src + !src_min * screen_dst;
        screen_w0 = src_min * ZToScreenSpace(seg.src.y) +
                    !src_min * ZToScreenSpace(seg.dst.y);
        screen_x1 = src_min * screen_dst + !src_min * screen_src;
        screen_w1 = src_min * ZToScreenSpace(seg.dst.y) +
                    !src_min * ZToScreenSpace(seg.src.y);

#ifdef DEBUG
        printf("{ { %d, %d }, { %d, %d }, %d }\n",
                seg.src.x, seg.src.y, seg.dst.x, seg.dst.y, seg.color);
#endif // DEBUG

        struct timespec start, end;

        timespec_get(&start, TIME_UTC);

        SB_Push(sbuffer,
                screen_x0, screen_x1,
                screen_w0, screen_w1,
                ID++,
                seg.color);

        timespec_get(&end, TIME_UTC);

        *push_time_millis = (end.tv_sec - start.tv_sec +
                             (end.tv_nsec - start.tv_nsec) / 1e9f) * 1e3f;
    }

    /* print debug statistics */
    const size_t memused = span_count * SPAN_SIZE + SBUFFER_SIZE;
    byte_t strbuf[100];
    sprintf(strbuf, "s-buffer memory: %lu bytes used (%.0f%%)",
            memused, round((float) memused / 32.0f));
    FillText(ctx, strbuf, 16, 16, 2, 0xff0000ff);
    sprintf(strbuf, "span count     : %lu", span_count);
    FillText(ctx, strbuf, 16, 32, 2, 0xff0000ff);
    sprintf(strbuf,
            "buffer depth   : %d",
            sbuffer->root ? sbuffer->root->height : 0);
    FillText(ctx, strbuf, 16, 48, 2, 0xff0000ff);

    if (did_push) *disappear_ticks = 250;

    if (*disappear_ticks > 0)
    {
        sprintf(strbuf, "push took      : %.3f ms", *push_time_millis);
        FillText(ctx, strbuf, 16, 64, 2, 0xff0000ff);
        *disappear_ticks = *disappear_ticks - 1;
    }
    else
    {
        *push_time_millis = 0;
    }

    *seg_head = head;
    FrameFlush(ctx);
}

/* WARNING: For debugging use only! */
void Prepopulate (sbuffer_t* sbuffer, seg2_t* segs, size_t* seg_head)
{
    /* [✅] FP causing infinitely pushing issue */
    // const size_t prepop_segs_count = 7;
    // seg2_t prepop_segs[] = {
    //     { { 176, 144 }, { 352, 464 }, 0x00ff00ff }, // A
    //     { { 496, 464 }, { 672, 128 }, 0x00ff00ff }, // B
    //     { { 352, 464 }, { 496, 464 }, 0x00ff00ff }, // C
    //     { { 144, 256 }, { 432, 496 }, 0x00ff00ff }, // D
    //     { { 432, 496 }, { 656, 272 }, 0x00ff00ff }, // E
    //     ////////////////////////////////////////////////
    //     { { 64, 448 }, { 688, 608 }, 0x00ff00ff },  // F
    //     { { 112, 592 }, { 208, 464 }, 0x00ff00ff }  // G
    // };

    /* [✅] FP precision causing DFS infinite loop issue */
    // const size_t prepop_segs_count = 11;
    // seg2_t prepop_segs[] = {
    //     { { 0, 624 }, { 800, 624 }, 0x00ff00ff },   // A
    //     { { 16, 640 }, { 80, 624 }, 0x00ff00ff },   // B
    //     { { 112, 640 }, { 192, 640 }, 0x00ff00ff }, // C
    //     { { 272, 640 }, { 336, 640 }, 0x00ff00ff }, // D
    //     { { 400, 640 }, { 496, 640 }, 0x00ff00ff }, // E
    //     ////////////////////////////////////////////////
    //     { { 528, 624 }, { 576, 608 }, 0x00ff00ff }, // F
    //     { { 576, 608 }, { 592, 624 }, 0x00ff00ff }, // G
    //     { { 560, 608 }, { 592, 608 }, 0x00ff00ff }, // H
    //     ////////////////////////////////////////////////
    //     { { 624, 640 }, { 688, 640 }, 0x00ff00ff }, // I
    //     { { 736, 640 }, { 784, 640 }, 0x00ff00ff }, // J
    //     { { 0, 640 }, { 800, 640 }, 0x00ff00ff }    // K
    // };

    /* [✅] DFS span skipping issue */
    // const size_t prepop_segs_count = 10;
    // seg2_t prepop_segs[] = {
    //     { { 80, 128 }, { 80, 608 }, 0x00ff00ff },   // A
    //     { { 32, 384 }, { 128, 448 }, 0x00ff00ff },  // B
    //     { { 32, 224 }, { 128, 288 }, 0x00ff00ff },  // C
    //     { { 32, 128 }, { 128, 192 }, 0x00ff00ff },  // D
    //     { { 64, 64 }, { 128, 192 }, 0x00ff00ff },   // E
    //     { { 128, 192 }, { 128, 288 }, 0xff0000ff }, // F
    //     { { 128, 288 }, { 128, 448 }, 0x00ff00ff }, // G
    //     { { 32, 592 }, { 128, 656 }, 0x00ff00ff },  // H
    //     { { 80, 608 }, { 80, 688 }, 0x00ff00ff },   // I
    //     { { 128, 448 }, { 128, 656 }, 0x00ff00ff }  // J
    // };

    /* [✅] FP precision causing z-fighting issue */
    // const size_t prepop_segs_count = 4;
    // seg2_t prepop_segs[] = {
    //     { { 272, 208 }, { 592, 208 }, 0xff0000ff }, // A
    //     { { 416, 176 }, { 464, 240 }, 0x00ff00ff }, // B
    //     { { 336, 176 }, { 464, 240 }, 0x0000ffff }, // C
    //     { { 320, 176 }, { 464, 240 }, 0xffff00ff }  // D
    // };

    /* [✅] Improper height issue */
    // const size_t prepop_segs_count = 34;
    // seg2_t prepop_segs[] = {
    //     { { 272, 96 }, { 640, 112 }, 1953937151 },
    //     { { 464, 224 }, { 608, 64 }, 1375729407 },
    //     { { 688, 208 }, { 320, 112 }, 214372095 },
    //     { { 736, 224 }, { 448, 304 }, 1401573631 },
    //     { { 736, 176 }, { 720, 496 }, 273654527 },
    //     { { 480, 432 }, { 752, 496 }, 655507711 },
    //     { { 768, 368 }, { 528, 432 }, 1435949823 },
    //     { { 320, 448 }, { 528, 432 }, 2066536959 },
    //     { { 352, 480 }, { 656, 464 }, 207573759 },
    //     { { 704, 576 }, { 640, 464 }, 526388223 },
    //     { { 224, 320 }, { 400, 496 }, 1649212927 },
    //     { { 96, 336 }, { 640, 288 }, 1706496767 },
    //     { { 112, 448 }, { 752, 544 }, 150500351 },
    //     { { 496, 592 }, { 752, 544 }, 2025167615 },
    //     { { 512, 576 }, { 704, 576 }, 1712488959 },
    //     { { 288, 544 }, { 768, 560 }, 1202311935 },
    //     { { 288, 544 }, { 496, 592 }, 1245862143 },
    //     { { 496, 592 }, { 752, 592 }, 373391359 },
    //     { { 752, 592 }, { 768, 560 }, 135498495 },
    //     { { 768, 560 }, { 768, 368 }, 2128905727 },
    //     { { 768, 368 }, { 752, 496 }, 1522140671 },
    //     { { 768, 368 }, { 736, 176 }, 1913940991 },
    //     { { 736, 176 }, { 640, 112 }, 1225466111 },
    //     { { 128, 512 }, { 288, 544 }, 1957625855 },
    //     { { 336, 576 }, { 432, 560 }, 1709175039 },
    //     { { 432, 560 }, { 448, 592 }, 620281343 },
    //     { { 464, 576 }, { 480, 608 }, 821532671 },
    //     { { 608, 576 }, { 576, 608 }, 2146977791 },
    //     { { 624, 576 }, { 656, 608 }, 302804991 },
    //     { { 704, 576 }, { 720, 608 }, 655896319 },
    //     { { 736, 576 }, { 720, 656 }, 1048814335 },
    //     { { 736, 560 }, { 752, 608 }, 642828287 },
    //     { { 736, 560 }, { 784, 688 }, 2028058879 },
    //     { { 736, 656 }, { 752, 672 }, 1522211071 }
    // };

    /* [✅] Improper balancing + height issue */
    const size_t prepop_segs_count = 38;
    // const size_t prepop_segs_count = 11;
    seg2_t prepop_segs[] = {
        { { 128, 192 }, { 512, 176 }, 1999622655 },
        { { 512, 160 }, { 704, 304 }, 222632191 },
        { { 112, 160 }, { 224, 384 }, 126026751 },
        { { 224, 368 }, { 528, 256 }, 1738702591 },
        { { 480, 208 }, { 576, 272 }, 1904414463 },
        { { 480, 288 }, { 560, 256 }, 417136639 },
        { { 368, 272 }, { 464, 336 }, 1968892415 },
        { { 272, 320 }, { 368, 336 }, 1049337087 },
        { { 352, 320 }, { 336, 352 }, 802506495 },
        { { 400, 320 }, { 480, 304 }, 503319551 },
        { { 448, 256 }, { 544, 304 }, 738634239 },
        { { 656, 224 }, { 560, 336 }, 71643135 },
        { { 464, 304 }, { 592, 320 }, 2027184639 },
        { { 272, 336 }, { 272, 368 }, 1895877887 },
        { { 96, 512 }, { 768, 432 }, 653422591 },
        { { 592, 432 }, { 336, 528 }, 1980339711 },
        { { 208, 480 }, { 256, 528 }, 1309147903 },
        { { 112, 560 }, { 496, 592 }, 690147839 },
        { { 624, 512 }, { 336, 608 }, 612906239 },
        { { 480, 544 }, { 544, 576 }, 1832711423 },
        { { 256, 544 }, { 320, 592 }, 1050939391 },
        { { 416, 560 }, { 480, 576 }, 848152063 },
        { { 448, 576 }, { 464, 608 }, 1120121343 },
        { { 480, 576 }, { 480, 608 }, 413118207 },
        { { 352, 560 }, { 352, 592 }, 1484700671 },
        { { 192, 544 }, { 240, 576 }, 1190558207 },
        { { 112, 608 }, { 592, 624 }, 726333183 },
        { { 432, 608 }, { 480, 640 }, 361439999 },
        { { 480, 624 }, { 448, 640 }, 655218943 },
        { { 560, 608 }, { 560, 640 }, 98926847 },
        { { 224, 608 }, { 272, 640 }, 1648942847 },
        { { 160, 608 }, { 208, 624 }, 669363967 },
        { { 240, 624 }, { 304, 624 }, 1344599551 },
        { { 160, 624 }, { 224, 624 }, 2097489919 },
        { { 128, 592 }, { 176, 624 }, 423063039 },
        { { 176, 624 }, { 192, 640 }, 1621593343 },
        { { 256, 608 }, { 336, 656 }, 637184767 },
        { { 416, 608 }, { 416, 640 }, 2039061247 }
        //
        // { { 368, 80 }, { 576, 80 }, 2019175679 },
        // { { 368, 176 }, { 576, 64 }, 63840255 },
        // { { 192, 144 }, { 592, 192 }, 927213823 },
        // { { 384, 96 }, { 512, 192 }, 545370623 },
        // { { 384, 208 }, { 512, 160 }, 287587839 },
        // { { 272, 144 }, { 416, 176 }, 1072269055 },
        // { { 272, 96 }, { 272, 192 }, 1410547455 },
        // { { 192, 112 }, { 288, 160 }, 1208276223 },
        // { { 352, 192 }, { 448, 208 }, 1642625279 },
        // { { 432, 176 }, { 448, 208 }, 861744895 },
        // { { 416, 192 }, { 432, 224 }, 1148353279 }
    };

    /* [✅] Improper balancing issue:
     * Turns out not an actual issue, just early verification following parent
     * bisection
     */
    // const size_t prepop_segs_count = 5;
    // seg2_t prepop_segs[] = {
    //     { { 336, 128 }, { 608, 144 }, 2034110975 },
    //     { { 432, 160 }, { 688, 144 }, 1804861183 },
    //     { { 496, 272 }, { 672, 128 }, 1667428351 },
    //     { { 480, 192 }, { 608, 272 }, 1240514047 },
    //     { { 496, 240 }, { 528, 256 }, 1603257599 }
    // };

    /* [✅] Another case of FP precision causing DFS infinite loop issue */
    // const size_t prepop_segs_count = 35;
    // seg2_t prepop_segs[] = {
    //     { { 160, 176 }, { 272, 144 }, 1879746303 },
    //     { { 384, 112 }, { 464, 128 }, 1986943487 },
    //     { { 384, 128 }, { 592, 144 }, 745789951 },
    //     { { 672, 192 }, { 288, 144 }, 1889099519 },
    //     { { 192, 240 }, { 464, 176 }, 1104709375 },
    //     { { 288, 176 }, { 384, 176 }, 383436543 },
    //     { { 272, 256 }, { 416, 240 }, 416783359 },
    //     { { 128, 240 }, { 240, 176 }, 204895999 },
    //     { { 96, 304 }, { 352, 272 }, 1080072191 },
    //     { { 160, 320 }, { 240, 240 }, 2021432063 },
    //     { { 176, 304 }, { 288, 288 }, 844059903 },
    //     { { 416, 320 }, { 272, 272 }, 104197631 },
    //     { { 256, 304 }, { 400, 288 }, 1119038207 },
    //     { { 448, 288 }, { 320, 304 }, 123848191 },
    //     { { 352, 320 }, { 416, 288 }, 2870271 },
    //     { { 528, 160 }, { 688, 176 }, 830335999 },
    //     { { 608, 192 }, { 576, 256 }, 638102527 },
    //     { { 704, 208 }, { 544, 208 }, 250467583 },
    //     { { 528, 160 }, { 624, 224 }, 2036658175 }, // <- (i=18, remaining=6.27450562, 3rd iter.)
    //     { { 688, 240 }, { 512, 224 }, 1259120127 },
    //     { { 688, 144 }, { 432, 288 }, 377375487 },
    //     { { 352, 240 }, { 560, 256 }, 570935039 },
    //     { { 672, 352 }, { 0, 256 }, 816266239 },
    //     { { 80, 304 }, { 704, 336 }, 1512013311 },
    //     { { 208, 336 }, { 544, 304 }, 1734740991 },
    //     { { 704, 368 }, { 400, 320 }, 299311103 },
    //     { { 384, 384 }, { 528, 288 }, 1315255295 },
    //     { { 304, 320 }, { 368, 320 }, 853548031 },
    //     { { 240, 288 }, { 432, 352 }, 2120175359 },
    //     { { 624, 384 }, { 336, 368 }, 1034504447 },
    //     { { 64, 368 }, { 464, 368 }, 670716927 },
    //     { { 464, 384 }, { 336, 352 }, 2033304831 },
    //     { { 192, 368 }, { 512, 384 }, 1578942975 },
    //     { { 560, 400 }, { 448, 368 }, 1753826559 }, // <- (i=33, infinite loop-inducing scheiße)
    //     { { 430, 704 }, { 438, 704 }, 4278190335 }  // <- (i=34, remaining=5.56756592, `bookmark_index` shenanigans)
    // };

    for (int i = 0; i < prepop_segs_count; ++i)
    {
        const seg2_t seg = *(prepop_segs + i);
        *(segs + i) = seg;
        float screen_src, screen_dst;
        ToScreenSpace(&seg.src, &screen_src);
        ToScreenSpace(&seg.dst, &screen_dst);
        float screen_x0, screen_x1, screen_w0, screen_w1;
        const byte_t src_min = screen_src <= screen_dst;

        /* sort the endpoints in ascending screen space x before pushing the
         * segment onto the buffer
         */
        screen_x0 = src_min * screen_src + !src_min * screen_dst;
        screen_w0 = src_min * ZToScreenSpace(seg.src.y) +
                    !src_min * ZToScreenSpace(seg.dst.y);
        screen_x1 = src_min * screen_dst + !src_min * screen_src;
        screen_w1 = src_min * ZToScreenSpace(seg.dst.y) +
                    !src_min * ZToScreenSpace(seg.src.y);

        SB_Push(sbuffer,
                screen_x0, screen_x1,
                screen_w0, screen_w1,
                ID++,
                seg.color);
    }

    *seg_head = prepop_segs_count;
}

int main (int argc, char** argv)
{
    SDL_Window* window = 0; // window
    SDL_Renderer* ctx = 0;  // rendering context

    int res = Setup(&window, &ctx);
    if (res) return res;

    mouse_state_t ms = { 0, 0, 0 }; // mouse state
    byte_t ks[128] = { 0 };         // key state
    byte_t do_run = 0xff;           // flag to keep the program running

    /* store segment endpoints */
    size_t seg_head = 0;
    seg2_t segs[MAX_SEGS];

    // s-buffer itself
    sbuffer_t* sbuffer = SB_Init(BUFFER_W, Z_NEAR, S_BUFFER_MAX_DEPTH);

    /* prepopulate if the optional argument `-p` is passed */
    if (argc > 1)
    {
        char* arg = *(argv + 1);

        if (*arg == '-' &&
            *(arg + 1) == 'p' &&
            *(arg + 2) == 'p' &&
            *(arg + 3) == '\0')
            Prepopulate(sbuffer, segs, &seg_head);
    }

    int disappear_ticks = 0; // timeout before we remove the 'push took' message
    double push_time_millis = 0; // how long the latest push took

    /* main loop */
    while (do_run)
        Update(sbuffer,
               window,
               ctx,
               &do_run,
               &ms,
               ks,
               segs,
               &seg_head,
               &push_time_millis,
               &disappear_ticks);

    printf("--------------------------[ SB_Dump ]--------------------------\n");
    SB_Dump(sbuffer);
    printf("---------------------------------------------------------------\n");

    SB_Destroy(sbuffer);
    Destroy(window, ctx);
    printf("Goodbye! 🙋‍♂️\n");

    return 0;
}
