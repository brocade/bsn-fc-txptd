SRCS	= fpin_main.c fpin_els.c fpin_dm.c

OBJS	= $(SRCS:.c=.o)

LIB	= -lpthread -ludev -ldevmapper -lmpathcmd

#CFLAGS	= -DFPIN_DEBUG -g

TARGET	= fctxpd

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIB)

all::	$(TARGET)

DESTDIR=/sbin
.PHONY: install
	cp $(TARGET) $(DESTDIR)
clean::
	$(RM) $(TARGET) $(OBJS)
