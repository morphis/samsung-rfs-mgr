sbin_PROGRAMS = src/samsung-rfs-mgr

DEBUG_CFLAGS =
if WANT_DEBUG
DEBUG_CFLAGS += -g -O0
endif

src_samsung_rfs_mgr_SOURCES = \
	src/main.c \
	src/rfs.c
src_samsung_rfs_mgr_CFLAGS = \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(SAMSUNG_IPC_CFLAGS)
src_samsung_rfs_mgr_LDFLAGS = \
	$(GLIB_LIBS) \
	$(SAMSUNG_IPC_LIBS)

if DATAFILES
if SYSTEMD
systemdunitdir = @SYSTEMD_UNITDIR@
systemdunit_DATA = src/samsung-rfs-mgr.service
endif
endif

EXTRA_DIST = \
	MAINTAINERS

MAINTAINERCLEANFILES = \
	aclocal.m4 compile config.guess config.sub \
	configure depcomp install-sh ltmain.sh     \
	Makefile.in missing config.h.in            \
	mkinstalldirs *~ \
	$(NULL)
