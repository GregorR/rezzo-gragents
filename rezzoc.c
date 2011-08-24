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
#include <string.h>

#include "rezzoc.h"

#include "../helpers.h"

const CardinalityHelper cardinalityHelpers[] = {
    {1, 0, 0, 1},
    {0, 1, -1, 0},
    {-1, 0, 0, -1},
    {0, -1, 1, 0}
};

/* really truly read this much data */
ssize_t readAll(int fd, char *buf, size_t count)
{
    ssize_t rd;
    size_t at;
    at = 0;
    while (at < count) {
        rd = read(fd, buf + at, count - at);
        if (rd <= 0) return -1;
        at += rd;
    }
    return at;
}

/* really truly write this much data */
ssize_t writeAll(int fd, const char *buf, size_t count)
{
    ssize_t wr;
    size_t at;
    at = 0;
    while (at < count) {
        wr = write(fd, buf + at, count - at);
        if (wr <= 0) return -1;
        at += wr;
    }
    return at;
}

/* create a CState */
CState *newCState()
{
    CState *ret;
    int i;

    SF(ret, malloc, NULL, (sizeof(CState)));

    ret->w = ret->h = -1;
    ret->minx = ret->miny = ret->maxx = ret->maxy = ret->x = ret->y = 0;

    ret->id = -1; /* unknown */
    for (i = 0; i < MAX_AGENTS; i++) {
        ret->agents[i].x = ret->agents[i].y = -1; /* unknown */
    }

    /* one of these three must be filled in some time! */
    ret->home = ret->cur = NULL;
    ret->c = NULL;
}

/* intialize a CState with its first server message */
void cstateFirstSM(CState *cs, int w, int h)
{
    if (w > 0 && h > 0) {
        /* oh good, a known size! */
        SF(cs->c, malloc, NULL, (w*h));
        memset(cs->c, CELL_UNKNOWN, w*h);
        cs->w = w;
        cs->h = h;
    } else {
        /* size isn't known, start with the current CCell */
        cs->home = cs->cur = newCCell(NULL, 0);
    }

    /* set our CM to nop (just in case) */
    cs->cm.ts = cs->sm.ts - 1;
    cs->cm.act = ACT_NOP;

    /* now just integrate the data */
    cstateUpdate(cs);
}

/* update a CState with a just-received server message */
void cstateUpdate(CState *cs)
{
    /* FIXME: support for unknown size */

    unsigned char c;
    int wi, si, y, wy, yoff, x, wx;
    CardinalityHelper ch = cardinalityHelpers[cs->card];

    /* handle the last client message */
    if (cs->sm.ts == ((cs->cm.ts + 1) & 0xFF) && cs->sm.ack == ACK_OK) {
        /* we actually did this action */
        switch (cs->cm.act) {
            case ACT_NOP:
            case ACT_HIT:
                break;

            case ACT_ADVANCE:
                cs->x -= ch.xd;
                cs->y -= ch.yd;
                break;

            case ACT_TURN_LEFT:
                cs->card--;
                if (cs->card < 0) cs->card = CARDINALITIES - 1;
                break;

            case ACT_TURN_RIGHT:
                cs->card++;
                if (cs->card >= CARDINALITIES) cs->card = 0;
                break;

            case ACT_BUILD:
                /* mark the current cell as conductor (FIXME: doesn't work with ccells) */
                cstateGetCell(&wi, NULL, cs, cs->x, cs->y);
                cs->c[wi] = CELL_CONDUCTOR;
                cs->x -= ch.xd;
                cs->y -= ch.yd;
                break;
        }

        /* make sure x and y are consistent */
        if (cs->w > 0 && cs->h > 0) {
            if (cs->x < 0) cs->x += cs->w;
            if (cs->x >= cs->w) cs->x -= cs->w;
            if (cs->y < 0) cs->y += cs->h;
            if (cs->y >= cs->h) cs->y -= cs->h;
        }

    } else {
        fprintf(stderr, "Error %d for action %c\n", (int) cs->sm.ack, cs->cm.act);

    }

    ch = cardinalityHelpers[cs->card];

    /* then update our state */
    for (y = 0, yoff = 0; y < VIEWPORT; y++, yoff += VIEWPORT) {
        for (x = 0, si = yoff; x < VIEWPORT; x++, si++) {
            /* calculate the world location */
            wx = cs->x + (x-VIEWPORT/2)*ch.xr + (y-VIEWPORT+1)*ch.xd;
            wy = cs->y + (x-VIEWPORT/2)*ch.yr + (y-VIEWPORT+1)*ch.yd;

            /* get the cell */
            c = cs->sm.c[si];
            cstateGetCell(&wi, NULL, cs, wx, wy);

            /* separate agent info */
            if (c >= CELL_AGENT && c <= CELL_AGENT_LAST) {
                cs->agents[c-CELL_AGENT].x = wx;
                cs->agents[c-CELL_AGENT].y = wy;
                c = CELL_NONE;
            }

            /* then mark it */
            cs->c[wi] = c;
        }
    }
}

