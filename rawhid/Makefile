PROG = macropad-driver

CC = clang
CFLAGS = -Wall -O3
LIBS = -lusb -lasound
STRIP = strip

all: $(PROG)

$(PROG): main.c
	$(CC) $(CFLAGS) -o $(PROG) main.c $(LIBS) hid_LINUX.c
	$(STRIP) $(PROG)

clean:
	rm -f $(PROG)

.PHONY: all clean