
ifeq ($(TOPDIR),)
	TOPDIR	= ..
endif

ifndef LIB
	ifeq ($(shell test -d /lib64 && echo 1),1)
		LIB=lib64
	else
		LIB=lib
	endif
endif

ifndef RUN
	ifeq ($(shell test -L /var/run -o ! -d /var/run && echo 1),1)
		RUN=run
	else
		RUN=var/run
	endif
endif

ifndef SYSTEMD
	ifeq ($(shell systemctl --version > /dev/null 2>&1 && echo 1), 1)
		SYSTEMD = $(shell systemctl --version 2> /dev/null |  sed -n 's/systemd \([0-9]*\)/\1/p')
	endif
endif

ifndef SYSTEMDPATH
	SYSTEMDPATH=usr/lib
endif

prefix		=
exec_prefix	= $(prefix)
usr_prefix	= $(prefix)
bindir		= $(exec_prefix)/sbin
includedir	= $(prefix)/usr/include
pkgconfdir	= $(usrlibdir)/pkgconfig
unitdir         = $(prefix)/$(SYSTEMDPATH)/systemd/system

RM		= rm -f
INSTALL_PROGRAM	= install
