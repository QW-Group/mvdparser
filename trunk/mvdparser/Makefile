
ifeq "$(shell uname | cut -d _ -f1)" "CYGWIN"
	OS=cygwin
else
	OS=linux
endif


CC_LIN = gcc
CC_WIN = gcc
CCFLAGS = -lm 
INCLUDEDIRS=

ifeq "$(OS)" "linux"
	SOURCES = frag_parser.c logger.c main.c mvd_parser.c net_msg.c netmsg_parser.c qw_protocol.c shared.c sys_linux.c 
	CC=$(CC_LIN)
else	## windows
	SOURCES = frag_parser.c logger.c main.c mvd_parser.c net_msg.c netmsg_parser.c qw_protocol.c shared.c strptime.c sys_win.c  
	CC=$(CC_WIN) -mno-cygwin -L/lib/w32api
	CCFLAGS+=-Wl,--allow-multiple-definition -Wl,--enable-auto-import
	EXTRA_LIBS=-lwinmm

endif


OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = mvdparser

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(CCFLAGS) $(INCLUDEDIRS) $(SOURCES) -o $(EXECUTABLE) $(EXTRA_LIBS)

clean: 
	rm -f *.o
	rm -f $(EXECUTABLE)


