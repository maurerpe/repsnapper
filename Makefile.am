AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

CLEANFILES = repsnapper.desktop

EXTRA_DIST = \
	README.asciidoc \
	repsnapper.desktop.in \
        README.win32 \
        licenses/BSL-1.0.txt \
        licenses/GPL-2.0.txt \
        licenses/LGPL-2.0.txt \
        licenses/MIT.txt \
        licenses/vmmlib-license.txt

Applicationsdir = $(datadir)/applications/
Applications_in_files = repsnapper.desktop.in
Applications_DATA = repsnapper.desktop

@INTLTOOL_DESKTOP_RULE@

# AppData support
@INTLTOOL_XML_RULE@
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = repsnapper.appdata.xml.in
EXTRA_DIST += $(appdata_in_files)
CLEANFILES += $(appdata_DATA)

noinst_LTLIBRARIES =

include libraries/Makefile.am
include src/Makefile.am
# po doesn't use automake, so it can't be included.
SUBDIRS = po
