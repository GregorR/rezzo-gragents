/*
 * Copyright (C) 2011 Gregor Richards
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _XOPEN_SOURCE /* for pid_t */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rezzoc.h"

CState *cs;

void exploreWorld();
void verticalLoop();
void horizontalShimmy();

int main()
{
    int ex, ey, i;

    cs = newCState();
    cstateFirstSM(cs, WW, WH);

    /* now just wander a lot! */
    exploreWorld();

    /* get away from our flag geysers */
    ex = 0;
    ey = -2;
    cstateGetCellXY(cs, ex, ey, &ex, &ey);
    findAndGoto(cs, ex, ey, ACT_ADVANCE, 0);

    /* then find an electron */
    cstateFindNearest(cs, &ex, &ey, CELL_ELECTRON);
    if (ex >= 0) {
        /* go there */
        findAndGoto(cs, ex, ey, ACT_BUILD, FIND_FLAG_WIRE | FIND_FLAG_STOP_SHORT | FIND_FLAG_AVOID_BASE);

        /* then make sure something gets built to connect them */
        for (i = 0; i < CARDINALITIES && cstateGetBlockingCell(cs) != CELL_NONE; i++) {
            cstateDoAndWait(cs, ACT_TURN_RIGHT);
        }
        if (i != CARDINALITIES) cstateDoAndWait(cs, ACT_BUILD);
    }

    /* and then spin like a loony */
    while (1)
        cstateDoAndWait(cs, ACT_TURN_RIGHT);
}

void exploreWorld()
{
    if (WW != -1) verticalLoop();
    horizontalShimmy();
    while (cs->x != 0) {
        verticalLoop();
        horizontalShimmy();
    }
}

void verticalLoop()
{
    findAndGoto(cs, cs->x, cs->h * 2 / 3, ACT_ADVANCE, 0);
    findAndGoto(cs, cs->x, cs->h / 3, ACT_ADVANCE, 0);
    findAndGoto(cs, cs->x, 0, ACT_ADVANCE, 0);
}

void horizontalShimmy()
{
    int x = cs->x + VIEWPORT;
    if (x >= cs->w) x = 0;
    findAndGoto(cs, x, 0, ACT_ADVANCE, 0);
}
