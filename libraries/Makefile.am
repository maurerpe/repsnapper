AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = poly2tri/LICENSE
noinst_LTLIBRARIES = libpoly2tri.la

libpoly2tri_la_SOURCES = \
	poly2tri/poly2tri/sweep/sweep.cc \
	poly2tri/poly2tri/sweep/sweep_context.cc \
	poly2tri/poly2tri/sweep/cdt.cc \
	poly2tri/poly2tri/sweep/advancing_front.cc \
	poly2tri/poly2tri/common/shapes.cc

include vmmlib/Makefile.am
