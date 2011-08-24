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

    unsigned char c, a;
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
                a = c-CELL_AGENT;
                if (cs->agents[a].x >= 0) {
                    /* they're not there any more! */
                    cs->c[
                        cs->agents[a].y*cs->w +
                        cs->agents[a].x
                    ] = CELL_NONE;
                }
                cs->agents[c-CELL_AGENT].x = wx;
                cs->agents[c-CELL_AGENT].y = wy;
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

/* match our cardinality to the given one */
void matchCardinality(CState *cs, int card)
{
    int cdiff = card - cs->card;
    unsigned char act = ACT_TURN_RIGHT;

    /* figure out which way to turn */
    while (cdiff < 0) cdiff += CARDINALITIES;
    if (cdiff > 2) act = ACT_TURN_LEFT;

    /* then do it */
    while (cs->card != card)
        cstateDoAndWait(cs, act);
}

static CPath *pathHead = NULL, *pathTail = NULL;

/* create a path element */
CPath *newPath(int x, int y, int card)
{
    CPath *ret;
    if (pathHead) {
        ret = pathHead;
        pathHead = ret->lnext;

    } else {
        SF(ret, malloc, NULL, (sizeof(CPath)));

    }

    memset(ret, 0, sizeof(CPath));
    ret->x = x;
    ret->y = y;
    ret->card = card;

    return ret;
}

/* free a path element list */
void freePathList(CPath *head, CPath *tail)
{
    if (pathTail) {
        head->lprev = pathTail;
        pathTail->lnext = head;

    } else {
        pathHead = head;

    }

    pathTail = tail;
}

/* free a single path element */
void freePath(CPath *path)
{
    if (pathTail) {
        path->lprev = pathTail;
        pathTail->lnext = path;

    } else {
        pathHead = path;

    }

    pathTail = path;
}

/* estimated cost for this motion */
static int estCost(CState *cs, int fx, int fy, int tx, int ty)
{
    int xdist = tx - fx;
    int ydist = ty - fy;
    int hw = cs->w/2;
    int hh = cs->h/2;

    /* compute the shortest wrapping distance */
    while (xdist < -hw) xdist += cs->w;
    while (xdist >= hw) xdist -= cs->w;
    while (ydist < -hh) ydist += cs->h;
    while (ydist >= hh) ydist -= cs->h;

    /* must be positive */
    if (xdist < 0) xdist = -xdist;
    if (ydist < 0) ydist = -ydist;

    return xdist + ydist;
}

static void insertByFScore(CPath **headp, CPath **tailp, CPath *insert)
{
    CPath *head = *headp;
    CPath *tail = *tailp;

    if (!head) {
        /* simplest case */
        head = tail = insert;

    } else {
        CPath *cur = head;
        for (; cur && cur->fScore <= insert->fScore; cur = cur->lnext);
        if (!cur) {
            /* insert goes on the tail */
            tail->lnext = insert;
            insert->lprev = tail;
            tail = insert;

        } else if (!cur->lprev) {
            /* insert goes on the head */
            head->lprev = insert;
            insert->lnext = head;
            head = insert;

        } else {
            /* insert goes in the middle */
            insert->lprev = cur->lprev;
            insert->lnext = cur;
            cur->lprev->lnext = insert;
            cur->lprev = insert;

        }

    }

    *headp = head;
    *tailp = tail;
}

static void removeFromList(CPath **headp, CPath **tailp, CPath *torem)
{
    CPath *head = *headp;
    CPath *tail = *tailp;

    if (head == torem) {
        if (tail == torem) {
            /* list is gone! */
            head = tail = NULL;

        } else {
            head = torem->lnext;
            torem->lnext = NULL;
            head->lprev = NULL;

        }

    } else if (tail == torem) {
        tail = torem->lprev;
        torem->lprev = NULL;
        tail->lnext = NULL;

    } else {
        torem->lprev->lnext = torem->lnext;
        torem->lnext->lprev = torem->lprev;
        torem->lprev = torem->lnext = NULL;

    }

    *headp = head;
    *tailp = tail;
}

