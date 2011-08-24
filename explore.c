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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rezzoc.h"

CState *cs;

void exploreWorld();
void verticalLoop();
void verticalStep();
void horizontalShimmy(int til);
void horizontalStep();
void assertDirection(int dir);

int main()
{
    cs = newCState();
    readAll(0, (char *) &cs->sm, sizeof(ServerMessage));
    cstateFirstSM(cs, 320, 320);

    /* now just wander a lot! */
    exploreWorld();
}

void exploreWorld()
{
    int sx = cs->x;

    do {
        verticalLoop();
        horizontalShimmy(sx);
        cstateDoAndWait(cs, ACT_TURN_LEFT);
    } while (cs->x != sx);
}

void verticalLoop()
{
    findAndGoto(cs, cs->x, cs->h * 2 / 3, -1);
    findAndGoto(cs, cs->x, cs->h / 3, -1);
    findAndGoto(cs, cs->x, 0, -1);
    fprintf(stderr, "%d %d\n", cs->x, cs->y);
}

void verticalStep()
{
    int i, ty;
    CardinalityHelper ch = cardinalityHelpers[cs->card];
    assertDirection(NORTH);

    /* figure out our target cell */
    ty = cs->y-1;
    if (ty < 0) ty = cs->h-1;
    cstateGetCell(&i, NULL, cs, cs->x, ty);

    while (cs->y != ty) {
        /* figure out if it's obstructed */
        while (cs->c[i] != CELL_NONE)
            cstateDoAndWait(cs, ACT_HIT);

        /* then move in */
        cstateDoAndWait(cs, ACT_ADVANCE);
    }
}

void horizontalShimmy(int til)
{
    int i;
    for (i = 0; i < VIEWPORT; i++) {
        horizontalStep();
        if (cs->x == til) break;
    }
}

void horizontalStep()
{
    int i, tx;
    CardinalityHelper ch = cardinalityHelpers[cs->card];
    assertDirection(EAST);

    /* figure out our target cell */
    tx = cs->x + 1;
    if (tx >= cs->w) tx = 0;

    while (cs->x != tx) {
        /* figure out if it's obstructed */
        cstateGetCell(&i, NULL, cs, tx, cs->y);
        while (cs->c[i] != CELL_NONE)
            cstateDoAndWait(cs, ACT_HIT);

        /* then move in */
        cstateDoAndWait(cs, ACT_ADVANCE);
    }
}

void assertDirection(int dir)
{
    while (cs->card != dir)
        cstateDoAndWait(cs, ACT_TURN_RIGHT);
}
