# PizzaFool Makefile for Solaris 7 / CDE / SPARCstation
#
# Requires: Motif (libXm), X11 (libX11, libXt)
# These are standard on Solaris 7 with CDE installed.

# Use tgcware GCC if available, fall back to system cc
CC = /usr/tgcware/gcc47/bin/gcc
#CC = cc

CFLAGS = -O2 -I/usr/dt/include -I/usr/openwin/include
LDFLAGS = -L/usr/dt/lib -L/usr/openwin/lib -R/usr/dt/lib -R/usr/openwin/lib
LIBS = -lXm -lXt -lX11 -lm

TARGET = pizzafool

all: $(TARGET)

$(TARGET): pizzafool.c
	$(CC) $(CFLAGS) -o $(TARGET) pizzafool.c $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Installing PizzaFool..."
	cp $(TARGET) /opt/pizzafool/pizzafool 2>/dev/null || cp $(TARGET) $$HOME/pizzafool
	mkdir -p /opt/pizzafool/images 2>/dev/null || mkdir -p $$HOME/pizzafool-images
	cp images/*.xpm /opt/pizzafool/images/ 2>/dev/null || cp images/*.xpm $$HOME/pizzafool-images/
	@echo "Done! Run with: PIZZAFOOL_IMAGES=./images ./pizzafool"

run: $(TARGET)
	DISPLAY=:0 PIZZAFOOL_IMAGES=./images ./$(TARGET)

.PHONY: all clean install run
