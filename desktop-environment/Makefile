# Author: Eric Pruitt (https://www.codevat.com)
# License: BSD 2-Clause (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:

CC = cc
PREFIX = $(HOME)/.local
SUPERUSER_PREFIX = /usr/local
RUN_COMMAND_AS_ROOT = $$( \
	{ command -v sudo && echo "/bin/sh"; } || \
	{ command -v doas && echo "/bin/sh"; } || \
	echo "su root" \
) -c

DMENU_URL = http://git.suckless.org/dmenu
DMENU_COMMIT = 7be720cc88ed2294338f7182600df10f21c575ce

DWM_URL = http://git.suckless.org/dwm
DWM_COMMIT = e81f17d4c196aaed6893fd4beed49991caa3e2a4

ST_URL = https://git.suckless.org/st
ST_COMMIT = 211964d56ee00a7d46e251cbc150afb79138ae37

USER_LAUNCHER_ENTRIES = \
	st \

CONFIG_TARGETS = \
	$(HOME)/.del \

UTILITIES = \
	bin/blackwalls \
	bin/del \
	bin/fifo2rootname \
	bin/statusline \
	bin/xidletime \

QUIETMAKE = MAKEFLAGS= $(MAKE) -s
BASENAME = $(@F)

.SILENT: all deps clean cleaner config.mk install printvar

# The default target handles fetching, patching and reverting (cleaning)
# external repositories. Targets matching "patch-*" will result in "*-src"
# being patched if there are patches available for it. Targets matching
# "clean-*" will result in "*" being cleaned or, when that path does not exist
# "*-src" being cleaned. Targets ending in "*-src" will retch external
# dependencies.
.DEFAULT:
	@case "$${target:=$@}" in \
	  patch-*) \
		patch_prefix="$${target#patch-}"; \
		cd "$$patch_prefix"-src/; \
		if git diff --quiet \
		  $$(find . -name .git -prune -o -type f); then \
			for patch in ../patches/"$$patch_prefix"-*.diff; do \
				test -e "$$patch" || exit 0; \
				echo "- $${patch##*/}"; \
				patch < "$$patch"; \
				find -name "*.orig" -delete; \
			done; \
		fi; \
		cd "$$OLDPWD"; \
		$(MAKE) -s "$$patch_prefix-src/config.mk"; \
	  ;; \
	  clean-*) \
		target="$${target#clean-}"; \
		if ! cd "$$target" 2>/dev/null && \
		   ! cd "$$target"-*/ 2>/dev/null && \
		   ! -e "$$target"; then \
			echo "make: $@: path not found" >&2; \
			exit 1; \
		elif [ -n "$$OLDPWD" ] && [ -e .git ]; then \
			git stash save --quiet -u "make $@"; \
			rm -f config.mk; \
		else \
			echo "make: not sure how to clean '$$target'" >&2; \
			exit 1; \
		fi; \
	  ;; \
	  dmenu-src|dwm-src|st-src) \
		prefix="$$(echo $${target%-src} | tr 'a-z-' 'A-Z_')"; \
		url="$$($(QUIETMAKE) printvar VARIABLE=$${prefix}_URL)"; \
		commit="$$($(QUIETMAKE) printvar VARIABLE=$${prefix}_COMMIT)"; \
		git clone "$$url" "$(BASENAME).tmp"; \
		(cd "$(BASENAME).tmp" && git reset --hard $$commit); \
		rm -f "$(BASENAME).tmp/config.mk"; \
		mv "$(BASENAME).tmp" "$(BASENAME)"; \
	  ;; \
	  *) \
		echo "make: $@: target not recognized" >&2; \
		exit 1; \
	  ;; \
	esac; \

# Compile all known binaries.
all:
	touch .MARK
	$(MAKE) -s dmenu dwm screen-locker st $(UTILITIES)
	if [ "$(.TARGETS)" != "install" ] && \
	  [ "$(MAKECMDGOALS)" != "install" ] ; then \
		find . -name .git -prune -o -newer .MARK -type f -print \
		  | grep -q ^ || echo "make: all targets up to date"; \
	fi
	rm -f .MARK

# Install all compiled binaries.
install: all
	echo "PREFIX = $(PREFIX)"
	echo "SUPERUSER_PREFIX = $(SUPERUSER_PREFIX)"
	echo "- screen-locker"
	$(MAKE) -s $(SUPERUSER_PREFIX)/bin/screen-locker
	mkdir -p $(PREFIX)/bin
	for binary in "$$PWD/bin/"*; do \
		echo "- $${binary##*/}"; \
		test ! -h "$(PREFIX)/bin/$${binary##*/}" || continue; \
		ln -s "$$binary" $(PREFIX)/bin; \
	done
	$(MAKE) -s $(CONFIG_TARGETS)

# Install build dependencies.
deps:
	if [ -e /etc/debian_version ]; then \
		apt-get install -y \
			acpid \
			bc \
			compton \
			fonts-dejavu \
			fonts-symbola \
			fonts-vlgothic \
			fonts-wqy-zenhei \
			gsfonts \
			gtk2-engines \
			libpam-dev \
			libx11-dev \
			libxft-dev \
			libxinerama-dev \
			libxrandr-dev \
			libxss-dev \
			mesa-utils \
			moc \
			moc-ffmpeg-plugin \
			pkg-config \
			scrot \
			xdotool \
			yad \
			$$( \
				lspci | grep -i -q nvidia && echo nvidia-smi; \
			) \
		; \
	else \
		echo "Unsupported operating system." >&2; \
		exit 1; \
	fi; \

