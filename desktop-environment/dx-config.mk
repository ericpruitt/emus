# Build options for dmenu and dwm
VERSION = git-$$(git log -1 --format=%h)
DEPENDENCIES = fontconfig x11 xft

# Xinerama is only needed to support multiple monitors, so it's an optional
# dependency since I generally only use one monitor.
XINERAMALIBS = $$(pkg-config --silence-errors --libs xinerama)
XINERAMAFLAGS = $$(test -z "$(XINERAMALIBS)" || echo "-DXINERAMA")

# Includes and libraries
INCS = $$(pkg-config --cflags $(DEPENDENCIES)) -I..
LIBS = $$(pkg-config --libs $(DEPENDENCIES)) $(XINERAMALIBS) -lrt

# General compiler and linker flags
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\" $(XINERAMAFLAGS)
CFLAGS = -std=c99 -pedantic -Wall -Wno-deprecated-declarations -Os $(INCS) $(CPPFLAGS)
LDFLAGS = $(LIBS)

CC = cc
