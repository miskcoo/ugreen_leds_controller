
CC = g++
CFLAGS = -I ./include/ -O2 -Wall -static -std=c++17
DEPS = include/*.h
OBJ = utils/i2c.o cli/leds_i2c.o cli/leds_kmod.o cli/leds_socket.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: ugreen_leds_cli ugreen_daemon

ugreen_leds_cli: $(OBJ) cli/main.o
	$(CC) -o $@ $^ $(CFLAGS)

ugreen_daemon: $(OBJ) daemon/main.o
	$(CC) -o $@ $^ $(CFLAGS)
