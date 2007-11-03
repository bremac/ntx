CC=gcc
CFLAGS=-O3 -Wall
LIBS=-lz
prefix=/usr/local
bindir=$(prefix)/bin
.PHONY=clean install test

SOURCE=src/ntx.c src/hash_table.c src/lookup2.c
SYSTEM=src/unix.c

OBJECT=$(SOURCE:.c=.o) $(SYSTEM:.c=.o)

BIN=ntx

all: $(BIN)
	strip $(BIN)

clean:
	rm -f $(BIN) $(OBJECT)

install: $(BIN)
	install -d $(bindir)
	install $(BIN) $(bindir)

test: $(BIN)
	@cd tests && bash test.sh && echo "All tests passed successfully."

$(BIN): $(OBJECT)
	$(CC) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
