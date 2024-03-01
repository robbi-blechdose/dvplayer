CC		= gcc

CFLAGS	= -Wall -g
LDFLAGS	= -liec61883 -lraw1394 -lncurses

PNAME	= dvplayer

# Files to be compiled
SRCDIR	=  ./src
VPATH	= $(SRCDIR)
SRC_C	= $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.c))
OBJS	= $(notdir $(patsubst %.c, %.o, $(SRC_C)))

# Rules to make executable
$(PNAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(PNAME) $^ $(LDFLAGS)

$(OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm *.o
	rm $(PNAME)