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
#include <string.h>
#include <math.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "demodef.h"
#include "prepop.h"
#define S_BUFFER_DEFS_ONLY
#include "s_buffer.h"

#define SIGN(x) (((x) >> 31 << 1) + 1)
#define MIN(a, b) ((((a) <= (b)) * (a)) + (((b) < (a)) * (b)))
#define CLAMP(x, lo, hi) (MIN(SB_MAX(x, lo), hi))
#define PX(x, y) (BUFFER_W * (y) + (x))

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

#define LETTER_SPACING 1

const size_t BUFFER_W_2 = BUFFER_W >> 1;
const size_t GRID = 1 << GRID_SIZE;
byte_t ID = 65;

// buffer the frame before it's actually drawn to the screen
color_t framebuffer[BUFFER_W * BUFFER_H] = { 0 };
const size_t N_PIXELS = BUFFER_W * BUFFER_H;

const size_t FRAMEBUFFER_SIZE = sizeof(framebuffer);
const size_t SBUFFER_SIZE = sizeof(sbuffer_t);
const size_t SPAN_SIZE = sizeof(span_t);

size_t AToLL (const char* str) // bastardized version of `stdlib`s `atoll`,
{                              // supports positive integers
    const size_t ZERO = 48;
    const char* curr = str;
    size_t len = 0, out = 0, k = 1;

    while (*curr) curr = str + ++len;

    --curr;

    for (size_t i = 0; i < len; ++i)
    {
        size_t digit = *curr - ZERO;
        out += digit * k;
        --curr;
        k *= 10;
    }

    return out;
}

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
        fprintf(stderr,
                "[CreateWindow] Could not create window!: %s\n",
                SDL_GetError());

        return 0;
    }

    return window;
}

SDL_Renderer* CreateCtx (SDL_Window* window, Uint32 flags)
{
    SDL_Renderer* ctx = SDL_CreateRenderer(window, -1, flags);

    if (!ctx)
    {
        fprintf(stderr,
                "[CreateCtx] Error while creating a rendering context: %s\n",
                SDL_GetError());

        return 0;
    }

    return ctx;
}

void Destroy (SDL_Window* window, SDL_Renderer* ctx, SDL_Texture* frame)
{
    if (ctx) SDL_DestroyRenderer(ctx);
    if (window) SDL_DestroyWindow(window);
    if (frame) SDL_DestroyTexture(frame);
    SDL_Quit();
}

void FrameFlush (SDL_Renderer* ctx, SDL_Texture* frame)
{
    void* pixels; // raw pointer to current frame's pixel data
    int width;    // how wide a row of pixels is in bytes
    SDL_LockTexture(frame, 0, &pixels, &width);
    memcpy(pixels, framebuffer, FRAMEBUFFER_SIZE);
    memset(framebuffer, 0, FRAMEBUFFER_SIZE);
    SDL_UnlockTexture(frame);
    SDL_SetRenderTarget(ctx, 0); // set the `window` as the render target
    SDL_SetRenderDrawColor(ctx, 0, 0, 0, 0);             // set a clean slate
    SDL_SetRenderDrawBlendMode(ctx, SDL_BLENDMODE_NONE); // for the new frame we
    SDL_RenderClear(ctx);                                // are about to render
    SDL_RenderCopy(ctx, frame, 0, 0); // copy the frame to the `window`
    SDL_RenderPresent(ctx);           // present the frame to the screen
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

color_t RandomColor ()
{
    return (rand() & 0xff000000) |
           (rand() & 0xff0000) |
           (rand() & 0xff00) |
           0xff;
}

//
// PutPixel
// Put a single pixel at the designated location on the framebuffer with smooth
// alpha blending
//
void PutPixel (int x, int y, color_t color)
{
    const int offset = PX(x, y);
    if (!(0 <= offset && offset < N_PIXELS)) return; // bounds check
    color_t* px = framebuffer + offset;
    const color_t buf_color = *px;
    const byte_t r = (color >> 24) & 0xff, buf_r = (buf_color >> 24) & 0xff;
    const byte_t g = (color >> 16) & 0xff, buf_g = (buf_color >> 16) & 0xff;
    const byte_t b = (color >> 8) & 0xff,  buf_b = (buf_color >> 8) & 0xff;
    const byte_t a = color & 0xff;
    const float blend = (float) a / 255, blend_inv = 1 - blend;
    const byte_t new_r = r * blend + buf_r * blend_inv;
    const byte_t new_g = g * blend + buf_g * blend_inv;
    const byte_t new_b = b * blend + buf_b * blend_inv;
    *px = (new_r << 24) | (new_g << 16) | (new_b << 8) | 0xff;
}

//
// DrawLineBresenham
// Why not use `SDL_RenderDrawLine` you degenerate slime!
//
void DrawLineBresenham (float x0, float y0, float x1, float y1, color_t color)
{
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
            PutPixel(x, y, color);

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
            PutPixel(x, y, color);

            const int decision = ~(p >> 31);
            p += (px & decision) + py;
            x += sign_delta_x & decision;
            ++y;
        }
    }
}

