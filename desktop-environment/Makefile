#!/usr/bin/make -f
DMENU_REPOSITORY = http://git.suckless.org/dmenu
DMENU_REVISION = 026827fd65c1163a92a984c4eae3882a6d69f8df

DWM_REPOSITORY = http://git.suckless.org/dwm
DWM_REVISION = ab9571bbc5f6fb04fd583238a665a7e830fc1397

SLOCK_REPOSITORY = http://git.suckless.org/slock
SLOCK_REVISION = a7afade1701a809f6a33b53525d59dd29b38d381

ST_REPOSITORY = http://git.suckless.org/st
ST_REVISION = e44832408bb3147826c346872b49de105a4d0e0b

PREFIX = $(HOME)
BINARIES = blackwalls del dmenu dwm st statusline
SUPERUSER_PREFIX = /usr/local
SUPERUSER_BINARIES = slock

# If the current user's name does not appear in /etc/passwd, it is presumed
# that the system relies on PAM for authentication.
USE_PAM = $(shell awk -F: '$$1 == "$(USER)" {exit 1}' /etc/passwd && echo 1)

INSTALL_TARGETS = \
	$(addprefix $(PREFIX)/bin/, $(BINARIES)) \
	$(addprefix $(SUPERUSER_PREFIX)/bin/, $(SUPERUSER_BINARIES)) \
	$(HOME)/.config/Trolltech.conf \
	$(HOME)/.del \
	$(HOME)/.fonts.conf \
	$(HOME)/.gtkrc-2.0 \
	$(HOME)/.xsession \

all: $(BINARIES) $(SUPERUSER_BINARIES)

# Dependency function for suckless projects. This accepts the project's name as
# its only argument e.g. $(call suckless, app) would generate a dependency list
# for the suckless project "app".
sucklessdeps = \
	$(1)-src/config.h \
	$(wildcard $(1)-src/*.[ch] $(1)-src/*.mk $(1)-*.[ch] patches/$(1)-*.diff) \
	$(if $(wildcard $(1)-src), , $(1)-src) \
	$(if $(wildcard patches/$(1)-*.diff), $(1)-src/.PATCHED,) \
	$(if $(wildcard $(1)-src/.PATCHED), , $(wildcard patches/$(1)-*.diff)) \

# Generic target for fetching upstream Git repositories: given the target
# "app-src", this recipe will clone the URL specified by $(APP_REPOSITORY) into
# the folder "app-src" and attempt to reset the repository to the revision
# specified by $(APP_REVISION) if it is non-empty string.
%-src:
	@echo "bin" "*-src" | tr ' ' '\n' > .git/info/exclude
	git clone $($(shell echo $(subst -src, ,$@) | tr a-z A-Z)_REPOSITORY) $@
	@revision=$($(shell echo $(subst -src, ,$@) | tr a-z A-Z)_REVISION); \
	if [ -n "$$revision" ]; then \
		cd $@ && git reset --hard $$revision; \
	fi

# Generic target for resetting for a repository: given the target "reset-app",
# this recipe will run "git --reset --hard $(APP_REVISION)" inside "app-src"
# followed by "make clean".
reset-%:
	@set -e; \
	cd $(subst reset-, ,$@)-src; \
	rm -f .PATCHED config.h; \
	git reset -q --hard \
		$($(shell echo $(subst reset-, ,$@) | tr a-z A-Z)_REVISION); \
	$(MAKE) -s clean > /dev/null

# Any folder ending in *-src will have a reset target.
reset: $(addprefix reset-,$(subst -src, ,$(wildcard *-src)))

# Generic target for applying patches to a repository: given the target
# "patched-app", the target apply all patches matching the shell glob
# "patches/app-*.diff" to the contents of the folder "app-src".
%-src/.PATCHED: | %-src
	@prefix=$(subst -src/.PATCHED, ,$@); \
	for patch in patches/$$prefix-*.diff; do \
		if ! [ -e $$patch ]; then \
			continue; \
		fi; \
		echo "- $$patch"; \
		if ! patch -d $$prefix-src -s < $$patch; then \
			exit 1; \
		fi; \
	done
	touch $@

# Generic target for suckless config.h files: given the target
# "app-src/config.h", this recipe will symlink "app-config.h" into the folder
# "app-src".
%-src/config.h: %-config.h | %-src
	test -e $@ || ln -s ../$< $@

bin:
	mkdir $@

# Generic target for utility binaries. The build command is extracted from the
# source code of the file.
bin/%: utilities/%.c | bin
	@command=$$(sed -n -e 's/.*\bBuild:\s\+\(.*\)/\1/p' $<); \
	if [ -z "$$command" ]; then \
		echo "Could not extract build command from \"$<\"." >&2; \
		exit 1; \
	fi; \
	sh -c "INPUT=$<; OUTPUT=$@; echo $$command && exec $$command"

packages:
	@if [ -e /etc/debian_version ]; then \
		sudo apt-get install \
			compton \
			fonts-dejavu \
			fonts-vlgothic \
			fonts-wqy-zenhei \
			gsfonts \
			gtk2-engines \
			libx11-dev \
			libxft-dev \
			libxinerama-dev \
			libxrandr-dev \
			pkg-config \
			scrot \
			$(if $(USE_PAM), libpam-dev,) \
	; else \
		echo "Unsupported operating system." >&2; \
	fi

blackwalls: bin/blackwalls

bin/dmenu: $(call sucklessdeps, dmenu) dx-config.mk | bin
	@ln -s -f ../dx-config.mk dmenu-src/config.mk
	@cd dmenu-src && $(MAKE) -s dmenu
	@mv dmenu-src/dmenu $@

dmenu: bin/dmenu

del: bin/del

$(HOME)/.del: $(PREFIX)/bin/del
	echo st >> ~/.del
	echo virtualbox >> ~/.del
	del -r

bin/dwm: $(call sucklessdeps, dwm) dx-config.mk | bin
	@ln -s -f ../dx-config.mk dwm-src/config.mk
	@rm -f dwm-src/*.o
	@cd dwm-src && $(MAKE) -s
	@mv dwm-src/dwm $@

dwm: bin/dwm

slock-src/slock: $(call sucklessdeps, slock)
	@cd slock-src || exit 1; \
	$(MAKE) -s clean > /dev/null; \
	$(MAKE) USE_PAM=$(USE_PAM) -s

slock: slock-src/slock

bin/st: $(call sucklessdeps, st) | bin
	@cd st-src && $(MAKE) -s
	@mv st-src/st $@

$(TERMINFO)/s/st: | st-src
	tic st-src/st.info

st: bin/st $(TERMINFO)/s/st

statusline: bin/statusline

$(HOME)/.xsession: xsession
	@test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(SUPERUSER_PREFIX)/bin/slock: slock-src/slock
	sudo install -m 4755 -g root -o root $^ $(SUPERUSER_PREFIX)/bin

$(PREFIX)/bin/%: bin/%
	@test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.config/Trolltech.conf: presentation/qt.conf
	cp $^ $@

$(HOME)/.fonts.conf: presentation/fonts.conf
	@test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

$(HOME)/.gtkrc-2.0: presentation/gtk-2.0.gtkrc
	@test ! -h $@ || rm $@
	ln -s $(PWD)/$^ $@

install: $(INSTALL_TARGETS)

clean: reset
	@rm -f bin/* slock-src/slock
	@echo "Done cleaning."

cleaner:
	@rm -rf *-src bin
	@echo "All external repos and compiled binaries have been deleted."

.PHONY: all clean cleaner install packages preinstall reset
.PRECIOUS: %-src %-src/.PATCHED
