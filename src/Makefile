# program
SHARED = lolfactory.c loltask.c network.c lolhelper.c
SRC_LOLD = ${SHARED} lold.c arduino-serial-lib.c
SRC_LOLPLAY = ${SHARED} lolplay.c
OBJ_LOLD = ${SRC_LOLD:.c=.o}
OBJ_LOLPLAY = ${SRC_LOLPLAY:.c=.o}

# compile env
CC = gcc
LD = ${CC}
DEBUG = -ggdb3
CFLAGS = -std=c99 -pedantic -Werror -Wno-error=unused-variable -Wall -Wextra -Wshadow -Wcast-qual -Wcast-align -Wfloat-equal -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations -Wmissing-include-dirs -Wswitch-default -Wswitch-enum -Wbad-function-cast -Wpointer-arith -Wstrict-overflow=5 -Winline -Wundef -Wnested-externs -Wunreachable-code -Wlogical-op -Wredundant-decls -Wold-style-definition -Wno-write-strings -Wno-unused-parameter -Wno-parentheses -O0 -fmessage-length=0 -fno-omit-frame-pointer -ffloat-store -fno-common -c ${DEBUG}

#CFLAGS = -std=c99 -pedantic -O3 -DNDEBUG -flto
LFLAGS = -Wall -lm -lpthread ${DEBUG}

####

all: options lold lolplay 

options:
	@echo build options:
	@echo "CC     = ${CC}"
	@echo "LD     = ${LD}"
	@echo "CFLAGS = ${CFLAGS}"
	@echo "LFLAGS = ${LFLAGS}"
	@echo

.c.o:
	$(CC) $(CFLAGS) $<

lold: ${OBJ_LOLD}
	$(LD) $(LFLAGS) ${OBJ_LOLD} -o $@
	mv $@ ..

lolplay: ${OBJ_LOLPLAY}
	$(LD) $(LFLAGS) ${OBJ_LOLPLAY} -o $@
	mv $@ ..

tar: clean
	@echo creating archive
	@tar -czvf lold.tar.gz * ../LICENSE ../README ../lolmpd.sh

clean:
	@echo cleaning
	@rm -f ../lold ../lolplay ${OBJ_LOLD} ${OBJ_LOLPLAY}

.PHONY: all options debug tar clean

