
CC = g++
CFLAGS = -I. -O2 -Wall
DEPS = i2c.h dx4600_leds.h
OBJ = i2c.o dx4600_leds.o main.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

dx4600_leds: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
