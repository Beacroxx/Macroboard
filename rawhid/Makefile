PROG = macropad-driver

CC = clang++
CFLAGS = -Wall -O3
LIBS = -lasound
STRIP = strip

all: $(PROG)

$(PROG): main.cpp
	$(CC) $(CFLAGS) -o $(PROG) main.cpp $(LIBS)
	$(STRIP) $(PROG)

clean:
	rm -f $(PROG)

.PHONY: all clean
