CC=gcc
CFLAGS=-g -O3
LDFLAGS=

CLIBFLAGS+=`sdl-config --cflags`
LIBS+=`sdl-config --libs`

EXPLORE_OBJS=explore.o rezzoc.o

all: explore

explore: $(EXPLORE_OBJS)
	$(CC) $(CFLAGS) $(CLIBFLAGS) $(LDFLAGS) $(EXPLORE_OBJS) $(LIBS) -o explore

.SUFFIXES: .c .o

%.o: %.c
	$(CC) $(CFLAGS) $(CLIBFLAGS) -c $<

clean:
	rm -f *.o explore
	rm -f deps

-include deps

deps:
	$(CC) -MM *.c > deps || echo > deps