void FillRect (int x, int y, int width, int height, color_t color)
{
    const int clipleft = SB_MAX(-x, 0);
    const int clipright = SB_MAX(x + width - BUFFER_W, 0);
    const int cliptop = SB_MAX(-y, 0);
    const int clipbottom = SB_MAX(y + height - BUFFER_H, 0);
    const int clipped_w = width - clipleft - clipright;
    const int clipped_h = height - cliptop - clipbottom;
    const int sx = x + clipleft, sy = y + cliptop;
    int xi, yi = sy;

    for (int i = 0; i < height; ++i)
    {
        xi = sx;
        for (int j = 0; j < width; ++j) PutPixel(xi++, yi, color);
        ++yi;
    }
}

void FillText (byte_t* str, int x, int y, int scale, color_t color)
{
    const size_t spacing = scale * LETTER_SPACING;
    int px_x = x, px_y = y, g_x = px_x;

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
                    if (px) FillRect(px_x, px_y, scale, scale, color);
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

void DrawGrid ()
{
    const size_t rows = WIN_H >> GRID_SIZE, cols = BUFFER_W >> GRID_SIZE;
    size_t x = 0, y = 0;

    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t c = 0; c < cols; ++c)
        {
            PutPixel(x, y, 0x7f7f7fff);
            x += GRID;
        }

        x = 0;
        y += GRID;
    }
}

