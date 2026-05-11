CC = gcc
CFLAGS = -Wall -lpthread

all: sync_car

sync_car: sync_car.c sync_car-runner.c
	$(CC) $(CFLAGS) sync_car.c sync_car-runner.c -o sync_car

clean:
	rm -f sync_car