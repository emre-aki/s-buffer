/*
 *  s_buffer.h
 *  s-buffer
 *
 *  Created by Emre Akı on 2024-09-17.
 *
 *  SYNOPSIS:
 *      A rather unique implementation of the ubiquitous S-Buffer -- once a
 *      popular alternative to Z-Buffering for solving the hidden surface
 *      removal problem in software rendering.
 *
 *      The implementation uses a binary tree instead of a linked list to cut
 *      down on the search time. It also supports self-balancing following each
 *      insertion to keep the depth of the tree at a minimum. A single insertion
 *      takes order O(log n), where `n` is the current number of spans pushed
 *      onto the buffer.
 *
 *      The spans need not be inserted in front-to-back order. The buffer can
 *      handle arbitrary ordering as well as interpenetrating geometry.
 *
 *      Original FAQ by Paul Nettle:
 *      https://www.gamedev.net/articles/programming/graphics/s-buffer-faq-r668/
 */

// FIXME: find a sensible way of using `e_malloc` in this library!

#ifndef s_buffer_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define s_buffer_h
#define s_buffer_h_span_t span_t
#define s_buffer_h_sbuffer_t sbuffer_t
#define s_buffer_h_SB_Init SB_Init
#define s_buffer_h_SB_Push SB_Push
#define s_buffer_h_SB_Dump SB_Dump
#define s_buffer_h_SB_Print SB_Print
#define s_buffer_h_SB_Destroy SB_Destroy

#define SB_INTERSECTING 0x0
#define SB_PARALLEL 0x1
#define SB_DEGENERATE 0x2
#define SB_NOT_INTERSECTING 0x3

#define SB_MAX(a, b) ((((a) > (b)) * (a)) + (((b) >= (a)) * (b)))

#define SB_LERP(a, b, p, t) (((b) - (a)) * (float) (p) / (t) + (a))

#define SB_BF(n) (((n)->next ? ((n)->next->height + 1) : 0) - \
                  ((n)->prev ? ((n)->prev->height + 1) : 0))

// FIXME: mitigate the `SB_MAX` usage here
#define SB_HEIGHT(n) (SB_MAX((n)->prev ? ((n)->prev->height + 1) : 0, \
                             (n)->next ? ((n)->next->height + 1) : 0))

#define SB_CROSS_2D(a, b, c, d) ((a) * (d) - (b) * (c))

#define SB_CROSS_SPAN2(u, v) (SB_CROSS_2D((u)->x, (u)->z, (v)->x, (v)->z))

typedef unsigned char byte_t;

typedef struct span {
    struct span *prev, *next; // pointers to left and right subtrees
    float        x0,    x1;   // start and end endpoints in screen space
    float        w0,    w1;   // reciprocal depths associated with each endpoint
    int          height;      // how tall is this span?
    byte_t       id;
} span_t;

typedef struct {
    span_t* root;      // the root of the buffer
    int     size;      // the buffer width
    float   z_near;    // distance from the eye to the near-clipping plane
    size_t  max_depth; // the maximum depth the root span is allowed to grow to
} sbuffer_t;

//
// SB_Falmeq
// Whether two float values are almost equal
//
static byte_t SB_Falmeq (float x, float y)
{
    float res = x - y;
    int i = *((int*) &res) & 0x7fffffff;
    res = *((float*) &i);

    return res < 1e-6;
}

static span_t* SB_Span (float x0, float x1, float w0, float w1, byte_t id)
{
    span_t* span = (span_t*) malloc(sizeof(span_t));

    span->prev = 0;
    span->next = 0;
    span->x0 = x0;
    span->x1 = x1;
    span->w0 = w0;
    span->w1 = w1;
    span->height = 0;
    span->id = id;

    return span;
}

//
// SB_Init
// Initialize a buffer with the given parameters:
// - `size`: The width of the buffer.
// - `z_near`: The view space distance from the eye to the near-clipping plane.
// - `max_depth`: The maximum depth to which the buffer can grow when inserting
//    spans.
//
sbuffer_t* SB_Init (int size, float z_near, size_t max_depth)
{
    sbuffer_t* sbuffer = (sbuffer_t*) malloc(sizeof(sbuffer_t));

    sbuffer->root = 0;
    sbuffer->size = size;
    sbuffer->z_near = z_near;
    sbuffer->max_depth = max_depth;

    return sbuffer;
}

