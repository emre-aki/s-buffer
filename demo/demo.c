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

SDL_Renderer* CreateRenderingBuffer (SDL_Window* window)
{
    SDL_Renderer* buffer = SDL_CreateRenderer(window,
                                              -1,
                                              SDL_RENDERER_SOFTWARE);

    if (!buffer)
    {
        printf("[CreateRenderingBuffer] Error while creating a rendering"
               "buffer: %s\n",
               SDL_GetError());

        return 0;
    }

    return buffer;
}

void Destroy (SDL_Window* window, SDL_Renderer* buffer)
{
    if (buffer) SDL_DestroyRenderer(buffer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void FrameFlush (SDL_Renderer* buffer)
{
    SDL_RenderPresent(buffer);
    SDL_SetRenderDrawColor(buffer, 0, 0, 0, 0);
    SDL_RenderClear(buffer);
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
( SDL_Renderer* buffer,
  float x0, float y0,
  float x1, float y1,
  int color )
{
    byte_t r, g, b, a;
    SDL_GetRenderDrawColor(buffer, &r, &g, &b, &a);
    SDL_SetRenderDrawColor(buffer,
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
            SDL_RenderDrawPoint(buffer, x, y);

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
            SDL_RenderDrawPoint(buffer, x, y);

            const int decision = ~(p >> 31);
            p += (px & decision) + py;
            x += sign_delta_x & decision;
            ++y;
        }
    }

    SDL_SetRenderDrawColor(buffer, r, g, b, a);
}

void DrawGrid (SDL_Renderer* buffer)
{
    const size_t rows = WIN_H >> GRID_SIZE, cols = BUFFER_W >> GRID_SIZE;
    size_t x = 0, y = 0;
    SDL_SetRenderDrawColor(buffer, 127, 127, 127, 255);

    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t c = 0; c < cols; ++c)
        {
            SDL_RenderDrawPoint(buffer, x, y);
            x += GRID;
        }

        x = 0;
        y += GRID;
    }
}

void DrawFrustum (SDL_Renderer* buffer)
{
    DrawLineBresenham(buffer,                   //
                      0, PROJ_PLANE_Y,          // projection plane
                      BUFFER_W, PROJ_PLANE_Y,   //
                      0xffffffff);
    DrawLineBresenham(buffer,                   //
                      0, PROJ_PLANE_Y,          // left "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(buffer,                   //
                      BUFFER_W, PROJ_PLANE_Y,   // right "edge"
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
    DrawLineBresenham(buffer,                   //
                      BUFFER_W_2, PROJ_PLANE_Y, // near distance
                      BUFFER_W_2, WIN_H,        //
                      0xffffffff);
}

void DrawAxes (SDL_Renderer* buffer)
{
    const size_t ox = GRID, oy = WIN_H - GRID;
    const size_t x = ox + GRID, z = oy - GRID;
    DrawLineBresenham(buffer, ox, oy, x, oy, 0xffffffff);                // x-
    DrawLineBresenham(buffer, x + 5, oy - 2, x + 9, oy + 2, 0xffffffff); // axis
    DrawLineBresenham(buffer, x + 9, oy - 2, x + 5, oy + 2, 0xffffffff); //

    DrawLineBresenham(buffer, ox, oy, ox, z, 0xffffffff);                //
    DrawLineBresenham(buffer, ox - 2, z - 9, ox + 2, z - 9, 0xffffffff); // z-
    DrawLineBresenham(buffer, ox + 2, z - 9, ox - 2, z - 5, 0xffffffff); // axis
    DrawLineBresenham(buffer, ox - 2, z - 5, ox + 2, z - 5, 0xffffffff); //
}

void DrawSpan (SDL_Renderer* buffer, span_t* span)
{
    /* fill out the "s-buffer representation" */
    const int screen_x0 = ceil(span->x0 - 0.5);
    const int screen_width = ceil(span->x1 - 0.5) - screen_x0;
    SDL_Rect screen_rect = { screen_x0, WIN_H, screen_width, S_BUFFER_REPR_H };
    SDL_SetRenderDrawColor(buffer,
                           (span->color & 0xff000000) >> 24,
                           (span->color & 0xff0000) >> 16,
                           (span->color & 0xff00) >> 8,
                           0xff);
    SDL_RenderFillRect(buffer, &screen_rect);

    /* draw the segment in "screen space", i.e., onto the projection plane */
    DrawLineBresenham(buffer,
                      span->x0, PROJ_PLANE_Y,
                      span->x1, PROJ_PLANE_Y,
                      span->color);
}

size_t
DrawSBufferDfs
( SDL_Renderer* buffer,
  sbuffer_t* sbuffer,
  void (*drawhook) (SDL_Renderer* buffer, span_t* span) )
{
    // s-buffer representation
    SDL_Rect sbuffer_rect = { 0, WIN_H, BUFFER_W, S_BUFFER_REPR_H };
    SDL_SetRenderDrawColor(buffer, 255, 255, 255, 255);
    SDL_RenderFillRect(buffer, &sbuffer_rect);

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
                drawhook(buffer, parent);
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
            drawhook(buffer, parent);
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
( SDL_Renderer* buffer,
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
        DrawLineBresenham(buffer, src.x, src.y, dst.x, dst.y, color);

        /* render projective debug lines */
        if (*(ks + KEY_D))
        {
            DrawLineBresenham(buffer, src.x, src.y, BUFFER_W_2, WIN_H, color);
            DrawLineBresenham(buffer, dst.x, dst.y, BUFFER_W_2, WIN_H, color);
        }
    }
}

int Setup (SDL_Window** window, SDL_Renderer** buffer)
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

    *buffer = CreateRenderingBuffer(*window);
    if (!buffer)
    {
        Destroy(*window, *buffer);

        return 1;
    }

    srand(time(0)); // random seed

    SDL_version sdl_version;
    SDL_RendererInfo info;

    SDL_GetVersion(&sdl_version);
    SDL_GetRendererInfo(*buffer, &info);
    printf("[SDL_Init] Done (v%d.%d.%d, renderer: %s) \n",
           sdl_version.major, sdl_version.minor, sdl_version.patch,
           info.name);

    return 0;
}

