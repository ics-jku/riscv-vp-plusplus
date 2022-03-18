CC=g++
CFLAGS=-std=c++14 -g2

all: cli-server cli-client

gpiocommon.o: gpiocommon.cpp
	$(CC) $(CFLAGS) -c gpiocommon.cpp

gpio-server.o: gpio-server.cpp
	$(CC) $(CFLAGS) -c gpio-server.cpp

gpio-client.o: gpio-client.cpp
	$(CC) $(CFLAGS) -c gpio-client.cpp #-lpthread need is from here

cli-client: gpio-client.o gpiocommon.o cli-client.cpp
	$(CC) $(CFLAGS) -o cli-client cli-client.cpp gpio-client.o gpiocommon.o -lpthread

cli-server: gpio-server.o gpiocommon.o gpio-client.o cli-server.cpp  # client just for sincere disconnect
	$(CC) $(CFLAGS) -o cli-server cli-server.cpp gpio-server.o gpio-client.o gpiocommon.o -lpthread

clean:
	rm -rf *.o
	rm -rf cli-client cli-server
