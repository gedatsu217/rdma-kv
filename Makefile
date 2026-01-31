CC      := gcc
CFLAGS  := -O2 -g -Wall -Wextra
LDFLAGS := -lrdmacm -libverbs

SRCS_LIB := kvlib.c
OBJS_LIB := kvlib.o

SRCS_SERVER := server.c
SRCS_CLIENT := client.c

BIN_SERVER := server
BIN_CLIENT := client

.PHONY: all clean

all: $(BIN_SERVER) $(BIN_CLIENT)

$(OBJS_LIB): $(SRCS_LIB) kvlib.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_SERVER): $(SRCS_SERVER) $(OBJS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_CLIENT): $(SRCS_CLIENT) $(OBJS_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o $(BIN_SERVER) $(BIN_CLIENT)
