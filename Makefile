# Makefile

UNAME=$(shell uname)
PREFIX=/usr/local/

# Optional features are enabled here:

# Define USE_FUSE:=1 to build with support for FUSE virtual
# filesystem. This provides easy access to the state of the NES while
# the emulator is running.

USE_FUSE?=0

# Configure variables according to optional features:

ifeq (1,$(USE_FUSE))
$(info FUSE filesystem enabled.)
FUSE_FLAGS=-DUSE_FUSE
FUSE_LIBS=-lfuse -lpthread
else
$(info FUSE filesystem disabled.)
FUSE_FLAGS=
FUSE_LIBS=
endif

# Build:

CC=gcc
CFLAGS= -Wall -O3 -g `sdl-config --cflags` `freetype-config --cflags` -msse2 -flax-vector-conversions -DPREFIX=\"$(PREFIX)\"
OBJECTS=nespal.o mapper_info.o rom.o sound.o sys.o nes.o vid.o config.o M6502.o global.o filters.o utility.o font.o filesystem.o ui.o main.o
INCLUDEDIRS= -IM6502
DEFINES=$(FUSE_FLAGS)

SYS_LIBS=-lm -ldl

ifeq ($(UNAME),Darwin)
SYS_LIBS=
endif

ifeq ($(UNAME),MINGW32)
SYS_LIBS=-lm
endif

LIBS= $(SYS_LIBS) -lSDL `sdl-config --libs` `freetype-config --libs` -lSDL_image $(FUSE_LIBS) 

MAPPERFILES=mappers/base.c mappers/mmc1.c mappers/konami2.c mappers/vromswitch.c mappers/mmc3.c mappers/axrom.c mappers/camerica.c mappers/vrc6.c mappers/gxrom.c

COBJ=$(CC) -std=c99 $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) -c
C90OBJ=$(CC) -std=c99 $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) -c
CAPP=$(CC) -std=c99 $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) $(OBJECTS) $(LIBS)

all: tenes
clean:
	rm -f *.o
	rm -f *~ M6502/*~ util/*~ util/a.out mappers/*~ \#*\# mappers/\#*\#
	rm -f tenes

install:
	install -d $(PREFIX)/share/tenes
	install media/*.png media/*.ttf $(PREFIX)/share/tenes
	install tenes $(PREFIX)/bin

romloadtest: rom.o romloadtest.c
	$(CAPP) romloadtest.c -o romloadtest

listmappers:
	$(CAPP) listmappers.c -o listmappers

tenes: Makefile $(OBJECTS)
	$(CAPP) -o tenes

main.o: Makefile main.c global.h ui.h nes.h rom.h sys.h
	$(COBJ) main.c

ui.o: Makefile ui.c global.h
	$(COBJ) ui.c

dasm.o: Makefile dasm.c
	$(COBJ) dasm.c

disasm.o: Makefile disasm.c
	$(COBJ) disasm.c

dis6502: Makefile disasm.o dasm.o
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) disasm.o dasm.o -o dis6502

nespal.o: Makefile nespal.c nespal.h
	$(COBJ) nespal.c

mapper_info.o: Makefile mapper_info.c mapper_info.h $(MAPPERFILES)
	$(COBJ) mapper_info.c

rom.o: Makefile rom.c rom.h
	$(COBJ) rom.c

sound.o: Makefile sound.c sound.h
	$(COBJ) sound.c

sys.o: Makefile sys.c sys.h
	$(COBJ) sys.c

nes.o: Makefile nes.c nes.h
	$(COBJ) nes.c

vid.o: Makefile vid.c vid.h nes.h
	$(COBJ) vid.c

filters.o: Makefile filters.c filters.h
	$(COBJ)	filters.c

M6502.o: Makefile M6502/M6502.c M6502/M6502.h M6502/Codes.h M6502/Tables.h
	$(COBJ) M6502/M6502.c

global.o: Makefile global.c global.h rom.h nes.h M6502/M6502.h
	$(COBJ) global.c

config.o: Makefile config.c config.h
	$(COBJ) config.c

filesystem.o: Makefile filesystem.c filesystem.h
	$(COBJ) -D_FILE_OFFSET_BITS=64 filesystem.c

font.o: Makefile font.c global.h font.h
	$(COBJ) font.c

