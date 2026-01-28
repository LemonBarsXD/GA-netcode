CC = gcc
VPATH = src
CFLAGS = -Wextra -std=gnu99 -Isrc -Ipthread -lGL -ldl -lm

BIN_PATH = bin/

SHARED_SRC = network.c client.c
SERVER_SRC = server.c
SERVER_OUT = server

GAME_SRC = test_game.c
GAME_OUT = game
GAME_LIBS = -lraylib

all: bin server game

server: $(SERVER_SRC) $(SHARED_SRC)
	$(CC) $^ -o $(BIN_PATH)$(SERVER_OUT) $(CFLAGS)

game: $(GAME_SRC) $(SHARED_SRC)
	$(CC) $^ -o $(BIN_PATH)$(GAME_OUT) $(CFLAGS) $(GAME_LIBS)

bin:
	@mkdir -p $(BIN_PATH)

clean:
	rm -f $(SERVER_OUT) $(GAME_OUT)
