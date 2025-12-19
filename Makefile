CC = gcc
CFLAGS = -Wextra -std=c99
GAME_LIBS = -lraylib -lm

SHARED_SRC = network.c

SERVER_SRC = server.c
SERVER_OUT = server
GAME_SRC = test_game.c
GAME_OUT = game

all: server game

server: $(SERVER_SRC) $(SHARED_SRC)
	$(CC) $(SERVER_SRC) $(SHARED_SRC) -o $(SERVER_OUT) $(CFLAGS)

game: $(GAME_SRC) $(SHARED_SRC)
	$(CC) $(GAME_SRC) $(SHARED_SRC) -o $(GAME_OUT) $(CFLAGS) $(GAME_LIBS)

clean:
	rm -f $(SERVER_OUT) $(GAME_OUT)
