#
# Combined Makefile for Linux and OS/X
#
#
# Copyright 2009+ Joachim Glauche
#
# This file is part of RepSnapper and is made available under
# the terms of the GNU General Public License, version 2, or at your
# option, any later version, incorporated herein by reference.

bin_PROGRAMS = repsnapper

LIB_DIR=$(top_srcdir)/libraries
WARNING_FLAGS = -Wall -Wsign-compare -Wno-invalid-offsetof

repsnapper_CPPFLAGS = \
	-I$(LIB_DIR)/vmmlib/include \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-I$(LIB_DIR) \
	-DHAVE_GTK $(CFLAGS) $(EXTRA_CFLAGS) -I$(LIB_DIR) $(GTKMM_CFLAGS) \
        -DRSDATADIR='$(repsnapperdatadir)' \
        -DSYSCONFDIR='$(repsnapperconfdir)' \
	-DLOCALEDIR='"$(localedir)"' \
	$(OPENMP_CFLAGS) \
	$(LIBZIP_CFLAGS) \
	$(WARNING_FLAGS)

SHARED_SRC= \
	src/geometry.cpp \
	src/transform3d.cpp \
	src/platform.cpp \
	src/objtree.cpp \
	src/model.cpp \
	src/model_slice.cpp \
	src/shape.cpp \
	src/gcode.cpp \
	src/triangle.cpp \
	src/render.cpp \
	src/files.cpp \
	src/settings.cpp

SHARED_INC= \
	src/geometry.h \
	src/transform3d.h \
	src/miniball.h \
	src/model.h \
	src/objtree.h \
	src/shape.h \
	src/gcode.h \
	src/triangle.h \
	src/files.h \
	src/stdafx.h \
	src/platform.h \
	src/render.h \
	src/settings.h \
	src/types.h

include src/ui/Makefile.am
include src/printer/Makefile.am

repsnapper_SOURCES = $(SHARED_SRC) $(SHARED_INC) src/repsnapper.cpp

repsnapper_LDFLAGS = $(EXTRA_LDFLAGS)

repsnapper_LDADD = $(CLIPPER_LIBS) $(OPENMP_CFLAGS) $(GTKMM_LIBS) $(LIBZIP_LIBS)

repsnapperdatadir = $(datadir)/@PACKAGE@
dist_repsnapperdata_DATA = src/repsnapper.ui src/repsnapper.svg

repsnapperconfdir = $(sysconfdir)/xdg/@PACKAGE@
dist_repsnapperconf_DATA = src/repsnapper.conf

repsnappericondir = $(datadir)/icons/hicolor/scalable/apps/
dist_repsnappericon_DATA = src/repsnapper.svg