# Revert external repositories to a fresh state and delete compiled binaries.
clean:
	for folder in *-src; do \
		test -e "$$folder" || continue; \
		echo "- $$folder"; \
		$(MAKE) -s "clean-$$folder"; \
	done
	rm -f .MARK bin/*
	echo "Done cleaning."

# Delete all external source trees and compiled binaries.
cleaner:
	rm -rf .MARK *-src bin
	echo "All external source trees and compiled binaries deleted."

# Print the contents of a variable named $(VARIABLE).
printvar:
	echo '$($(VARIABLE))'

# Check each value in $(ARGS) to verify that the compilation toolchain supports
# it as a flag or include. If an argument starts with a "-", the argument is
# treated as a compiler flag. Otherwise, the argument is treated as a path to
# load using an "include" preprocessor directive. The compiler is only invoked
# once per argument, and if the inovcation succeeds, the argument is printed to
# standard output.
cc-test:
	trap 'rm -f -- a.out "$$src"' EXIT; \
	src=".temp.$$$$.c"; \
	for arg in $(ARGS); do \
		echo "int main(void) {}" > "$$src"; \
		test -z "$${arg%%-*}" || echo "#include <$$arg>" >> "$$src"; \
		$(CC) "$$src" $${arg##[!-]*} 2>/dev/null || continue; \
		printf "%s " "$$arg"; \
	done

# Print text that can be used to generate a config.mk file for suckless.org
# projects. This target assumes that the variables CFLAGS, LDFLAGS and CC flags
# are defined. A list of libraries supported by pkg-config can optionally be
# provided via LIBRARIES.
config.mk:
	echo CFLAGS = -std=c99 -pedantic -Wall -O3 -I.. \
		-D_DEFAULT_SOURCE '-DVERSION=\"edge\"' $(CFLAGS) \
		$$(test -z "$(LIBRARIES)" || pkg-config --cflags $(LIBRARIES))
	echo LDFLAGS = $(LDFLAGS) \
		$$(test -z "$(LIBRARIES)" || pkg-config --libs $(LIBRARIES))
	echo CC = $(CC)
	echo STCFLAGS = '$$(CFLAGS)'
	echo STLDFLAGS = '$$(LDFLAGS)'

# Dummy target used to ensure a recipe is always executed even if it is
# otherwise up to date.
ALWAYS_RUN:

$(HOME)/.del: $(PREFIX)/bin/del
	echo "- launcher list"
	printf "%s\n" $(USER_LAUNCHER_ENTRIES) | $? -r > /dev/null

# The combination of the "+" command prefix and "-n" is used to force make to
# display the command being executed even if "-s" is inherited from the
# invoking make.
$(UTILITIES): ALWAYS_RUN
	mkdir -p bin
	source="utilities/$(BASENAME).c"; \
	make=$$(sed -nf extract-recipe.sed "$$source"); \
	echo "$@: $$source; +$${make:?$@: recipe not found}" | $(MAKE) -n -f -

dmenu-src/config.mk dwm-src/config.mk:
	lxinerama="$$(pkg-config --silence-errors --libs xinerama)"; \
	$(QUIETMAKE) -s config.mk \
		CFLAGS="$${lxinerama:+-DXINERAMA}" \
		LDFLAGS="$$lxinerama -lm $$($(QUIETMAKE) cc-test ARGS=-lrt)" \
		LIBRARIES="fontconfig x11 xft" \
	  > $@.tmp
	mv $@.tmp $@

dmenu-src/dmenu: dmenu-src ALWAYS_RUN
	ln -s -f ../dmenu-config.h dmenu-src/config.h
	$(MAKE) -s patch-dmenu
	(cd dmenu-src && $(MAKE) -s dmenu)

bin/dmenu: dmenu-src/dmenu
	mkdir -p bin
	cp -f $? $@

dmenu: bin/dmenu

dwm-src/dwm: dwm-src ALWAYS_RUN
	ln -s -f ../dwm-config.h dwm-src/config.h
	$(MAKE) -s patch-dwm
	(cd dwm-src && $(MAKE) -s dwm)

bin/dwm: dwm-src/dwm
	mkdir -p bin
	cp -f $? $@

dwm: bin/dwm

screen-locker/screen-locker: screen-locker/screen-locker.c
	cc_shadow_h="$$($(QUIETMAKE) cc-test ARGS=shadow.h)"; \
	LDFLAGS="$$($(QUIETMAKE) cc-test ARGS='-lcrypt -lpam')"; \
	echo "$$LDFLAGS" | grep -q -e -lpam && pam_cflags="-DHAVE_PAM_AUTH"; \
	(cd $(@D); \
	$(MAKE) $(@F) \
		CC="$(CC)" \
		LDFLAGS="$$LDFLAGS" \
		CFLAGS="$${pam_cflags:-} $${cc_shadow_h:+-DHAVE_SHADOW_H}" \
	)

screen-locker: screen-locker/screen-locker

$(SUPERUSER_PREFIX)/bin/screen-locker: screen-locker/screen-locker
	$(RUN_COMMAND_AS_ROOT) 'install -m 4755 -g 0 -o 0 $? $@'

st-src/config.mk:
	$(MAKE) -s config.mk \
		CFLAGS="-D_XOPEN_SOURCE" \
		LDFLAGS="-lm -lutil $$($(QUIETMAKE) cc-test ARGS=-lrt)" \
		LIBRARIES="fontconfig x11 xft" \
	  > $@.tmp
	mv $@.tmp $@

st-src/st: st-src ALWAYS_RUN
	ln -s -f ../st-config.h st-src/config.h
	$(MAKE) -s patch-st
	(cd st-src && $(MAKE) -s st)

bin/st: st-src/st
	mkdir -p bin
	cp -f $? $@

st: bin/st
