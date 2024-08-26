PROG = macropad-driver

CC = clang
CFLAGS = -Wall -O3 -Wno-incompatible-pointer-types
LIBS = -lasound
STRIP = strip

all: $(PROG)

$(PROG): main.c
	$(CC) $(CFLAGS) -o $(PROG) main.c $(LIBS)
	$(STRIP) $(PROG)

clean:
	rm -f $(PROG)

.PHONY: all clean