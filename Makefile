CFILES = $(shell find src/ -name "*.c")
DFILES = $(CFILES:.c=.d)
OFILES = $(CFILES:.c=.o)

CFLAGS = -Wall -pedantic -Iinc
CC = gcc

.PHONY: all
all: $(OFILES)
	$(CC) $^ -o bup

-include $(DFILES)
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(OFILES) $(DFILES)
