CC      = gcc
CFLAGS  = -O2 -DLSB_FIRST -DDEBUG

dasm:	dasm.c
	${CC} -o dasm dasm.c

M6502.o:	M6502.c M6502.h Tables.h Codes.h
Debug.o:	Debug.c M6502.h
