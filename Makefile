
ifeq "$(shell uname | cut -d _ -f1)" "CYGWIN"
	OS=cygwin
else
	OS=linux
endif


CC_LIN = gcc
CC_WIN = gcc
CCFLAGS = -lm
INCLUDEDIRS =

ifeq "$(OS)" "linux"
	SOURCES = src/frag_parser.c src/logger.c src/main.c src/mvd_parser.c src/net_msg.c src/netmsg_parser.c src/qw_protocol.c src/shared.c src/sys_linux.c
	CC=$(CC_LIN)
else	## windows
	SOURCES = src/frag_parser.c src/logger.c src/main.c src/mvd_parser.c src/net_msg.c src/netmsg_parser.c src/qw_protocol.c src/shared.c src/strptime.c src/sys_win.c
	CC=$(CC_WIN) -mno-cygwin -L/lib/w32api
	CCFLAGS+=-Wl,--allow-multiple-definition -Wl,--enable-auto-import
	EXTRA_LIBS=-lwinmm

endif


OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = mvdparser

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(INCLUDEDIRS) $(SOURCES) -o $(EXECUTABLE) $(CCFLAGS) $(EXTRA_LIBS)

clean: 
	rm -f src/*.o
	rm -f $(EXECUTABLE)
