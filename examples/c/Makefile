CC ?= cc
CFLAGS ?= -Wall -Wextra -std=c11

.PHONY: all clean

all: rf-example

rf-example: main.c
	$(CC) $(CFLAGS) main.c -lcurl -o rf-example

clean:
	rm -f rf-example
