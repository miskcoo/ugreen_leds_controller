
CC = g++
CFLAGS = -I. -O2 -Wall -static
DEPS = i2c.h ugreen_leds.h ugreen_leds_i2c.h ugreen_leds_kmod.h ugreen_leds_socket.h
OBJ = i2c.o ugreen_leds_i2c.o ugreen_leds_kmod.o ugreen_leds_socket.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: ugreen_leds_cli ugreen_daemon

ugreen_leds_cli: $(OBJ) ugreen_leds_cli.o
	$(CC) -o $@ $^ $(CFLAGS)

ugreen_daemon: $(OBJ) ugreen_daemon.o
	$(CC) -o $@ $^ $(CFLAGS)
