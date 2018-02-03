# 
# this is the makefile to the 8048 cross assembler 
# and the 8048 disassembler. 
# 
 
CC		= cc
XDIR	= /usr/local/bin
DEBUG	= #-g
OPT		= -O2
CFLAGS	= $(OPT) $(DEBUG)
LDFLAGS	= $(DEBUG)
 
dis12: dis12.o code.o loadfile.o

dis12.o: dis12.c dis12.h

code.o: code.c dis12.h dis12tbl.h

loadfile.o: loadfile.c

clean: 
	rm -f *.o core
 
pritine: 
	rm -f *.o core dis12
 
install: 
	cp dis12 $(XDIR)/dis12 

