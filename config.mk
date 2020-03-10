NAME = dwmstatus
VERSION = 1.2

deps = glib-2.0 gio-2.0 x11 libcurl

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -std=c99 -pedantic -Wall -O0 -g $(shell pkg-config --cflags $(deps))
LDFLAGS = $(shell pkg-config --libs $(deps))

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