/* find a path from the current location to the given location */
CPath *findPath(CState *cs, int tx, int ty)
{
    CPath *cur, *next, *good;
    CPath *toSearchHead, *toSearchTail;
    CPath *searchedHead, *searchedTail;
    unsigned char *searched;
    int scard, sx, sy, si, scost;
    unsigned char c, act;

    /* our initial searchlist is just the starting position */
    cur = toSearchHead = toSearchTail = newPath(cs->x, cs->y, cs->card);
    cur->gScore = cur->fScore = 0;
    good = NULL;
    searchedHead = searchedTail = NULL;

    SF(searched, malloc, NULL, (cs->w * cs->h));
    memset(searched, 0, cs->w*cs->h);

    /* now search! */
    while (toSearchHead) {
        /* remove it */
        cur = toSearchHead;
        removeFromList(&toSearchHead, &toSearchTail, cur);

        /* is it good? */
        if (cur->x == tx && cur->y == ty) {
            /* done! */
            good = cur;
            break;
        }

        /* add it to the searched list */
        insertByFScore(&searchedHead, &searchedTail, cur);
        searched[cur->y*cs->w+cur->x] = 1;

        /* search surrounding areas */
        for (scard = 0; scard < CARDINALITIES; scard++) {
            CardinalityHelper ch = cardinalityHelpers[scard];

            /* take a step */
            sx = cur->x - ch.xd;
            sy = cur->y - ch.yd;
            if (sx < 0) sx += cs->w;
            if (sx >= cs->w) sx -= cs->w;
            if (sy < 0) sy += cs->h;
            if (sy >= cs->h) sy -= cs->h;
            si = sy*cs->w + sx;
            if (searched[si]) continue;
            c = cs->c[si];
            act = ACT_ADVANCE;

            /* ignore impenatrable things */
            if ((c >= CELL_AGENT && c <= CELL_AGENT_LAST) ||
                (c >= CELL_FLAG && c <= CELL_FLAG_LAST) ||
                (c >= CELL_FLAG_GEYSER && c <= CELL_FLAG_GEYSER_LAST) ||
                (c >= CELL_BASE && c <= CELL_BASE_LAST)) continue;

            /* cost of this action */
            scost = 1; /* to move */
            if (scard != cur->card) scost++; /* to turn */
            if (c != CELL_NONE && c != (unsigned char) CELL_UNKNOWN) {
                /* to hit */
                act = ACT_HIT;
                scost += 4;
            }

            /* now make the object for it */
            next = newPath(sx, sy, scard);
            next->act = act;
            next->prev = cur;
            next->gScore = cur->gScore + scost;
            next->fScore = next->gScore + estCost(cs, sx, sy, tx, ty);
            insertByFScore(&toSearchHead, &toSearchTail, next);

        }
    }

    /* if we have a good list, organize it properly */
    if (good && good->prev) {
        good->prev->next = good;
        for (cur = good->prev;; cur = cur->prev) {
            /* remove it from the searched list so it doesn't get freed */
            removeFromList(&searchedHead, &searchedTail, cur);

            if (cur->prev) {
                /* hook it the other way */
                cur->prev->next = cur;
            } else break;
        }

        good = cur->next;
        good->prev = NULL;

        /* first step will always be a no-op */
        freePath(cur);
    }

    /* free up our lists */
    if (toSearchHead) freePathList(toSearchHead, toSearchTail);
    if (searchedHead) freePathList(searchedHead, searchedTail);
    free(searched);

    return good;
}

/* follow this path (and free it). Returns 1 if it was successful, 0 otherwise */
int followPath(CState *cs, CPath *path)
{
    int succ = 1;
    CPath *last;
    while (path) {
        int i = path->y*cs->w + path->x;

        /* make our cardinality match */
        if (cs->card != path->card)
            matchCardinality(cs, path->card);

        /* check if our expectations are met */
        if (path->act == ACT_ADVANCE) {
            /* shouldn't have an obstacle */
            if (cs->c[i] != CELL_NONE) {
                /* oh dear! */
                succ = 0;
                goto fail;
            }
        }

        /* hit if we need to */
        if (path->act == ACT_HIT) {
            while (cs->c[i] != CELL_NONE) {
                if (!cstateDoAndWait(cs, ACT_HIT)) {
                    if (cs->sm.ack != ACK_NO_MESSAGE) {
                        succ = 0;
                        goto fail;
                    }
                }
            }
        }

        /* then go */
        while (!cstateDoAndWait(cs, ACT_ADVANCE)) {
            if (cs->sm.ack != ACK_NO_MESSAGE) {
                succ = 0;
                goto fail;
            }
        }

        /* make sure all went well */
        if (cs->x != path->x || cs->y != path->y) {
            /* oh no! */
            succ = 0;
            goto fail;
        }

        /* success! Move on */
        last = path;
        path = path->next;
        if (path) path->prev = NULL;
        freePath(last);
    }
fail:

    /* free anything left over */
    while (path) {
        last = path;
        path = path->next;
        freePath(last);
    }

    return succ;
}

/*
static void printPath(CPath *path)
{
    fprintf(stderr, "%c", path->act);
    if (path->next) printPath(path->next);
}
*/

/* JUST GET THERE! */
int findAndGoto(CState *cs, int tx, int ty)
{
    while (cs->x != tx || cs->y != ty) {
        CPath *path = findPath(cs, tx, ty);
        if (path == NULL) return 0;
        /*printPath(path);
        fprintf(stderr, "\n");*/
        followPath(cs, path);
    }
    return 1;
}
