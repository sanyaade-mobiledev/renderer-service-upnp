INCLUDES = -DG_LOG_DOMAIN=\"RSU\"

AM_CFLAGS =	$(GLIB_CFLAGS)				\
		$(GIO_CFLAGS)				\
		$(GSSDP_CFLAGS)				\
		$(GUPNP_CFLAGS)				\
		$(GUPNPAV_CFLAGS)			\
		$(GUPNPDLNA_CFLAGS)			\
		$(SOUP_CFLAGS)				\
		-DSYS_CONFIG_DIR="\"$(sysconfdir)\"" 	\
		-include config.h


ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

sysconf_DATA = renderer-service-upnp.conf

renderer_service_upnp_sources =	src/async.c			\
				src/device.c			\
				src/error.c			\
				src/host-service.c		\
				src/log.c			\
				src/renderer-service-upnp.c	\
				src/service-task.c		\
				src/settings.c			\
				src/task.c			\
				src/task-processor.c		\
				src/upnp.c

renderer_service_upnp_headers =	src/async.h			\
				src/device.h			\
				src/error.h			\
				src/host-service.h		\
				src/log.h			\
				src/prop-defs.h			\
				src/renderer-service-upnp.h	\
				src/service-task.h		\
				src/settings.h			\
				src/task.h			\
				src/task-atom.h			\
				src/task-processor.h		\
				src/upnp.h

libexec_PROGRAMS = renderer-service-upnp

renderer_service_upnp_SOURCES = $(renderer_service_upnp_sources) \
				$(renderer_service_upnp_headers)

renderer_service_upnp_LDADD = $(GLIB_LIBS)	\
			      $(GIO_LIBS)	\
			      $(GSSDP_LIBS)	\
			      $(GUPNP_LIBS)	\
			      $(GUPNPAV_LIBS)	\
			      $(GUPNPDLNA_LIBS)	\
			      $(SOUP_LIBS)	\
			      -lm

dbussessiondir = @DBUS_SESSION_DIR@
dbussession_DATA = src/com.intel.renderer-service-upnp.service

EXTRA_DIST = test/cap.py \
	     $(sysconf_DATA)

MAINTAINERCLEANFILES =	Makefile.in		\
			aclocal.m4		\
			configure		\
			config.h.in		\
			config.h.in~		\
			build-aux/depcomp	\
			build-aux/compile	\
			build-aux/missing	\
			build-aux/install-sh

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = renderer-service-upnp.pc

CLEANFILES = $(pkgconfig_DATA) $(dbussession_DATA) renderer-service-upnp.conf
DISTCLEANFILES = $(pkgconfig_DATA) $(dbussession_DATA) renderer-service-upnp.conf

maintainer-clean-local:
	rm -rf build-aux
