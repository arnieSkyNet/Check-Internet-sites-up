CC = gcc
CFLAGS = -Wall -O2

all: chkinetup

chkinetup: chkinetup.c
	$(CC) $(CFLAGS) -o chkinetup chkinetup.c

clean:
	rm -f chkinetup

