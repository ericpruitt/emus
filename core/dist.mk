#!/usr/bin/make -f
# https://github.com/ericpruitt/static-unix-userland
# https://www.codevat.com/
.POSIX:
.SILENT:

BIN = <BIN>
MAN = <MAN>
PLATFORM = <PLATFORM>

install:
	echo 'Build platform: $(PLATFORM)'
	echo BIN = $(BIN)
	echo MAN = $(MAN)
	if ! [ -e bin ]; then \
		echo "no binaries found; nothing to install" >&2; \
		exit 1; \
	fi
	if [ -z "$(BIN)" ]; then \
		echo "BIN must be set to install" >&2; \
		exit 1; \
	fi
	mkdir -p $(MAN) $(BIN)
	if [ -n "$(MAN)" ]; then \
		for d in man/*/; do \
			rm -rf $(MAN)/"$${d#*/}"; \
		done; \
		mv -f man/* $(MAN); \
	fi
	for d in bin/*/; do \
		rm -rf $(BIN)/"$${d#*/}"; \
	done
	mv -f bin/* $(BIN)
	rmdir bin man
	rm Makefile
	echo "Installation complete."
