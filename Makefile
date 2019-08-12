
ifndef SYSTEMD
        ifeq ($(shell systemctl --version > /dev/null 2>&1 && echo 1), 1)
                SYSTEMD = $(shell systemctl --version 2> /dev/null |  sed -n 's/systemd \([0-9]*\)/\1/p')
        endif
endif

ifndef SYSTEMDPATH
        SYSTEMDPATH=usr/lib
endif

prefix          =
bindir          = $(exec_prefix)/usr/sbin
unitdir         = $(prefix)/$(SYSTEMDPATH)/systemd/system

RM              = rm -f
INSTALL_PROGRAM = install


SRCS	= fpin_main.c fpin_els.c fpin_dm.c

OBJS	= $(SRCS:.c=.o)

LIB	= -lpthread -ludev -ldevmapper -lmpathcmd


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
