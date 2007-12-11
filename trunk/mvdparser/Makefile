
CC = gcc
CCFLAGS = -lm

SOURCES = frag_parser.c logger.c main.c mvd_parser.c net_msg.c netmsg_parser.c qw_protocol.c shared.c sys_linux.c 
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = mvdparser

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(CCFLAGS) $(SOURCES) -o $(EXECUTABLE)

clean: 
	rm -f *.o
	rm -f $(EXECUTABLE)

