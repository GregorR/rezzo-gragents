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
#include <string.h>

#include "rezzoc.h"

#include "../buffer.h"

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
    ret->x = ret->y = ret->card = 0;

    ret->id = -1; /* unknown */
    for (i = 0; i < MAX_AGENTS; i++) {
        ret->agents[i].x = ret->agents[i].y = -1; /* unknown */
    }

    /* this must be filled in some time! */
    ret->c = NULL;

    return ret;
}

/* intialize a CState with its first server message */
void cstateFirstSM(CState *cs, int w, int h)
{
    readAll(0, (char *) &cs->sm, sizeof(ServerMessage));

    /* set our CM to nop (just in case) */
    cs->cm.ts = cs->sm.ts - 1;
    cs->cm.act = ACT_NOP;

    if (w == -1) {
        struct Buffer_ClientMessage bcm;
        struct Buffer_ServerMessage bsm;
        int tmpi, i;
        unsigned char cl, cr, dcl, dcr;

        INIT_BUFFER(bcm);
        INIT_BUFFER(bsm);
        WRITE_ONE_BUFFER(bcm, cs->cm);
        WRITE_ONE_BUFFER(bsm, cs->sm);

        /* first determine the world height */
        while (1) {
            /* move forward */
            cs->cm.ts = cs->sm.ts;
            if (cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2)] == CELL_NONE) {
                cs->cm.act = ACT_ADVANCE;
            } else {
                cs->cm.act = ACT_HIT;
            }

            SF(tmpi, writeAll, -1, (1, (char *) &cs->cm, sizeof(ClientMessage)));
            SF(tmpi, readAll, -1, (0, (char *) &cs->sm, sizeof(ServerMessage)));

            /* put them in the buffer */
            WRITE_ONE_BUFFER(bcm, cs->cm);
            WRITE_ONE_BUFFER(bsm, cs->sm);

            /* then handle the message */
            if (cs->sm.ts == ((cs->cm.ts + 1) & 0xFF) &&
                cs->cm.act == ACT_ADVANCE &&
                cs->sm.ack == ACK_OK) {
                cs->y--;
            }

            /* should we stop? */
            cl = cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2) - 1];
            cr = cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2) + 1];
            if (cl >= CELL_BASE && cl <= CELL_BASE_LAST &&
                cr >= CELL_BASE && cr <= CELL_BASE_LAST) {
                /* we're done! */
                cs->h = -cs->y;
                cs->y = 0;
                break;
            }
        }

        /* turn right */
        while (1) {
            /* move forward */
            cs->cm.ts = cs->sm.ts;
            cs->cm.act = ACT_TURN_RIGHT;

            SF(tmpi, writeAll, -1, (1, (char *) &cs->cm, sizeof(ClientMessage)));
            SF(tmpi, readAll, -1, (0, (char *) &cs->sm, sizeof(ServerMessage)));

            /* put them in the buffer */
            WRITE_ONE_BUFFER(bcm, cs->cm);
            WRITE_ONE_BUFFER(bsm, cs->sm);

            /* then handle the message */
            if (cs->sm.ts == ((cs->cm.ts + 1) & 0xFF) && cs->sm.ack == ACK_OK) {
                cs->card++;
                break;
            }
        }

        /* then determine the world width */
        while (1) {
            /* move forward */
            cs->cm.ts = cs->sm.ts;
            if (cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2)] == CELL_NONE) {
                cs->cm.act = ACT_ADVANCE;
            } else {
                cs->cm.act = ACT_HIT;
            }

            SF(tmpi, writeAll, -1, (1, (char *) &cs->cm, sizeof(ClientMessage)));
            SF(tmpi, readAll, -1, (0, (char *) &cs->sm, sizeof(ServerMessage)));

            /* put them in the buffer */
            WRITE_ONE_BUFFER(bcm, cs->cm);
            WRITE_ONE_BUFFER(bsm, cs->sm);

            /* then handle the message */
            if (cs->sm.ts == ((cs->cm.ts + 1) & 0xFF) &&
                cs->cm.act == ACT_ADVANCE &&
                cs->sm.ack == ACK_OK) {
                cs->x++;
            }

            /* should we stop? */
            cl = cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2) - 1];
            cr = cs->sm.c[(VIEWPORT-2)*VIEWPORT + (VIEWPORT/2) + 1];
            dcl = cs->sm.c[(VIEWPORT-4)*VIEWPORT + (VIEWPORT/2) - 1];
            dcr = cs->sm.c[(VIEWPORT-4)*VIEWPORT + (VIEWPORT/2) + 1];
            if (cl >= CELL_BASE && cl <= CELL_BASE_LAST &&
                cr >= CELL_FLAG_GEYSER && cr <= CELL_FLAG_GEYSER_LAST &&
                !(dcl >= CELL_BASE && dcl <= CELL_BASE_LAST) &&
                !(dcr >= CELL_FLAG_GEYSER && dcl <= CELL_FLAG_GEYSER_LAST)) {
                /* we're done! */
                cs->w = cs->x;
                cs->x = 0;
                cs->card = NORTH;
                break;
            }
        }

        /* make room */
        w = cs->w;
        h = cs->h;
        SF(cs->c, malloc, NULL, (w*h));
        memset(cs->c, CELL_UNKNOWN, w*h);

        /* and handle all these messages */
        for (i = 0; i < bcm.bufused; i++) {
            cs->cm = bcm.buf[i];
            cs->sm = bsm.buf[i];
            cstateUpdate(cs);
        }

        FREE_BUFFER(bcm);
        FREE_BUFFER(bsm);

    } else {
        SF(cs->c, malloc, NULL, (w*h));
        memset(cs->c, CELL_UNKNOWN, w*h);
        cs->w = w;
        cs->h = h;

        /* now just integrate the data */
        cstateUpdate(cs);
    }
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
                wi = cstateGetCell(cs, cs->x, cs->y);
                cs->c[wi] = CELL_CONDUCTOR;
                cs->x -= ch.xd;
                cs->y -= ch.yd;
                break;
        }

        /* make sure x and y are consistent */
        cstateGetCellXY(cs, cs->x, cs->y, &cs->x, &cs->y);

    } else {
        fprintf(stderr, "Error %d for action %c\n", (int) cs->sm.ack, cs->cm.act);
        if (cs->sm.ack != ACK_NO_MESSAGE) abort();

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
            wi = cstateGetCell(cs, wx, wy);

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
    cs->cm.ts = cs->sm.ts;
    cs->cm.act = act;
    SF(tmpi, writeAll, -1, (1, (char *) &cs->cm, sizeof(ClientMessage)));
    SF(tmpi, readAll, -1, (0, (char *) &cs->sm, sizeof(ServerMessage)));
    cstateUpdate(cs);
    return (cs->sm.ack == ACK_OK);
}

