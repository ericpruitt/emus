#!/usr/bin/make -f
.POSIX:
.SILENT: test sanity

CFLAGS = -Werror -Wall -fPIC -O3 -isystem $(UTF8PROC)
CPPFLAGS = -D _GNU_SOURCE -D _XOPEN_SOURCE -D LD_PRELOAD_SO -D _DEFAULT_SOURCE
LDFLAGS = -ldl

LIBRARY_SO = utf8proc-wcwidth.so
LIBRARY_SRC = $(LIBRARY_SO:.so=.c)

UTF8PROC = ../utf8proc-src

# These variables are used for the sanity test and may need to be adjusted if
# the current host's environment differs from the developer's.
GNU_LS = /bin/ls
LD_DEBUG_OUTPUT = ld-debug-make.$@
LIBASAN_SO = /usr/lib/gcc/x86_64-linux-gnu/6/libasan.so
SANITY_CC = clang
SANITY_CFLAGS = $(CFLAGS) -g -Weverything -flto -fsanitize=address,undefined
SANITY_LDFLAGS = $(LDFLAGS) -lasan -lubsan

QUIESCE_GMAKE = $(MAKEFILE_LIST:GNUmakefile=--no-print-directory)

all: $(LIBRARY_SO)

$(UTF8PROC)/utf8proc.c $(UTF8PROC)/utf8proc_data.c: $(UTF8PROC)/.git

$(LIBRARY_SO): $(LIBRARY_SRC) $(UTF8PROC)/utf8proc.c $(UTF8PROC)/utf8proc_data.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIBRARY_SRC) -o $(LIBRARY_SO) $(LDFLAGS)

# GNU ls(1) version 8.27 (current at the time of this writing) will call
# wcwidth(3) when "-C" is specified and certain characters appear in filenames.
test: $(LIBRARY_SO)
	trap 'rm -f "🦀"' EXIT && \
	touch "🦀" && \
	LC_ALL="C.UTF-8" \
	LD_DEBUG="symbols" \
	LD_DEBUG_OUTPUT="$(LD_DEBUG_OUTPUT)" \
	LD_PRELOAD="$(LIBASAN_SO):$(PWD)/$(LIBRARY_SO)" $(GNU_LS) -C >/dev/null
	for path in $(LD_DEBUG_OUTPUT).*; do \
		test -e "$$path" && break; \
		echo "$@: no files written to $(LD_DEBUG_OUTPUT).*; is" \
		     "$(GNU_LS) dynamically linked against glibc?" >&2; \
		exit 1; \
	done
	awk >&2 ' \
		$$2 == "symbol=wcwidth;" { \
			count++; \
			if ($$(NF - 1) == "file=$(PWD)/$(LIBRARY_SO)") { \
				found = 1; \
				exit; \
			} \
			calls[count] = $$0; \
		} \
		END { \
			if (found) { \
				exit; \
			} \
			printf "%s: ", "$@"; \
			if (count) { \
				print "$(LIBRARY_SO) not used for lookup:"; \
			} else { \
				print "$(GNU_LS) did not use wcwidth"; \
			} \
			for (i = 1; i <= count; i++) { \
				print calls[i]; \
			}; \
			exit 1; \
		} \
	' $(LD_DEBUG_OUTPUT).*
	echo "$?: library is functional"
	rm -f ld-debug-make.*

sanity:
	$(MAKE) $(QUIESCE_GMAKE) clean
	$(MAKE) $(QUIESCE_GMAKE) \
		CC="$(SANITY_CC)" \
		CFLAGS="$(SANITY_CFLAGS)" \
		LDFLAGS="$(SANITY_LDFLAGS)" \
	;
	echo "$@: no problems detected"
	$(MAKE) $(QUIESCE_GMAKE) clean

clean:
	rm -f $(LIBRARY_SO) ld-debug-make.*
