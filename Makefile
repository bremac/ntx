CC=gcc
CFLAGS=-O3 -Wall
LIBS=-lz
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
.PHONY=clean install

SOURCE=ntx.c hash_table.c lookup2.c
SYSTEM=unix.c

OBJECT=$(SOURCE:.c=.o) $(SYSTEM:.c=.o)

BIN=ntx

all: $(BIN)
	strip $(BIN)

clean:
	rm -f $(BIN) $(OBJECT)

install: $(BIN)
	install -d $(BINDIR)
	install $(BIN) $(BINDIR)

$(BIN): $(OBJECT)
	$(CC) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