typedef struct {
    float x, z;
} span2_t;

//
// SB_Intersect2D
// 2-D line segment intersection
//
static
byte_t
SB_Intersect2D
( span2_t a, span2_t b,
  span2_t c, span2_t d,
  span2_t* out )
{
    const span2_t u = { b.x - a.x, b.z - a.z };
    const span2_t v = { d.x - c.x, d.z - c.z };
    const span2_t c_a = { c.x - a.x, c.z - a.z };
    const float numer_t = SB_CROSS_SPAN2(&c_a, &v);
    const float numer_q = SB_CROSS_SPAN2(&c_a, &u);
    const float denom = SB_CROSS_SPAN2(&u, &v);
    const float t = numer_t / denom;
    const float q = numer_q / denom;

    if (numer_t && !denom) return SB_PARALLEL;
    if (!(numer_t || denom)) return SB_DEGENERATE;
    if (t <= 1e-6 || t >= 1 - 1e-6 || q <= 1e-6 || q >= 1 - 1e-6)
        return SB_NOT_INTERSECTING;

    out->x = t * u.x + a.x;
    out->z = t * u.z + a.z;

    return SB_INTERSECTING;
}

//
// SB_SpanIntersect
// Calculate the "2-D" intersection of two spans along the x-z plane and store
// the result in the `out` variable as screen space x if they intersect. A
// non-zero return value indicates that the spans are not intersecting.
//   - 0x1: The spans are parallel to one another
//   - 0x2: The two spans are identical, either in the same direction or
//          opposing directions
//   - 0x3: The spans are not intersecting
//
// The function also stores whether the former span originates from or lies on
// the left (i.e., in front) of the point of intersection in the `leftness`
// argument passed. A negative value for `leftness` can be interpreted as
// truthy.
//
// The input vertices are all in perspective-correct screen space.
//
static
byte_t
SB_SpanIntersect
( float u_x0, float u_w0,
  float u_x1, float u_w1,
  float v_x0, float v_w0,
  float v_x1, float v_w1,
  float buffer_width,
  float z_near,
  float* out,
  float* leftness )
{
    const float buffer_width_half = buffer_width * 0.5;
    const float _z_near = 1.0f / z_near;
    const float u_z0 = 1.0f / u_w0, u_z1 = 1.0f / u_w1;
    const float v_z0 = 1.0f / v_w0, v_z1 = 1.0f / v_w1;
    const float u_wx0 = (u_x0 - buffer_width_half) * u_z0 * _z_near;
    const float u_wx1 = (u_x1 - buffer_width_half) * u_z1 * _z_near;
    const float v_wx0 = (v_x0 - buffer_width_half) * v_z0 * _z_near;
    const float v_wx1 = (v_x1 - buffer_width_half) * v_z1 * _z_near;
    const span2_t a = { u_wx0, u_z0 }, b = { u_wx1, u_z1 };
    const span2_t c = { v_wx0, v_z0 }, d = { v_wx1, v_z1 };
    span2_t intersect;
    int res = SB_Intersect2D(a, b, c, d, &intersect);

    if (res)
    {
        *leftness = 0;

        /* a hack that is a bit on the dirty side:
         * in cases where either one of the start endpoints is on the other
         * span, we need to determine whether the first one obscures the other
         */
        if (res == SB_NOT_INTERSECTING)
        {
            const span2_t u = { u_wx1 - v_wx0, u_z1 - v_z0 };
            const span2_t v = { v_wx1 - v_wx0, v_z1 - v_z0 };
            *leftness = SB_CROSS_SPAN2(&u, &v);
        }

        return res;
    }

    *out = intersect.x * z_near / intersect.z + buffer_width_half;

    const span2_t u = { u_wx0 - intersect.x, u_z0 - intersect.z };
    const span2_t v = { v_wx0 - intersect.x, v_z0 - intersect.z };
    *leftness = SB_CROSS_SPAN2(&u, &v);

    return res;
}

