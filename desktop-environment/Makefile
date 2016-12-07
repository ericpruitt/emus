.POSIX:

PREFIX = $(HOME)
SUPERUSER_PREFIX = /usr/local

DMENU_URL = http://git.suckless.org/dmenu
DMENU_COMMIT = e90b88e12a88d6214c00d5ee58ceb69446aa5ac4

DWM_URL = http://git.suckless.org/dwm
DWM_COMMIT = 839c7f6939368fe5784058975ee95062cc88d4c3

SLOCK_URL = http://git.suckless.org/slock
SLOCK_COMMIT = 2d2a21a90ad1b53594b1b90b97486189ec54afce
SLOCK_FORCE_PAM = false
SLOCK_VARS = $$($(SLOCK_FORCE_PAM) && echo "IN_PASSWD=false")

ST_URL = http://git.suckless.org/st
ST_COMMIT = e44832408bb3147826c346872b49de105a4d0e0b

CONFIG_TARGETS = \
	$(HOME)/.config/Trolltech.conf \
	$(HOME)/.del \
	$(HOME)/.fonts.conf \
	$(HOME)/.gtkrc-2.0 \
	$(HOME)/.xsession \
	$(TERMINFO)/s/st \

UTILITIES = \
	bin/blackwalls \
	bin/del \
	bin/statusline \

BASENAME = $(@F)

.SILENT: all deps clean cleaner install printvar

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
			done; \
		fi; \
	  ;; \
	  clean-*) \
		target="$${target#clean-}"; \
		if ! cd "$$target" 2>/dev/null && \
		   ! cd "$$target"-*/ 2>/dev/null && \
		   ! -e "$$target"; then \
			echo "make: $@: path not found" >&2; \
			exit 1; \
		elif [ -n "$$OLDPWD" ] && [ -e .git ]; then \
			git stash save --quiet "make $@"; \
			git clean -d -f -q -x; \
		else \
			echo "make: not sure how to clean '$$target'" >&2; \
			exit 1; \
		fi; \
	  ;; \
	  dmenu-src|dwm-src|slock-src|st-src) \
		prefix="$$(echo $${target%-src} | tr 'a-z-' 'A-Z_')"; \
		url="$$($(MAKE) -s printvar VARIABLE=$${prefix}_URL)"; \
		commit="$$($(MAKE) -s printvar VARIABLE=$${prefix}_COMMIT)"; \
		git clone "$$url" "$(BASENAME).tmp"; \
		(cd "$(BASENAME).tmp" && git reset --hard $$commit); \
		mv "$(BASENAME).tmp" "$(BASENAME)"; \
	  ;; \
	  *) \
		echo "make: $@: target not recognized" >&2; \
		exit 1; \
	  ;; \
	esac; \

all:
	touch .MARK
	$(MAKE) -s dmenu dwm slock st $(UTILITIES)
	if [ "$(.TARGETS)" != "install" ] && \
	  [ "$(MAKECMDGOALS)" != "install" ] ; then \
		find . -name .git -prune -o -newer .MARK -type f -print \
		  | grep -q ^ || echo "make: all targets up to date"; \
	fi
	rm -f .MARK

install: all
	echo "PREFIX = $(PREFIX)"
	echo "SUPERUSER_PREFIX = $(SUPERUSER_PREFIX)"
	echo "- slock" && $(MAKE) -s $(SUPERUSER_PREFIX)/bin/slock
	test -e $(PREFIX)/bin || mkdir $(PREFIX)/bin
	for binary in "$$PWD/bin/"*; do \
		echo "- $${binary##*/}"; \
		test ! -h "$(PREFIX)/bin/$${binary##*/}" || continue; \
		ln -s "$$binary" $(PREFIX)/bin; \
	done
	$(MAKE) -s $(CONFIG_TARGETS)

deps:
	if [ -e /etc/debian_version ]; then \
		apt-get install \
			compton \
			fonts-dejavu \
			fonts-vlgothic \
			fonts-wqy-zenhei \
			gsfonts \
			gtk2-engines \
			libpam-dev \
			libx11-dev \
			libxft-dev \
			libxinerama-dev \
			libxrandr-dev \
			pkg-config \
			scrot \
		; \
	else \
		echo "Unsupported operating system." >&2; \
		exit 1; \
	fi; \

clean:
	for folder in *-src; do \
		test -e "$$folder" || continue; \
		echo "- $$folder"; \
		$(MAKE) -s "clean-$$folder"; \
	done
	rm -f .MARK bin/*
	echo "Done cleaning."

cleaner:
	rm -rf .MARK *-src bin
	echo "All external source trees and compiled binaries deleted."

printvar:
	echo '$($(VARIABLE))'

$(HOME)/.config/Trolltech.conf: presentation/qt.conf
	cp $^ $@

$(HOME)/.del: $(PREFIX)/bin/del
	echo st > $@
	del -r > /dev/null

$(HOME)/.fonts.conf: presentation/fonts.conf
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.gtkrc-2.0: presentation/gtk-2.0.gtkrc
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.xsession: xsession
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(TERMINFO)/s/st: st-src/st.info
	tic -s -x st-src/st.info

.ALWAYS_RUN:

# The combination of the "+" command prefix and "-n" is used to force make to
# display the command being executed even if "-s" is inherited from the
# invoking make.
$(UTILITIES): .ALWAYS_RUN
	mkdir -p bin
	source="utilities/$(BASENAME).c"; \
	make=$$(sed -n "/^[^A-Za-z]* Make: /{s///p;q;}; /^$$/q" "$$source"); \
	echo "$@: $$source; +$${make:?$@: recipe not found}" | $(MAKE) -n -f -

dmenu-src/dmenu: dmenu-src .ALWAYS_RUN
	ln -s -f ../dx-config.mk dmenu-src/config.mk
	ln -s -f ../dmenu-config.h dmenu-src/config.h
	$(MAKE) -s patch-dmenu
	(cd dmenu-src && $(MAKE) -s dmenu)

bin/dmenu: dmenu-src/dmenu
	mkdir -p bin
	cp $? $@

dmenu: bin/dmenu

dwm-src/dwm: dwm-src .ALWAYS_RUN
	ln -s -f ../dx-config.mk dwm-src/config.mk
	ln -s -f ../dwm-config.h dwm-src/config.h
	$(MAKE) -s patch-dwm
	(cd dwm-src && $(MAKE) -s dwm)

bin/dwm: dwm-src/dwm
	mkdir -p bin
	cp $? $@

dwm: bin/dwm

st-src/st: st-src .ALWAYS_RUN
	ln -s -f ../st-config.h st-src/config.h
	$(MAKE) -s patch-st
	(cd st-src && $(MAKE) -s st)

bin/st: st-src/st
	mkdir -p bin
	cp $? $@

st: bin/st

slock-src/slock: slock-src .ALWAYS_RUN
	ln -s -f ../slock-config.h slock-src/config.h
	$(MAKE) -s patch-slock
	(cd slock-src && $(MAKE) -s $(SLOCK_VARS) slock)

slock: slock-src/slock

$(SUPERUSER_PREFIX)/bin/slock: slock-src/slock
	sudo install -m 4755 -g 0 -o 0 $? $@
