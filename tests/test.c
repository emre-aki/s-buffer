/*
 *  test.c
 *  s-buffer
 *
 *  Created by Emre Akı on 2024-12-01.
 *
 *  SYNOPSIS:
 *     The S-Buffer test suite
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "shared/s_helpers.h"
#include "shared/s_prepop.h"
#define S_BUFFER_DEFS_ONLY
#include "s_buffer.h"

#define SCREEN_HALFWIDTH 400
#define SCREEN_HEIGHT 800
#define Z_NEAR 96

static void PushSpans (sbuffer_t* sbuffer, const test_case_t* tc)
{
    byte_t ID = 65;

    for (size_t i = 0; i < tc->segs_count; ++i)
    {
        const seg2_t seg = *(tc->segs + i);

        const float screen_src = S_ToScreenSpace(&seg.src,
                                                 SCREEN_HALFWIDTH,
                                                 SCREEN_HEIGHT,
                                                 Z_NEAR);

        const float screen_dst = S_ToScreenSpace(&seg.dst,
                                                 SCREEN_HALFWIDTH,
                                                 SCREEN_HEIGHT,
                                                 Z_NEAR);

        float screen_x0, screen_x1, screen_w0, screen_w1;
        const byte_t src_min = screen_src <= screen_dst;

        /* sort the endpoints in ascending screen space x before pushing the
         * segment onto the buffer
         */
        screen_x0 = src_min * screen_src + !src_min * screen_dst;
        screen_x1 = src_min * screen_dst + !src_min * screen_src;

        screen_w0 = src_min * S_ZToScreenSpace(seg.src.y, SCREEN_HEIGHT) +
                    !src_min * S_ZToScreenSpace(seg.dst.y, SCREEN_HEIGHT);

        screen_w1 = src_min * S_ZToScreenSpace(seg.dst.y, SCREEN_HEIGHT) +
                    !src_min * S_ZToScreenSpace(seg.src.y, SCREEN_HEIGHT);

        SB_Push(sbuffer,
                screen_x0, screen_x1,
                screen_w0, screen_w1,
                ID++,
                seg.color);
    }
}

static int RunTestCase (const test_case_t* tc)
{
    /* fork and run the test case in a separate process: it may fail or exit
     * with a non-zero status code, and we don't want to take the test runner
     * down with it
     */
    pid_t pid = fork();
    if (!pid)
    {
        sbuffer_t* sbuffer = SB_Init(SCREEN_HALFWIDTH << 1, Z_NEAR, 10);
        PushSpans(sbuffer, tc);
        SB_Destroy(sbuffer);

        _exit(0);
    }

    int code;               // wait for the child process that executes the test
    waitpid(pid, &code, 0); // case to exit

    return WIFEXITED(code) && !WEXITSTATUS(code);
}

int main ()
{
    int failcount = 0;

    for (size_t i = 0; i < N_CASES; ++i)
    {
        const int success = RunTestCase(TEST_CASES + i);
        failcount += 1 & !success;

        if (success) printf("[test] ✅ Case %d/%d passed\n", i + 1, N_CASES);
        else printf("[test] ❌ Case %d/%d failed\n", i + 1, N_CASES);
    }

    if (failcount)
        printf("[test] 🤦‍♂️ %d out of %d tests failed!\n", failcount, N_CASES);
    else
        printf("[test] 🎉 All tests passed!\n");

    return failcount > 0;
}