//
// SB_BisectParent
// Bisect the `parent` due to being obscured by another span that lies partially
// or completely in front of it.
//
static
void
SB_BisectParent
( span_t* parent,
  float   x0,    float x1,
  float   w0,    float w1,
  float   visx0, float visx1,
  byte_t  id )
{
    const float size = x1 - x0;
    const float old_parent_size = parent->x1 - parent->x0;
    const float old_parent_x0 = parent->x0, old_parent_x1 = parent->x1;
    const float old_parent_w0 = parent->w0, old_parent_w1 = parent->w1;
    const byte_t old_parent_id = parent->id;
    span_t* parent_split;

    /* override the `parent` with the visible portion of the `span` */
    parent->x0 = visx0; parent->x1 = visx1;
    parent->w0 = SB_LERP(w0, w1, visx0 - x0, size);
    parent->w1 = SB_LERP(w0, w1, visx1 - x0, size);
    parent->id = id;

    /* insert the left bisection of the parent immediately to the left */
    parent_split = SB_Span(old_parent_x0, visx0,
                           old_parent_w0, SB_LERP(old_parent_w0, old_parent_w1,
                                                  visx0 - old_parent_x0,
                                                  old_parent_size),
                           old_parent_id);
    parent_split->prev = parent->prev;
    parent->prev = parent_split;
    if (SB_BF(parent_split) < -1)  // balance if necessary
    {
        span_t* old_parent = parent_split;
        span_t* new_parent = parent_split->prev;
        span_t* child = new_parent->prev;

        if (SB_BF(new_parent) > 0) // need to do a double-rotation
        {
            child = new_parent;
            new_parent = child->next;
            child->next = new_parent->prev;
            new_parent->prev = child;
        }

        old_parent->prev = new_parent->next;
        new_parent->next = old_parent;
        parent->prev = new_parent;
        /* update the heights of the spans affected by the balancing */
        old_parent->height = SB_HEIGHT(old_parent);
        child->height = SB_HEIGHT(child);
        new_parent->height = SB_HEIGHT(new_parent);
    }
    else
    {
        parent_split->height = SB_HEIGHT(parent_split);
    }

    /* insert the right bisection of the parent immediately to the right */
    parent_split = SB_Span(visx1, old_parent_x1,
                           SB_LERP(old_parent_w0, old_parent_w1,
                                   visx1 - old_parent_x0,
                                   old_parent_size),
                           old_parent_w1,
                           old_parent_id);
    parent_split->next = parent->next;
    parent->next = parent_split;
    parent_split->height = SB_HEIGHT(parent_split);

    parent->height = SB_HEIGHT(parent);
}

typedef struct {
    span_t* span;
    float   left, right;
} pscope_t;

