CC = gcc
FLAGS = -Wextra -std=c99
SRC = network.c
TARGET = netw

GAME_SRC = test_game.c
GAME_FLAGS = -Wextra -std=c99 -lraylib
GAME_TARGET = game

default: network

network: $(SRC)
	clear
	$(CC) $(SRC) -o $(TARGET) $(FLAGS)

game: $(GAME_SRC)
	clear
	$(CC) $(GAME_SRC) -o $(GAME_TARGET) $(GAME_FLAGS)

clean:
	rm -f $(TARGET)
	rm -f $(GAME_TARGET)
