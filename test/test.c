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
    sbuffer_t* sbuffer = SB_Init(6, 1024);

    SB_Push(sbuffer, 5, 6, A);
    SB_Push(sbuffer, 1, 2, A + 1);
    SB_Push(sbuffer, 3, 4, A + 2);
    SB_Push(sbuffer, 3, 6, A + 3);
    SB_Push(sbuffer, 3, 7, A + 4);
    SB_Push(sbuffer, 0, 7, A + 5);
    SB_Push(sbuffer, 3, 4, A + 6);
    SB_Push(sbuffer, 0, 3, A + 7);
    SB_Push(sbuffer, 0, 5, A + 8);

    SB_Dump(sbuffer);
    SB_Print(sbuffer);
    SB_Destroy(sbuffer);

    return 0;
}