//
// SB_Push
// Push a span onto the buffer with endpoints `(x0, w0)` and `(x1, w1)` where
// both endpoints are in perspective-correct screen space -- meaning `w0` and
// `w1` are the multiplicative inverses of their corresponding distances from
// the eye in view space. Another way to put it is that they are the reciprocals
// of the w-components in clip space coordinates:
//
// `1 / w0_clip = 1 / z0_view = w0`
// `1 / w1_clip = 1 / z1_view = w1`
//
// A unique `id` can be provided for debugging and identification purposes.
//
int
SB_Push
( sbuffer_t* sbuffer,
  float  x0, float x1,
  float  w0, float w1,
  byte_t id )
{
    const float size = x1 - x0;
    span_t* curr = sbuffer->root;

    /* the buffer is empty — initialize the root and return immediately */
    if (!curr)
    {
        // clip the segment from left
        const float clipleft = SB_MAX(-x0, 0);
        // ...and right
        const float clipright = SB_MAX(x1 - sbuffer->size, 0);
        const float clipped_size = size - clipright - clipleft;

        /* only insert if there's something left to insert */
        if (clipped_size > 0)
        {
            const float new_x0 = x0 + clipleft, new_x1 = new_x0 + clipped_size;
            const float new_w0 = SB_LERP(w0, w1, new_x0 - x0, size);
            const float new_w1 = SB_LERP(w0, w1, new_x1 - x0, size);
            sbuffer->root = SB_Span(new_x0, new_x1, new_w0, new_w1, id);

            return 0;
        }

        return 1;
    }

    // left and right boundaries of insertion
    float left = 0, right = sbuffer->size;
    // where the current insertion starts, and how wide the remaining segment is
    float x = x0, remaining = size;
    byte_t pushed = 0; // whether we were able push to anything
    /* initialize the push-stack to store the local scope for each "recursive"
     * stride
     */
    pscope_t stack[sbuffer->max_depth];
    int depth = 0; // stack pointer: how deep into the tree we currently are

    /* continue pushing in sub-segments unless there's nothing left to insert */
    while (remaining > 0)
    {
        span_t* parent;

        /* try to find an available spot to insert */
        while (curr)
        {
            if (depth == sbuffer->max_depth)
            {
                printf("[SB_Push] Maximum buffer depth reached!\n");

                return 1;
            }

            parent = curr;
            pscope_t scope = { parent, left, right };
            *(stack + depth++) = scope;

            const float parent_size = parent->x1 - parent->x0;
            const float w = SB_LERP(w0, w1, x - x0, size);

            float intersection, leftness;
            const byte_t not_intersecting = SB_SpanIntersect(
                x, w,
                x1, w1,
                parent->x0, parent->w0,
                parent->x1, parent->w1,
                sbuffer->size,
                sbuffer->z_near,
                &intersection,
                &leftness
            );

            if (x < parent->x0)
            {
                /* does the span we're about to insert overlap with the one
                 * we're currently on along the x-axis?
                 */
                if (x1 > parent->x0)
                {
                    if (!not_intersecting)
                    {
                        if (leftness > 0)
                        {
                            /* =========== [CASE-L1]: bisecting =========== */
                            if (x1 < parent->x1)
                            {
                                SB_BisectParent(parent, x0, x1, w0, w1,
                                                intersection, x1,
                                                id);
                                pushed = 0xff;
                            }
                            /* ==== [CASE-L2]: obscures from the right ==== */
                            else
                            {
                                parent->w1 = SB_LERP(parent->w0, parent->w1,
                                                     intersection - parent->x0,
                                                     parent_size);
                                parent->x1 = intersection;
                            }
                        }
                        /* ======= [CASE-L3]: obscures from the left ====== */
                        else
                        {
                            parent->w0 = SB_LERP(parent->w0, parent->w1,
                                                 intersection - parent->x0,
                                                 parent_size);
                            parent->x0 = intersection;
                        }
                    }
                    else
                    {
                        const float w_at_parent_x0 = SB_LERP(w0, w1,
                                                            parent->x0 - x0,
                                                            size);
                        /* FIXME: 😬 */
                        const int w_at_parent_x0_comp =
                            (int) (w_at_parent_x0 * 1000000);
                        const int parent_w0_comp = (int) (parent->w0 * 1000000);

                        if (parent_w0_comp < w_at_parent_x0_comp ||
                            parent_w0_comp == w_at_parent_x0_comp &&
                                leftness > 0)
                        {
                            /* ===== [CASE-L4]: obscures from the left ==== */
                            if (x1 < parent->x1)
                            {
                                parent->w0 = SB_LERP(parent->w0, parent->w1,
                                                     x1 - parent->x0,
                                                     parent_size);
                                parent->x0 = x1;
                            }
                            /* ====== [CASE-L5]: completely obscures ====== */
                            else
                            {
                                parent->w0 = w_at_parent_x0;
                                parent->w1 = SB_LERP(w0, w1,
                                                     parent->x1 - x0,
                                                     size);
                                parent->id = id;
                                pushed = 0xff;
                            }
                        }
                    }
                }

                right = parent->x0;
                curr = parent->prev;
            }
            else
            {
                /* does the span we're about to insert overlap with the one
                 * we're currently on along the x-axis?
                 */
                if (x < parent->x1)
                {
                    if (!not_intersecting)
                    {
                        if (leftness > 0)
                        {
                            /* =========== [CASE-R1]: bisecting =========== */
                            if (x1 < parent->x1)
                            {
                                SB_BisectParent(parent, x0, x1, w0, w1,
                                                intersection, x1,
                                                id);
                                pushed = 0xff;
                            }
                            /* ==== [CASE-R2]: obscures from the right ==== */
                            else
                            {
                                parent->w1 = SB_LERP(parent->w0, parent->w1,
                                                     intersection - parent->x0,
                                                     parent_size);
                                parent->x1 = intersection;
                            }
                        }
                        else
                        {
                            /* =========== [CASE-R3]: bisecting =========== */
                            if (x > parent->x0)
                            {
                                SB_BisectParent(parent, x0, x1, w0, w1,
                                                x, intersection,
                                                id);
                                pushed = 0xff;
                            }
                            /* ===== [CASE-R4]: obscures from the left ==== */
                            else
                            {
                                parent->w0 = SB_LERP(parent->w0, parent->w1,
                                                     intersection - parent->x0,
                                                     parent_size);
                                parent->x0 = intersection;
                                /* need to proceed leftward instead, since we're
                                 * obscuring from left
                                 */
                                right = parent->x0;
                                curr = parent->prev;

                                continue;
                            }
                        }
                    }
                    else
                    {
                        const float parent_w_at_x = SB_LERP(parent->w0,
                                                            parent->w1,
                                                            x - parent->x0,
                                                            parent_size);
                        /* FIXME: 😬 */
                        const int parent_w_at_x_comp =
                            (int) (parent_w_at_x * 1000000);
                        const int w_comp = (int) (w * 1000000);

                        if (parent_w_at_x_comp < w_comp ||
                            parent_w_at_x_comp == w_comp && leftness > 0)
                        {
                            if (x > parent->x0)
                            {
                                /*========== [CASE-R5]: bisecting ========= */
                                if (x1 < parent->x1)
                                {
                                    SB_BisectParent(parent, x0, x1, w0, w1,
                                                    x, x1,
                                                    id);
                                    pushed = 0xff;
                                }
                                /* == [CASE-R6]: obscures from the right == */
                                else
                                {
                                    parent->w1 = SB_LERP(parent->w0, parent->w1,
                                                         x - parent->x0,
                                                         parent_size);
                                    parent->x1 = x;
                                }
                            }
                            else
                            {
                                /* === [CASE-R7]: obscures from the left == */
                                if (x1 < parent->x1)
                                {
                                    parent->w0 = SB_LERP(parent->w0, parent->w1,
                                                         x1 - parent->x0,
                                                         parent_size);
                                    parent->x0 = x1;
                                    /* need to proceed leftward instead, since
                                     * we're obscuring from left
                                     */
                                     right = parent->x0;
                                     curr = parent->prev;

                                     continue;

                                }
                                /* ==== [CASE-R8]: completely obscures ==== */
                                else
                                {
                                    parent->w0 = w;
                                    parent->w1 = SB_LERP(w0, w1,
                                                         parent->x1 - x0,
                                                         size);
                                    parent->id = id;
                                    pushed = 0xff;
                                }
                            }
                        }
                    }
                }

                left = parent->x1;
                curr = parent->next;
            }
        }
        /* we should have found an appropriate spot to insert by now */

        // clip the current sub-segment from left
        const float clipleft = SB_MAX(left - x, 0);
        // ...and right
        const float clipright = SB_MAX(x + remaining - right, 0);
        const float clipped_size = remaining - clipleft - clipright;

        /* only insert if there's something left to insert */
        if (clipped_size > 0)
        {
            const float new_x0 = x + clipleft, new_x1 = new_x0 + clipped_size;
            const float new_w0 = SB_LERP(w0, w1, new_x0 - x0, size);
            const float new_w1 = SB_LERP(w0, w1, new_x1 - x0, size);
            curr = SB_Span(new_x0, new_x1, new_w0, new_w1, id);
            if (x < parent->x0) parent->prev = curr;
            else parent->next = curr;
            pushed = 0xff;
        }

        // where to continue inserting should there be any remaining
        // sub-segments
        int insertion_bookmark = -1;
        // where the imbalance occurred, if one did occur
        int imbalance_bookmark = -1;
        // temporary stack pointer to walk back up the stack
        int stack_depth = depth - 1;
        // temporary pointer to determine whether there had been a left turn
        // while walking back up the stack
        float tmp_x = x;

        /* trace the insertion stack back in reverse to see if we need to
         * continue inserting remaining segments, or if we need to re-balance
         * the buffer...
         */
        for (size_t i = 0; i < depth; ++i)
        {
            /* ...until we've found both a left turn and an imbalanced
             * subtree
             */
            if (!(insertion_bookmark < 0 || imbalance_bookmark < 0)) break;

            span_t* parent_span = (stack + stack_depth)->span;

            /* remember "where we left off" for the next iteration:
             * we only care about left turns, as they are the ones that can
             * potentially leave outstanding sub-segments yet to be inserted
             */
            if (insertion_bookmark < 0 && tmp_x < parent_span->x0)
                insertion_bookmark = stack_depth;
            tmp_x = parent_span->x0;

            if (imbalance_bookmark < 0)
            {
                const int balance_factor = SB_BF(parent_span);
                /* remember where the imbalance occurred, if there happened to
                 * be one...
                 */
                if (balance_factor < -1 || balance_factor > 1)
                    imbalance_bookmark = stack_depth;
                /* ...otherwise, update the height of this span */
                else if (curr)
                    /* FIXME: there might not be a need to use `MAX` here due to
                     * the intrinsic nature of how AVL trees grow, i.e.,
                     * `depth - stack_depth` should always be greater than or
                     * equal to `span->height`
                     */
                    parent_span->height = SB_MAX(parent_span->height,
                                                 depth - stack_depth);
            }

            --stack_depth;
        }

        /* update the scope parameters if we are to continue inserting */
        if (insertion_bookmark >= 0)
        {
            pscope_t scope = *(stack + insertion_bookmark);
            curr = scope.span;
            left = scope.left;
            right = scope.right;
            x = curr->x0;
            // there's an outstanding sub-segment of size `clipright` waiting to
            // be inserted in the next iteration
            remaining = clipright;
            // adjust the stack pointer for the next iteration
            depth = insertion_bookmark;
        }
        /* if not, then we're free to exit */
        else
        {
            remaining = 0;
        }

        /* lo and behold: *the* balancing, at long last!
         * let's balance the crap out of this buffer, shall we?
         * here goes nothing...
         */
        if (imbalance_bookmark >= 0)
        {
            /* remember the parent of where the imbalance started, you're gonna
             * need it later
             */
            span_t* imbalance_parent = 0;
            if (imbalance_bookmark)
                imbalance_parent = (stack + imbalance_bookmark - 1)->span;

            span_t* old_parent = (stack + imbalance_bookmark)->span;
            span_t *new_parent, *child;

            /* restore balance in the `prev` sub-tree */
            if (SB_BF(old_parent) < 0)
            {
                new_parent = old_parent->prev;
                child = new_parent->prev;

                if (SB_BF(new_parent) > 0) // need to do a double-rotation
                {
                    child = new_parent;
                    new_parent = child->next;
                    child->next = new_parent->prev;
                    new_parent->prev = child;
                }

                old_parent->prev = new_parent->next;
                new_parent->next = old_parent;
            }
            /* restore balance in the `next` sub-tree */
            else
            {
                new_parent = old_parent->next;
                child = new_parent->next;

                if (SB_BF(new_parent) < 0) // need to do a double-rotation
                {
                    child = new_parent;
                    new_parent = child->prev;
                    child->prev = new_parent->next;
                    new_parent->next = child;
                }

                old_parent->next = new_parent->prev;
                new_parent->prev = old_parent;
            }

            /* update the heights after balancing */
            old_parent->height = SB_HEIGHT(old_parent);
            child->height = SB_HEIGHT(child);
            new_parent->height = SB_HEIGHT(new_parent);

            /* update the parent of the newly balanced span */
            if (imbalance_parent)
            {
                if (new_parent->x0 < imbalance_parent->x0)
                    imbalance_parent->prev = new_parent;
                else
                    imbalance_parent->next = new_parent;
            }
            /* if there is no parent, it means we just balanced the root span,
             * so update its reference
             */
            else
            {
                sbuffer->root = new_parent;
            }

            /* re-construct the stack after having balanced the buffer, only if
             * the balancing had occurred higher up the stack than where we
             * should continue inserting from
             */
            if (imbalance_bookmark <= insertion_bookmark)
            {
                int i = imbalance_bookmark;
                float new_left = 0, new_right = sbuffer->size;

                /* re-adjust the initial `left` and `right` boundaries unless
                 * the imbalance occurred at the root
                 */
                if (i)
                {
                    const pscope_t parent_scope = *(stack + i - 1);
                    const span_t* parent_span = parent_scope.span;
                    new_left = parent_scope.left;
                    new_right = parent_scope.right;

                    if (new_parent->x0 < parent_span->x0)
                        new_right = parent_span->x0;
                    else
                        new_left = parent_span->x1;
                }

                for (span_t* stack_span = new_parent; stack_span; ++i)
                {
                    pscope_t scope = { stack_span, new_left, new_right };
                    *(stack + i) = scope;

                    // we've reached the "insertion bookmark", the
                    // re-construction of the stack is complete
                    if (stack_span == curr) break;

                    if (x < stack_span->x0)
                    {
                        new_right = stack_span->x0;
                        stack_span = stack_span->prev;
                    }
                    else
                    {
                        new_left = stack_span->x1;
                        stack_span = stack_span->next;
                    }
                }

                left = new_left;   // update the `left`...
                right = new_right; // ...and the `right` boundaries
                depth = i; // adjust the stack pointer for the next iteration
            }
        }
    }

    if (!pushed)
    {
        printf("[SB_Push] Cannot add more segments, spot fully occluded!\n");

        return 1;
    }

    return 0;
}

