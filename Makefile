CC=gcc
CFLAGS=-O3 -Wall
LIBS=-lz
.PHONY=clean

SOURCE=ntx.c asprintf.c hash_table.c lookup2.c

OBJECT=$(SOURCE:.c=.o)

BIN = ntx

all: $(BIN)
	strip $(BIN)

clean:
	rm -f $(BIN) $(OBJECT)

$(BIN): $(OBJECT)
	$(CC) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
