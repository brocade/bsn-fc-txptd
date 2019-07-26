include Makefile.inc

SRCS	= fpin_main.c fpin_els.c fpin_dm.c

OBJS	= $(SRCS:.c=.o)

LIB	= -lpthread -ludev -ldevmapper -lmpathcmd -ldl


#CFLAGS	= -DFPIN_DEBUG -g
CFLAGS += -g
TARGET	= fctxpd

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIB)

all::	$(TARGET)

.PHONY: install
install:
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) -m 755 $(TARGET) $(DESTDIR)$(bindir)/
ifdef SYSTEMD
	$(INSTALL_PROGRAM) -d $(DESTDIR)$(unitdir)
	$(INSTALL_PROGRAM) -m 644 $(TARGET).service $(DESTDIR)$(unitdir)
endif

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(bindir)/$(TARGET)
	$(RM) $(DESTDIR)$(unitdir)/$(TARGET).service
clean::
	$(RM) $(TARGET) $(OBJS)

include $(wildcard $(OBJS:.o=.d))

dep_clean:
	$(RM) $(OBJS:.o=.d)