static void _SB_Dump (span_t* span, size_t depth)
{
    size_t indent = depth << 2;
    for (size_t i = 0; i < indent; ++i) printf(" ");
    printf("[%c] [%.3f, %.3f)\n", span->id, span->x0, span->x1);
    if (span->prev) _SB_Dump(span->prev, depth + 1);
    if (span->next) _SB_Dump(span->next, depth + 1);
}

//
// SB_Dump
// Dump the spans in the buffer to `stdout` in a tree-like structure to help in
// debugging.
//
// Each line in the dump follows the format `[id] [x0, x1)`.
//
void SB_Dump (sbuffer_t* sbuffer)
{
    span_t* root = sbuffer->root;
    if (!root)
    {
        printf("[SB_Dump] Empty S-Buffer!\n");

        return;
    }

    _SB_Dump(root, 0);
}

static void _SB_Print (span_t* span, byte_t* buffer)
{
    const int X0 = ceil(span->x0 - 0.5f), X1 = ceil(span->x1 - 0.5f);
    const int span_size = X1 - X0;
    int x = X0;

    for (int i = 0; i < span_size; ++i) *(buffer + x++) = span->id;
    if (span->prev) _SB_Print(span->prev, buffer);
    if (span->next) _SB_Print(span->next, buffer);
}