/* get a cell id at a specified location, and store the real x and y */
int cstateGetCellXY(CState *cs, int x, int y, int *sx, int *sy)
{
    while (x < 0) x += cs->w;
    while (x >= cs->w) x -= cs->w;
    while (y < 0) y += cs->h;
    while (y >= cs->h) y -= cs->h;
    if (sx) {
        *sx = x;
        *sy = y;
    }
    return y*cs->w+x;
}

/* get a cell id at a specified location, which may be out of bounds */
int cstateGetCell(CState *cs, int x, int y)
{
    return cstateGetCellXY(cs, x, y, NULL, NULL);
}

/* get the cell value in front of the agent */
unsigned char cstateGetBlockingCell(CState *cs)
{
    CardinalityHelper ch = cardinalityHelpers[cs->card];
    return cs->c[cstateGetCell(cs, cs->x-ch.xd, cs->y-ch.yd)];
}

/* find the nearest cell with the given value */
int cstateFindNearest(CState *cs, int *sx, int *sy, unsigned char type)
{
    int r, x, y, rx, ry, i;

    for (r = 1; r < cs->w || r < cs->h; r++) {
        for (y = -r; y < r; y++) {
            for (x = -r; x < r; x++) {
                i = cstateGetCellXY(cs, cs->x+x, cs->y+y, &rx, &ry);
                if (cs->c[i] == type) {
                    /* gotcha! */
                    *sx = rx;
                    *sy = ry;
                    return i;
                }
            }
        }
    }

    *sx = -1;
    *sy = -1;
    return -1;
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

/* indestructable types */
static int indestructable(unsigned char c)
{
    return (c >= CELL_AGENT && c <= CELL_AGENT_LAST) ||
           (c >= CELL_FLAG_GEYSER && c <= CELL_FLAG_GEYSER_LAST) ||
           (c >= CELL_BASE && c <= CELL_BASE_LAST);
}

/* find a path from the current location to the given location */
CPath *findPath(CState *cs, int tx, int ty, unsigned char dact, unsigned char flags)
{
    CPath *cur, *next, *good;
    CPath *toSearchHead, *toSearchTail;
    CPath *searchedHead, *searchedTail;
    unsigned char *searched;
    int scard, sx, sy, si, scost;
    unsigned char c, act, hit, hitl, hitr;

    /* our initial searchlist is just the starting position */
    cur = toSearchHead = toSearchTail = newPath(cs->x, cs->y, cs->card);
    cur->gScore = cur->fScore = 0;
    cur->act = ACT_NOP;
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

        /* search surrounding areas */
        for (scard = 0; scard < CARDINALITIES; scard++) {
            CardinalityHelper ch = cardinalityHelpers[scard];

            /* take a step */
            sx = cur->x - ch.xd;
            sy = cur->y - ch.yd;
            si = cstateGetCellXY(cs, sx, sy, &sx, &sy);
            if (searched[si]) continue;
            searched[si] = 1;
            c = cs->c[si];
            act = dact;
            hit = 0;
            hitl = hitr = 0;

            /* ignore impenatrable things */
            if (indestructable(c)) continue;

            /* cost of this action */
            scost = 1; /* to move */
            if (scard != cur->card) scost++; /* to turn */
            if (c != CELL_NONE && c != (unsigned char) CELL_UNKNOWN) {
                /* to hit */
                hit = 1;
                scost += CELL_DESTROY_DAMAGE;
            }

            /* more complicated for building wires */
            if (flags & FIND_FLAG_WIRE) {
                int nx, ny, ni;

                /* left */
                nx = cur->x - ch.xr;
                ny = cur->y - ch.yr;
                ni = cstateGetCell(cs, nx, ny);
                if (cs->c[ni] != CELL_NONE && !indestructable(cs->c[ni])) hitl = 1;

                /* right */
                nx = cur->x + ch.xr;
                ny = cur->y + ch.yr;
                ni = cstateGetCell(cs, nx, ny);
                if (cs->c[ni] != CELL_NONE && !indestructable(cs->c[ni])) hitr = 1;
            }
            scost += hitl * (2+CELL_DESTROY_DAMAGE) + hitr * (2+CELL_DESTROY_DAMAGE);

            /* and more complicated for avoiding the base */
            if (flags & FIND_FLAG_AVOID_BASE) {
                if (sx <= 2 || sy <= 2 ||
                    sx >= cs->w - 2 || sy >= cs->h - 2)
                    scost += 1024; /* arbitrary large value */
            }

            /* now make the object for it */
            next = newPath(sx, sy, scard);
            next->act = act;
            next->hit = hit;
            next->hitl = hitl;
            next->hitr = hitr;
            next->prev = cur;
            next->gScore = cur->gScore + scost;
            next->fScore = next->gScore + estCost(cs, sx, sy, tx, ty);
            insertByFScore(&toSearchHead, &toSearchTail, next);

        }
    }

    /* if we have a good list, organize it properly */
    if (good && good->prev) {
        if ((flags & FIND_FLAG_STOP_SHORT) && good->prev) {
            cur = good->prev;
            freePath(good);
            good = cur;
        }

        if (good->prev) {
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
    }

    /* free up our lists */
    if (toSearchHead) freePathList(toSearchHead, toSearchTail);
    if (searchedHead) freePathList(searchedHead, searchedTail);
    free(searched);

    return good;
}

static int hitItTilItDies(CState *cs, int i)
{
    while (cs->c[i] != CELL_NONE) {
        if (!cstateDoAndWait(cs, ACT_HIT)) {
            if (cs->sm.ack != ACK_NO_MESSAGE) {
                return 0;
            }
        }
    }

    return 1;
}

/* follow this path (and free it). Returns 1 if it was successful, 0 otherwise */
int followPath(CState *cs, CPath *path)
{
    int succ = 1;
    unsigned char nact = 0;
    CPath *last;
    while (path) {
        int i = path->y*cs->w + path->x;
        int ni, tcard;
        CardinalityHelper ch = cardinalityHelpers[path->card];

        /* make our cardinality match */
        if (cs->card != path->card) {
            matchCardinality(cs, path->card);
        }

        /* check if our expectations are met */
        if (!path->hit) {
            /* shouldn't have an obstacle */
            if (cs->c[i] != CELL_NONE) {
                /* oh dear! */
                succ = 0;
                goto fail;
            }
        }

        /* hit if we need to */
        if (!hitItTilItDies(cs, i)) {
            succ = 0;
            goto fail;
        }

        /* hit left/right if we need to */
        if (path->hitl) {
            ni = cstateGetCell(cs, cs->x - ch.xr, cs->y - ch.yr);
            if (cs->c[ni] != CELL_NONE) {
                tcard = path->card - 1;
                if (tcard < 0) tcard += CARDINALITIES;
                matchCardinality(cs, tcard);
                if (!hitItTilItDies(cs, ni)) { succ = 0; goto fail; }
            }
            matchCardinality(cs, path->card);
        }
        if (path->hitr) {
            ni = cstateGetCell(cs, cs->x + ch.xr, cs->y + ch.yr);
            if (cs->c[ni] != CELL_NONE) {
                tcard = path->card + 1;
                if (tcard >= CARDINALITIES) tcard -= CARDINALITIES;
                matchCardinality(cs, tcard);
                if (!hitItTilItDies(cs, ni)) { succ = 0; goto fail; }
            }
            matchCardinality(cs, path->card);
        }

        /* then go */
        while (!cstateDoAndWait(cs, nact ? nact : path->act)) {
            if (cs->sm.ack != ACK_NO_MESSAGE) {
                succ = 0;
                goto fail;
            }
        }
        nact = 0;
        if (path->hit) nact = ACT_BUILD;

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
int findAndGoto(CState *cs, int tx, int ty, unsigned char act, unsigned char wire)
{
    while (1) {
        CPath *path = findPath(cs, tx, ty, act, wire);
        if (path == NULL) return 0;
        /*printPath(path);
        fprintf(stderr, "\n");*/
        if (followPath(cs, path)) break;
    }
    return 1;
}
