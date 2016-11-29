FORCE_PAM = false
PREFIX = $(HOME)
SUPERUSER_PREFIX = /usr/local

DMENU_URL = http://git.suckless.org/dmenu
DMENU_COMMIT = e90b88e12a88d6214c00d5ee58ceb69446aa5ac4

DWM_URL = http://git.suckless.org/dwm
DWM_COMMIT = 839c7f6939368fe5784058975ee95062cc88d4c3

SLOCK_URL = http://git.suckless.org/slock
SLOCK_COMMIT = 2d2a21a90ad1b53594b1b90b97486189ec54afce

ST_URL = http://git.suckless.org/st
ST_COMMIT = e44832408bb3147826c346872b49de105a4d0e0b

CONFIG_TARGETS = \
	$(HOME)/.config/Trolltech.conf \
	$(HOME)/.fonts.conf \
	$(HOME)/.gtkrc-2.0 \
	$(HOME)/.xsession \

UTILITIES = \
	bin/blackwalls \
	bin/del \
	bin/statusline \

BASENAME = $(@F)

.POSIX:
.SILENT: all deps clean cleaner install

all:
	touch .MARK
	$(MAKE) -s dmenu dwm slock st $(UTILITIES)
	find . -name .git -prune -o -newer .MARK -type f -print \
	  | grep -q ^ || echo "make: all targets up to date"
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
		sudo apt-get install \
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
	for folder in *-src/.git; do \
		test -e "$$folder" || contiue; \
		echo "- $${folder%/.git}"; \
		$(MAKE) -s git-reset-"$${folder%/.git}"; \
	done
	rm -f .MARK bin/* slock-src/slock
	echo "Done cleaning."

cleaner:
	rm -rf .MARK *-src bin
	echo "All external source trees and compiled binaries deleted."

$(HOME)/.config/Trolltech.conf: presentation/qt.conf
	cp $^ $@

$(HOME)/.fonts.conf: presentation/fonts.conf
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.gtkrc-2.0: presentation/gtk-2.0.gtkrc
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.xsession: xsession
	test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

.ALWAYS_RUN:

.DEFAULT:
	@target="$@"; \
	case "$$target" in \
	  dmenu-src) \
		git="$(DMENU_URL)"; \
		commit="$(DMENU_COMMIT)"; \
	  ;; \
	  dwm-src) \
		git="$(DWM_URL)"; \
		commit="$(DWM_COMMIT)"; \
	  ;; \
	  slock-src) \
		git="$(SLOCK_URL)"; \
		commit="$(SLOCK_COMMIT)"; \
	  ;; \
	  st-src) \
		git="$(ST_URL)"; \
		commit="$(ST_COMMIT)"; \
	  ;; \
	  patch-*) \
		patch_prefix="$${target#patch-}"; \
	  ;; \
	  git-reset-*) \
		git_reset_folder="$${target#git-reset-}"; \
	  ;; \
	  *) \
		echo "make: $@: target not recognized" >&2; \
		exit 1; \
	  ;; \
	esac; \
	if [ -n "$$git" ]; then \
		git clone "$$git" "$(BASENAME).tmp"; \
		if [ -n "$$commit" ]; then \
			(cd "$(BASENAME).tmp" && git reset --hard "$$commit"); \
		fi; \
		mv "$(BASENAME).tmp" "$(BASENAME)"; \
		exit 0; \
	fi; \
	if [ -n "$$patch_prefix" ]; then \
		cd "$$patch_prefix"-src/; \
		if git diff --quiet \
		  $$(find . -name .git -prune -o -type f); then \
			for patch in ../patches/"$$patch_prefix"-*.diff; do \
				test -e "$$patch" || exit 0; \
				echo "- $${patch##*/}"; \
				patch < "$$patch"; \
			done; \
		fi; \
		exit 0; \
	fi; \
	if [ -n "$$git_reset_folder" ]; then \
		cd "$$git_reset_folder"; \
		git stash save --quiet "make $@"; \
		git clean -d -f -q -x; \
		exit 0; \
	fi; \

# The combination of the "+" command prefix and "-n" is used to force make to
# display the command being executed even if "-s" is inherited from the
# invoking make.
$(UTILITIES): .ALWAYS_RUN
	mkdir -p bin
	source="utilities/$(@F).c"; \
	command=$$(sed -n -e "/^ \* Make: /{s///p;q}" "$$source"); \
	if [ -z "$$command" ]; then \
		echo "$$source: could not extract make recipe" >&2; \
		exit 1; \
	fi; \
	echo "$@: $$source; +$$command" | $(MAKE) -n -f -

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
	(cd slock-src && \
	 $(MAKE) -s $$($(FORCE_PAM) && echo "IN_PASSWD=false") slock)

slock: slock-src/slock

$(SUPERUSER_PREFIX)/bin/slock: slock-src/slock
	sudo install -m 4755 -g 0 -o 0 $? $@
