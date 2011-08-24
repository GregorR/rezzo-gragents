CC=gcc
CFLAGS=-g -O3
LDFLAGS=

# You may optionally statically specify world size
WW=-1
WH=-1
WCFLAGS=-DWW=$(WW) -DWH=$(WH)

EXPLORE_OBJS=explore.o rezzoc.o

all: explore

explore: $(EXPLORE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(EXPLORE_OBJS) -o explore

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(WCFLAGS) -c $<

clean:
	rm -f *.o explore
	rm -f deps

-include deps

deps:
	$(CC) -MM *.c > deps || echo > deps
