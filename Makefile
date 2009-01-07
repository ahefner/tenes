# Makefile

CC=gcc
CFLAGS= -Wall -O3 -g -pg `sdl-config --cflags` -std=c99

OBJECTS=nespal.o mapper_info.o rom.o sound.o sys.o nes.o vid.o config.o M6502.o global.o filters.o
INCLUDEDIRS= -IM6502
DEFINES=
LIBS= -lSDL -lm -lpthread -ldl `sdl-config --libs`
MAPPERFILES=mappers/base.c mappers/mmc1.c mappers/konami2.c mappers/vromswitch.c mappers/mmc3.c mappers/axrom.c mappers/camerica.c

COBJ=$(CC) $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) -c
CAPP=$(CC) $(CFLAGS) $(DEFINES) $(INCLUDEDIRS) $(OBJECTS) $(LIBS)

all: nesemu
clean:
	rm -f *.o
	rm -f nesemu

romloadtest: rom.o romloadtest.c
	$(CAPP) romloadtest.c -o romloadtest

listmappers:
	$(CAPP) listmappers.c -o listmappers

nesemu: Makefile $(OBJECTS) main.c
	$(CAPP) main.c -o nesemu

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

# Bye.
