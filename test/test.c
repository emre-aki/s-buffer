/*
 *  test.c
 *  s-buffer
 *
 *  Created by Emre Akı on 2024-12-01.
 *
 *  SYNOPSIS:
 *     The S-Buffer test suite
 */

#include "s_buffer.h"

int main ()
{
    const byte_t A = 65;
    sbuffer_t* sbuffer = SB_Init(16, 4, 1024);

    SB_Push(sbuffer, 88.0f/15, 20.0f/3, 1.0f/15, 1.0f/6, A);
    SB_Push(sbuffer, 28.0f/3, 152.0f/15, 1.0f/6, 1.0f/15, A + 1);
    SB_Push(sbuffer, 20.0f/3, 28.0f/3, 1.0f/6, 1.0f/6, A + 2);
    SB_Push(sbuffer, 17.0f/3, 8, 1.0f/12, 0.2, A + 3);
    SB_Push(sbuffer, 8, 31.0f/3, 0.2, 1.0f/12, A + 4);

    SB_Dump(sbuffer);
    SB_Print(sbuffer);
    SB_Destroy(sbuffer);

    return 0;
}
