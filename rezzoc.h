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

#ifndef REZZOC_H
#define REZZOC_H

/* this must be checked out as a subdir of rezzo */
#include "../agent.h"

/* extra cell values */
#define CELL_UNKNOWN '\xFF'
#define CELL_AGENT_LAST '9'
#define CELL_FLAG_LAST 'J'
#define CELL_FLAG_GEYSER_LAST 'j'
#define CELL_BASE_LAST 'u'

/* highly-inefficient graph layout hooray! (for before size is known) */
typedef struct _CCell CCell;
struct _CCell {
    CCell *n, *e, *s, *w; /* ne, nw, etc can be derived */
    unsigned char c; /* the actual cell value */
};

/* agent info */
typedef struct _CAgent CAgent;
struct _CAgent {
    int x, y;
};

/* player state */
typedef struct _CState CState;
struct _CState {
    unsigned char id; /* player id */
    int x, y, card; /* player location */
    CAgent agents[MAX_AGENTS]; /* other agents */

    int minx, miny, maxx, maxy; /* should all start at 0 */
    CCell *home, *cur; /* starting position and current cell */

    int w, h; /* -1 for unknown */
    unsigned char *c; /* the full map, when size is known */

    ClientMessage cm; /* the last client message sent */
    ServerMessage sm; /* the last server message received */
};

/* path (step) for pathfinding */
typedef struct _CPath CPath;
struct _CPath {
    CPath *prev, *next; /* previous, next step if following this path */
    CPath *lprev, *lnext; /* previous, next where this is stored on a list */
    int gScore, fScore; /* cost to get here, estimate cost of getting here + getting to goal */
    unsigned char act; /* expected action (hit or just advance?) */
    int x, y, card; /* expected state after this step */
};

/* really truly read this much data */
ssize_t readAll(int fd, char *buf, size_t count);

/* really truly write this much data */
ssize_t writeAll(int fd, const char *buf, size_t count);

/* create a CState */
CState *newCState();

/* intialize a CState with its first server message */
void cstateFirstSM(CState *cs, int w, int h);

/* update a CState with a just-received server message */
void cstateUpdate(CState *cs);

/* perform this action then wait for the next tick, returning 1 if the action
 * was successful */
int cstateDoAndWait(CState *cs, unsigned char act);

/* create a new CCell off of this CCell in the given direction */
CCell *newCCell(CCell *cur, int card);

/* get a CCell off of this CCell in the given direction (will create a new one if it doesn't exist) */
CCell *getCCell(CCell *cur, int card);

/* get a cell id at a specified location, which may be out of bounds */
void cstateGetCell(int *i, CCell **cc, CState *cs, int x, int y);

/* match our cardinality to the given one */
void matchCardinality(CState *cs, int card);

/* create a path element */
CPath *newPath(int x, int y, int card);

/* free a path element list */
void freePathList(CPath *head, CPath *tail);

/* free a single path element */
void freePath(CPath *path);

/* find a path from the current location to the given location */
CPath *findPath(CState *cs, int tx, int ty);

/* follow this path (and free it). Returns 1 if it was successful, 0 otherwise */
int followPath(CState *cs, CPath *path);

/* JUST GET THERE! */
int findAndGoto(CState *cs, int tx, int ty);

#endif