//
// SB_Print
// Render the contents of the buffer into `stdout`.
//
void SB_Print (sbuffer_t* sbuffer)
{
    int buffer_size = sbuffer->size;
    byte_t buffer[buffer_size + 1];

    *(buffer + buffer_size) = 0;
    for (int i = 0; i < buffer_size; ++i) *(buffer + i) = '_';
    if (sbuffer->root) _SB_Print(sbuffer->root, buffer);
    printf("%s\n", buffer);
}

//
// SB_Destroy
// Free up all memory allocated by the buffer.
//
void SB_Destroy (sbuffer_t* sbuffer)
{
    span_t *curr = sbuffer->root, *parent;
    span_t* stack[sbuffer->max_depth];
    size_t i = 0;

    while (curr)
    {
        while (curr)
        {
            parent = curr;
            *(stack + i++) = parent;
            curr = parent->prev;
        }
        /* `prev` sub-trees have been exhausted at this point */

        curr = parent->next; // try the `next` sub-tree

        /* no `prev` or `next` sub-trees mean we're on a leaf span, go ahead and
         * free it
         */
        if (!curr)
        {
            span_t* grandparent = 0;

            if (--i)
            {
                grandparent = *(stack + --i);
                /* remove the link from the grandparent to the parent, so we
                 * won't end up back here going back up the stack
                 */
                if (parent->x0 < grandparent->x0) grandparent->prev = 0;
                else grandparent->next = 0;
            }

            free(parent);
            curr = grandparent; // continue freeing from the grandparent
        }
    }

    free(sbuffer);
}

#endif