void
Update
( sbuffer_t*     sbuffer,
  SDL_Window*    window,
  SDL_Renderer*  buffer,
  byte_t*        do_run,
  mouse_state_t* ms,
  byte_t*        ks,
  seg2_t*        segs,
  size_t*        seg_head )
{
    size_t head = *seg_head;
    vec2_t dst;
    byte_t prev_holding_mouse_button = ms->pressed == SDL_BUTTON_LEFT;
    byte_t holding_esc;
    HandleEventSync(do_run, ms, ks);
    DrawGrid(buffer);
    DrawFrustum(buffer);
    DrawAxes(buffer);
    DrawSegments(buffer, ks, segs, *seg_head);
    DrawSBufferDfs(buffer, sbuffer, DrawSpan);
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
        DrawLineBresenham(buffer, src.x, src.y, dst.x, dst.y, 0xff0000ff);
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
        SB_Push(sbuffer,
                screen_x0, screen_x1,
                screen_w0, screen_w1,
                ID++,
                seg.color);
    }

    *seg_head = head;
    FrameFlush(buffer);
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
    const size_t prepop_segs_count = 10;
    seg2_t prepop_segs[] = {
        { { 80, 128 }, { 80, 608 }, 0x00ff00ff },   // A
        { { 32, 384 }, { 128, 448 }, 0x00ff00ff },  // B
        { { 32, 224 }, { 128, 288 }, 0x00ff00ff },  // C
        { { 32, 128 }, { 128, 192 }, 0x00ff00ff },  // D
        { { 64, 64 }, { 128, 192 }, 0x00ff00ff },   // E
        { { 128, 192 }, { 128, 288 }, 0xff0000ff }, // F
        { { 128, 288 }, { 128, 448 }, 0x00ff00ff }, // G
        { { 32, 592 }, { 128, 656 }, 0x00ff00ff },  // H
        { { 80, 608 }, { 80, 688 }, 0x00ff00ff },   // I
        { { 128, 448 }, { 128, 656 }, 0x00ff00ff }  // J
    };

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
    SDL_Window* window = 0;   // window
    SDL_Renderer* buffer = 0; // rendering buffer

    int res = Setup(&window, &buffer);
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

    /* main loop */
    while (do_run)
        Update(sbuffer,
               window,
               buffer,
               &do_run,
               &ms,
               ks,
               segs,
               &seg_head);

    printf("--------------------------[ SB_Dump ]--------------------------\n");
    SB_Dump(sbuffer);
    printf("---------------------------------------------------------------\n");

    SB_Destroy(sbuffer);
    Destroy(window, buffer);
    printf("Goodbye! 🙋‍♂️\n");

    return 0;
}
