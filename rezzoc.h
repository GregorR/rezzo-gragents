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

/* flags for finding */
#define FIND_FLAG_WIRE          1
#define FIND_FLAG_STOP_SHORT    2
#define FIND_FLAG_AVOID_BASE    4

BUFFER(ClientMessage, ClientMessage);
BUFFER(ServerMessage, ServerMessage);

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
    unsigned char act; /* expected action (advance or build?) */
    unsigned char hit; /* do we need to hit first? */
    unsigned char hitl, hitr; /* need to hit at least one neighbor first */
    int x, y, card; /* expected state after this step */
};

/* really truly read this much data */
ssize_t readAll(int fd, char *buf, size_t count);

/* really truly write this much data */
ssize_t writeAll(int fd, const char *buf, size_t count);

/* create a CState */
CState *newCState();

/* intialize a CState with its first server message. Determines the world size
 * if w==h==-1 */
void cstateFirstSM(CState *cs, int w, int h);

/* update a CState with a just-received server message */
void cstateUpdate(CState *cs);

/* perform this action then wait for the next tick, returning 1 if the action
 * was successful */
int cstateDoAndWait(CState *cs, unsigned char act);

/* get a cell id at a specified location, and store the real x and y */
int cstateGetCellXY(CState *cs, int x, int y, int *sx, int *sy);

/* get a cell id at a specified location, which may be out of bounds */
int cstateGetCell(CState *cs, int x, int y);

/* get the cell value in front of the agent */
unsigned char cstateGetBlockingCell(CState *cs);

/* find the nearest cell with the given value */
int cstateFindNearest(CState *cs, int *x, int *y, unsigned char type);

/* match our cardinality to the given one */
void matchCardinality(CState *cs, int card);

/* create a path element */
CPath *newPath(int x, int y, int card);

/* free a path element list */
void freePathList(CPath *head, CPath *tail);

/* free a single path element */
void freePath(CPath *path);

/* find a path from the current location to the given location */
CPath *findPath(CState *cs, int tx, int ty, unsigned char act, unsigned char flags);

/* follow this path (and free it). Returns 1 if it was successful, 0 otherwise */
int followPath(CState *cs, CPath *path);

/* JUST GET THERE! */
int findAndGoto(CState *cs, int tx, int ty, unsigned char act, unsigned char flags);

#endif
