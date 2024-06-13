
CC = g++
CFLAGS = -I. -O2 -Wall -static
DEPS = i2c.h ugreen_leds.h ugreen_leds_i2c.h ugreen_leds_kmod.h
OBJ = i2c.o ugreen_leds.o ugreen_leds_i2c.o ugreen_leds_kmod.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

ugreen_leds_cli: $(OBJ) ugreen_leds_cli.o
	$(CC) -o $@ $^ $(CFLAGS)