/* perform this action then wait for the next tick */
int cstateDoAndWait(CState *cs, unsigned char act)
{
    int tmpi;
    unsigned char lastTs = cs->sm.ts;
    cs->cm.ts = cs->sm.ts;
    cs->cm.act = act;
    SF(tmpi, writeAll, -1, (1, (char *) &cs->cm, sizeof(ClientMessage)));
    SF(tmpi, readAll, -1, (0, (char *) &cs->sm, sizeof(ServerMessage)));
    cstateUpdate(cs);
    return (cs->sm.ack == ACK_OK);
}

/* create a new CCell off of this CCell in the given direction */
CCell *newCCell(CCell *cur, int card)
{
    CCell *ret;

    SF(ret, malloc, NULL, (sizeof(CCell)));
    memset(ret, 0, sizeof(CCell));

    if (cur) {
        switch (card) {
            case NORTH:
                cur->n = ret;
                ret->s = cur;
                break;

            case EAST:
                cur->e = ret;
                ret->w = cur;
                break;

            case SOUTH:
                cur->s = ret;
                ret->n = cur;
                break;

            case WEST:
                cur->w = ret;
                ret->e = cur;
                break;
        }
    }

    return cur;
}

/* get a CCell off of this CCell in the given direction (will create a new one if it doesn't exist) */
CCell *getCCell(CCell *cur, int card)
{
    CCell *ret;
    switch (card) {
        case NORTH:
            ret = cur->n;
            break;

        case EAST:
            ret = cur->e;
            break;

        case SOUTH:
            ret = cur->s;
            break;

        case WEST:
            ret = cur->w;
            break;
    }

    if (!ret) ret = newCCell(cur, card);

    return ret;
}

/* assert that the ccell map includes this location */
void assertCell(CState *cs, int x, int y)
{
    int xi, yi, dx, dy;
    CCell *cur, *tan, *v;

    if (x >= cs->minx && x <= cs->maxx &&
        y >= cs->miny && y <= cs->maxy) return;

    if (x >= 0) dx = 1; else dx = -1;
    if (y >= 0) dy = 1; else dy = -1;

    /* go row by row, column by column */
    v = cs->home;
    for (yi = dx; yi != y+dy; yi += dy) {
        tan = v;
        v = getCCell(v, (dy > 0) ? SOUTH : NORTH);
        cur = v;
        for (xi = 0; xi != x+dx; xi += dx) {
            cur = getCCell(cur, (dx > 0) ? EAST : WEST);
            v = getCCell(v, (dx > 0) ? EAST : WEST);
            if (dy > 0) {
                v->s = cur;
                cur->n = v;
            } else {
                v->n = cur;
                cur->s = v;
            }
        }
    }

    /* then mark it */
    if (x < cs->minx) cs->minx = x;
    if (x > cs->maxx) cs->maxx = x;
    if (y < cs->miny) cs->miny = y;
    if (y > cs->maxy) cs->maxy = y;
}

/* get a cell id at a specified location, which may be out of bounds */
void cstateGetCell(int *i, CCell **cc, CState *cs, int x, int y)
{
    if (cs->c) {
        if (cc) *cc = NULL;

        /* preferred case, size is known */
        while (x < 0) x += cs->w;
        while (x >= cs->w) x -= cs->w;
        while (y < 0) y += cs->h;
        while (y >= cs->h) y -= cs->h;
        *i = y*cs->w+x;

    } else {
        CCell *cur;
        int xi, yi;

        if (i) *i = -1;

        assertCell(cs, x, y);

        /* start from cur and work our way there */
        cur = cs->cur;
        xi = cs->x;
        yi = cs->y;
        for (; xi < x; xi++) cur = getCCell(cur, EAST);
        for (; xi > x; xi--) cur = getCCell(cur, WEST);
        for (; yi < y; yi++) cur = getCCell(cur, SOUTH);
        for (; yi > y; yi--) cur = getCCell(cur, NORTH);

        *cc = cur;
    }
}