void DrawFrustum ()
{
    DrawLineBresenham(0, PROJ_PLANE_Y,          // projection plane
                      BUFFER_W, PROJ_PLANE_Y,   //
                      0xffffffff);
    DrawLineBresenham(0, PROJ_PLANE_Y,          // left "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(BUFFER_W, PROJ_PLANE_Y,   // right "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(BUFFER_W_2, PROJ_PLANE_Y, // near distance
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
}

void DrawAxes ()
{
    const size_t ox = GRID, oy = WIN_H - GRID;
    const size_t x = ox + GRID, z = oy - GRID;
    DrawLineBresenham(ox, oy, x, oy, 0xffffffff);                //
    DrawLineBresenham(x + 5, oy - 2, x + 9, oy + 2, 0xffffffff); // x-axis
    DrawLineBresenham(x + 9, oy - 2, x + 5, oy + 2, 0xffffffff); //

    DrawLineBresenham(ox, oy, ox, z, 0xffffffff);                //
    DrawLineBresenham(ox - 2, z - 9, ox + 2, z - 9, 0xffffffff); // z-axis
    DrawLineBresenham(ox + 2, z - 9, ox - 2, z - 5, 0xffffffff); //
    DrawLineBresenham(ox - 2, z - 5, ox + 2, z - 5, 0xffffffff); //
}

void DrawSpan (const span_t* span)
{
    /* fill out the "S-buffer representation" */
    const int screen_x0 = ceil(span->x0 - 0.5);
    const int screen_width = ceil(span->x1 - 0.5) - screen_x0;
    FillRect(screen_x0, WIN_H, screen_width, S_BUFFER_REPR_H, span->color);

    // draw the segment in "screen space", i.e., onto the projection plane
    FillRect(screen_x0, PROJ_PLANE_Y, screen_width, 1, span->color);
}

size_t
DrawSBufferDfs
( sbuffer_t* sbuffer,
  void       (*drawhook) (const span_t* span) )
{
    // draw the background for the "S-Buffer representation"
    FillRect(0, WIN_H, BUFFER_W, S_BUFFER_REPR_H, 0xffffffff);

    const size_t size = sbuffer->max_depth + 1;
    const span_t* stack[size];
    byte_t bookmarks[size]; // store "where we left off" for each non-leaf span
    const span_t* curr = sbuffer->root;
    size_t count = 0;
    int sp = 0; // stack pointer
    int cp = 0; // child pointer -> 0: prev, 1: next

    while (curr)
    {
        SB_ASSERT(sp < size, "[DrawSBufferDfs] Max buffer depth reached!\n");

        *(stack + sp) = curr;
        *(bookmarks + sp++) = cp;

        if (!cp && curr->prev)
        {
            curr = curr->prev;
        }
        else if (cp < 2 && curr->next)
        {
            ++count;
            drawhook(curr);

            cp = 0; // reset child pointer as we're about to enter a new subtree
            *(bookmarks + sp - 1) = 1; // update parent's bookmark to `next`
            curr = curr->next;
        }
        /* either we've already visited both children, or we hit a leaf node.
         * either way, walk back one step up the stack.
         */
        else if (--sp > 0)
        {
            if (!curr->next)
            {
                ++count;
                drawhook(curr);
            }

            curr = *(stack + --sp);
            cp = *(bookmarks + sp) + 1; // carry on with the `next` span
        }
        else
        {
            if (!curr->next)    //
            {                   // we need to account for cases where the
                ++count;        // S-Buffer only has a single `prev` child
                drawhook(curr); // and a maximum depth of one
            }                   //

            curr = 0; // exit condition
        }
    }

    return count;
}

void DrawSegments (byte_t* ks, seg2_t* segs, size_t seg_head)
{
    /* go over all segments stored in the queue */
    for (size_t i = 0; i < seg_head; ++i)
    {
        /* draw the segment in world space */
        const seg2_t seg = *(segs + i);
        const vec2_t src = seg.src;
        const vec2_t dst = seg.dst;
        const color_t color = seg.color;
        DrawLineBresenham(src.x, src.y, dst.x, dst.y, color);

        /* render projective debug lines */
        if (*(ks + KEY_D))
        {
            DrawLineBresenham(src.x, src.y, BUFFER_W_2, WIN_H, color);
            DrawLineBresenham(dst.x, dst.y, BUFFER_W_2, WIN_H, color);
        }
    }
}

int Setup (SDL_Window** window, SDL_Renderer** ctx, SDL_Texture** frame)
{
    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (res)
    {
        fprintf(stderr,
                "[Setup] Initializing SDL failed: %s.\n", SDL_GetError());
        Destroy(0, 0, 0);

        return 1;
    }

    *window = CreateWindow(WIN_TITLE,
                           SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED,
                           BUFFER_W, BUFFER_H,
                           0);
    if (!*window)
    {
        Destroy(*window, 0, 0);

        return 1;
    }

    *ctx = CreateCtx(*window, SDL_RENDERER_ACCELERATED);
    if (!*ctx)
    {
        Destroy(*window, *ctx, 0);

        return 1;
    }

    *frame = SDL_CreateTexture(*ctx,
                               SDL_PIXELFORMAT_RGBA8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               BUFFER_W, BUFFER_H);
    if (!*frame)
    {
        fprintf(stderr,
                "[Setup] Error while initializing frame: %s.\n",
                SDL_GetError());

        Destroy(*window, *ctx, *frame);

        return 1;
    }

    srand(time(0)); // random seed

    SDL_version sdl_version;
    SDL_RendererInfo info;

    SDL_GetVersion(&sdl_version);
    SDL_GetRendererInfo(*ctx, &info);
    printf("[SDL_Init] Done (v%d.%d.%d, renderer: %s)\n",
           sdl_version.major, sdl_version.minor, sdl_version.patch,
           info.name);

    return 0;
}

void
Update
( sbuffer_t*     sbuffer,
  SDL_Window*    window,
  SDL_Renderer*  ctx,
  SDL_Texture*   frame,
  byte_t*        do_run,
  mouse_state_t* ms,
  byte_t*        ks,
  seg2_t*        segs,
  size_t*        seg_head,
  size_t*        disappear_ticks )
{
    size_t span_count;
    size_t head = *seg_head;
    vec2_t dst;
    byte_t prev_holding_mouse_button = ms->pressed == SDL_BUTTON_LEFT;
    byte_t holding_esc;
    double push_time_millis = -1; // how long the latest push took
    HandleEventSync(do_run, ms, ks);
    DrawGrid();
    DrawFrustum();
    DrawAxes();
    DrawSegments(ks, segs, *seg_head);
    span_count = DrawSBufferDfs(sbuffer, DrawSpan);
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
        DrawLineBresenham(src.x, src.y, dst.x, dst.y, 0xff0000ff);
    }
    /* left mouse button is released */
    else if (!holding_esc && prev_holding_mouse_button && !ms->pressed)
    {
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

        push_time_millis = (end.tv_sec - start.tv_sec +
                            (end.tv_nsec - start.tv_nsec) / 1e9f) * 1e3f;
    }

    /* print debug statistics */
    const size_t memused = span_count * SPAN_SIZE + SBUFFER_SIZE;
    byte_t strbuf[100];
    sprintf(strbuf, "s-buffer memory: %lu bytes used (%.0f%%)",
            memused, round((float) memused / 32.0f));
    FillText(strbuf, 16, 16, 2, 0xff0000ff);
    sprintf(strbuf, "span count     : %lu", span_count);
    FillText(strbuf, 16, 32, 2, 0xff0000ff);
    sprintf(strbuf,
            "buffer depth   : %d",
            sbuffer->root ? sbuffer->root->height : 0);
    FillText(strbuf, 16, 48, 2, 0xff0000ff);

    if (push_time_millis > 0) *disappear_ticks = 250;

    if (*disappear_ticks)
    {
        sprintf(strbuf, "push took      : %.3f ms", push_time_millis);
        FillText(strbuf, 16, 64, 2, 0xff0000ff);
        --*disappear_ticks;
    }

    *seg_head = head;
    FrameFlush(ctx, frame);
}

