CC = gcc
CFLAGS = -Wall -Wextra -g

OBJS = lwmp.o rr.o magic64.o

all: $(OBJS)

lwmp.o: lwmp.c lwp.h rr.h
	$(CC) $(CFLAGS) -c lwmp.c

rr.o: rr.c lwp.h rr.h
	$(CC) $(CFLAGS) -c rr.c

magic64.o: magic64.S
	$(CC) $(CFLAGS) -c magic64.S

clean:
	rm -f $(OBJS)
