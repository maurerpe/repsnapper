AC_PREREQ(2.63)
AC_INIT([repsnapper],[3.0_alpha1],[https://github.com/maurerpe/repsnapper], [repsnapper], [https://github.com/maurerpe/repsnapper])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/repsnapper.cpp])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS

AC_CHECK_HEADERS([fcntl.h float.h limits.h memory.h stdlib.h string.h sys/ioctl.h sys/time.h termios.h unistd.h asm/termbits.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

AM_GNU_GETTEXT_VERSION([0.19.7])
GETTEXT_PACKAGE=repsnapper
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",[The prefix for our gettext translation domains.])
AM_GNU_GETTEXT

PKG_PROG_PKG_CONFIG([0.22])

AC_LANG_CPLUSPLUS
AC_TYPE_SSIZE_T

PKG_CHECK_MODULES(GTKMM, [gtkmm-3.0 gtk+-3.0 epoxy])

AC_OPENMP()

PKG_CHECK_MODULES(LIBZIP, libzip >= 0.10)

AC_SEARCH_LIBS([tan], [m], [], [
  AC_MSG_ERROR([unable to find library containing tan])
])

AC_SEARCH_LIBS([PS_ParseJsonFile], [printer_settings], [], [
  AC_MSG_ERROR([unable to find the printer_settings library])
])

#mingw
#  GL_LIBS="-lopengl32" # mingw
# -lglu32 -lglut32"
#  EXTRA_LDFLAGS="-Wl,-subsystem,windows -shared-libgcc"
#  EXTRA_CFLAGS="-fexceptions -mthreads"
# AC_CHECK_HEADER(GL/glut.h, , AC_MSG_ERROR([cannot find headers for freeglut]))
#  WIN32_BUILD=yes
 
EXTRA_CFLAGS=--std=c++11

AM_CONDITIONAL(WIN32_BUILD, test "x$WIN32_BUILD" = "xyes")

AC_SUBST(GL_LIBS)
AC_SUBST(EXTRA_LDFLAGS)
AC_SUBST(EXTRA_CFLAGS)

# Convert install path to use \\ for substitution in the win32 nsi script
prefix_win=`echo $prefix | sed -e 's|/|\\\\|g'`
AC_SUBST(prefix_win)

AC_CONFIG_FILES([
  Makefile
  libraries/Makefile
  po/Makefile.in
  intl/Makefile
  repsnapper.desktop
  repsnapper.appdata.xml
  win32/repsnapper.nsi])
AC_OUTPUT