/* WARNING: For debugging use only! */
void
Prepopulate
( sbuffer_t* sbuffer,
  size_t     test_case_id,
  seg2_t*    segs,
  size_t*    seg_head )
{
    const test_case_t* tc = TEST_CASES + test_case_id;

    for (int i = 0; i < tc->segs_count; ++i)
    {
        const seg2_t seg = *(tc->segs + i);
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

    *seg_head = tc->segs_count;
}

int main (int argc, char** argv)
{
    SDL_Window* window = 0; // window
    SDL_Renderer* ctx = 0;  // rendering context
    SDL_Texture* frame = 0; // the current frame rendered into a texture

    int res = Setup(&window, &ctx, &frame);
    if (res) return res;

    mouse_state_t ms = { 0, 0, 0 }; // mouse state
    byte_t ks[128] = { 0 };         // key state
    byte_t do_run = 0xff;           // flag to keep the program running

    /* store segment endpoints */
    size_t seg_head = 0;
    seg2_t segs[MAX_SEGS];

    // s-buffer itself
    sbuffer_t* sbuffer = SB_Init(BUFFER_W, Z_NEAR, S_BUFFER_MAX_DEPTH);

    /* prepopulate if the optional argument `-pp` is passed */
    if (argc > 1)
    {
        char* arg = *(argv + 1);
        size_t tcid = 0;

        if (*arg == '-' &&
            *(arg + 1) == 'p' &&
            *(arg + 2) == 'p' &&
            *(arg + 3) == '\0')
        {
            if (argc > 2) tcid = AToLL(*(argv + 2));

            if (tcid >= N_CASES)
            {
                fprintf(stderr,
                        "[sbuffer-demo] Invalid test case id (%lu)\n",
                        tcid);
                exit(1);
            }

            Prepopulate(sbuffer, tcid, segs, &seg_head);
        }
    }

    // remaining ticks before we remove the 'push took' message
    size_t disappear_ticks = 0;

    /* main loop */
    while (do_run)
        Update(sbuffer,
               window,
               ctx,
               frame,
               &do_run,
               &ms,
               ks,
               segs,
               &seg_head,
               &disappear_ticks);

    printf("--------------------------[ SB_Dump ]--------------------------\n");
    SB_Dump(sbuffer);
    printf("---------------------------------------------------------------\n");

    SB_Destroy(sbuffer);
    Destroy(window, ctx, frame);
    printf("Goodbye!\n");

    return 0;
}
