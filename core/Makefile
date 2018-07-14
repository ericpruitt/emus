#!/usr/bin/make -f
.POSIX:
.SILENT:

all:
	echo "Until \"./configure\" is run, the only valid target is" \
	     "\"deps\"." >&2
	exit 1

deps: makefile.in
	$(MAKE) -f $? MAKE="$(MAKE) -f $?" $@

.DEFAULT:
	$(MAKE) -s all
