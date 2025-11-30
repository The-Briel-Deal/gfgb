CC = gcc
CFLAGS = -lSDL3 -g

gb_emu: src/main.c
	$(CC) $(CFLAGS) -o $@ $^
