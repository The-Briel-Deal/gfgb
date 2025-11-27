CC = gcc
CFLAGS = -lSDL3

gb_emu: src/main.c
	$(CC) $(CFLAGS) -o $@ $^
