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
 *      The spans need not be inserted in the front-to-back order. The buffer
 *      can handle arbitrary ordering as well as interpenetrating spans.
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

#define SB_MAX(a, b) ((((a) > (b)) * (a)) + (((b) >= (a)) * (b)))

#define SB_LERP(a, b, t) (((b) - (a)) * (t) + (a))

#define SB_BF(n) (((n)->next ? ((n)->next->height + 1) : 0) - \
                  ((n)->prev ? ((n)->prev->height + 1) : 0))

// FIXME: mitigate the `SB_MAX` usage here
#define SB_HEIGHT(n) (SB_MAX((n)->prev ? ((n)->prev->height + 1) : 0, \
                             (n)->next ? ((n)->next->height + 1) : 0))

typedef unsigned char byte_t;

typedef struct span {
    struct span *prev, *next;
    int          x0,    x1;
    int          height;
    byte_t       id;
} span_t;

typedef struct {
    span_t* root;
    int     size;
    size_t  max_depth;
} sbuffer_t;

static span_t* SB_Span (int x0, int x1, byte_t id)
{
    span_t* span = (span_t*) malloc(sizeof(span_t));

    span->prev = 0;
    span->next = 0;
    span->x0 = x0;
    span->x1 = x1;
    span->height = 0;
    span->id = id;

    return span;
}

sbuffer_t* SB_Init (int size, size_t max_depth)
{
    sbuffer_t* sbuffer = (sbuffer_t*) malloc(sizeof(sbuffer_t));

    sbuffer->root = 0;
    sbuffer->size = size;
    sbuffer->max_depth = max_depth;

    return sbuffer;
}

typedef struct {
    span_t* span;
    int     left, right;
} pscope_t;

int SB_Push (sbuffer_t* sbuffer, int x0, int x1, byte_t id)
{
    const int size = x1 - x0;
    span_t* curr = sbuffer->root;

    /* the buffer is empty — initialize the root and return immediately */
    if (!curr)
    {
        // clip the segment from left
        const int clipleft = SB_MAX(-x0, 0);
        // ...and right
        const int clipright = SB_MAX(x1 - sbuffer->size, 0);
        const int clipped_size = size - clipright - clipleft;

        /* only insert if there's something left to insert */
        if (clipped_size > 0)
        {
            const int new_x0 = x0 + clipleft, new_x1 = new_x0 + clipped_size;
            sbuffer->root = SB_Span(new_x0, new_x1, id);

            return 0;
        }

        return 1;
    }

    // left and right boundaries of insertion
    int left = 0, right = sbuffer->size;
    // where the current insertion starts, and how wide the remaining segment is
    int x = x0, remaining = size;
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

            if (x < parent->x0)
            {
                right = parent->x0;
                curr = parent->prev;
            }
            else
            {
                left = parent->x1;
                curr = parent->next;
            }
        }
        /* we should have found an empty spot to insert by now */

        // clip the current sub-segment from left
        const int clipleft = SB_MAX(left - x, 0);
        // ...and right
        const int clipright = SB_MAX(x + remaining - right, 0);
        const int clipped_size = remaining - clipleft - clipright;

        /* only insert if there's something left to insert */
        if (clipped_size > 0)
        {
            const int new_x0 = x + clipleft, new_x1 = new_x0 + clipped_size;
            curr = SB_Span(new_x0, new_x1, id);
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
        int tmp_x = x;

        /* trace the insertion stack back in reverse to see if we need to
         * continue inserting remaining segments, or if we need to re-balance
         * the buffer...
         */
        for (size_t i = 0; i < depth; ++i)
        {
            /* ...until we've found both a left turn and an imbalanced
             * subtree
             */
            if (!(insertion_bookmark < 0 || (curr && imbalance_bookmark < 0)))
                break;

            span_t* parent_span = (stack + stack_depth)->span;

            /* remember "where we left off" for the next iteration:
             * we only care about left turns, as they are the ones that can
             * potentially leave outstanding sub-segments yet to be inserted
             */
            if (insertion_bookmark < 0 && tmp_x < parent_span->x0)
                insertion_bookmark = stack_depth;
            tmp_x = parent_span->x0;

            if (imbalance_bookmark < 0 && curr)
            {
                const int balance_factor = SB_BF(parent_span);
                /* remember where the imbalance occurred, if there happened to
                 * be one...
                 */
                if (balance_factor < -1 || balance_factor > 1)
                    imbalance_bookmark = stack_depth;
                /* ...otherwise, update the height of this span */
                else
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
            x = curr->x1;
            // there's an outstanding sub-segment of size `clipright` waiting to
            // be inserted — try to insert it to the right of the current span
            remaining = clipright + curr->x0 - curr->x1;
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
                int new_left = 0, new_right = sbuffer->size;

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
    printf("[%c] [%d, %d)\n", span->id, span->x0, span->x1);
    if (span->prev) _SB_Dump(span->prev, depth + 1);
    if (span->next) _SB_Dump(span->next, depth + 1);
}

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
    const int span_size = span->x1 - span->x0;
    int x = span->x0;

    for (int i = 0; i < span_size; ++i) *(buffer + x++) = span->id;
    if (span->prev) _SB_Print(span->prev, buffer);
    if (span->next) _SB_Print(span->next, buffer);
}

void SB_Print (sbuffer_t* sbuffer)
{
    int buffer_size = sbuffer->size;
    byte_t buffer[buffer_size + 1];

    *(buffer + buffer_size) = 0;
    for (int i = 0; i < buffer_size; ++i) *(buffer + i) = '_';
    if (sbuffer->root) _SB_Print(sbuffer->root, buffer);
    printf("%s\n", buffer);
}

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
