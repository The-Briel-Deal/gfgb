CC = gcc
CFLAGS = -lSDL3 -g -Isrc

gb_emu: src/main.c src/cpu.c
	$(CC) $(CFLAGS) -o $@ $^
