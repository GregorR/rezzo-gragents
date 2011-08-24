CC=gcc
CFLAGS=-g -O3
LDFLAGS=

CLIBFLAGS+=`sdl-config --cflags`
LIBS+=`sdl-config --libs`

# You may optionally statically specify world size
WW=-1
WH=-1
WCFLAGS=-DWW=$(WW) -DWH=$(WH)

EXPLORE_OBJS=explore.o rezzoc.o

all: explore

explore: $(EXPLORE_OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(LDFLAGS) $(EXPLORE_OBJS) $(LIBS) -o explore

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(WCFLAGS) -c $<

clean:
	rm -f *.o explore
	rm -f deps

-include deps

deps:
	$(CC) -MM *.c > deps || echo > deps
