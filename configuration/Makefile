# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:
.SILENT:

# Dotfiles follow a simple pattern: the source file is the basename of the
# destination with the leading dot removed, and installation consists of
# creating a single symlink.
DOTFILES = \
	$(HOME)/.abcde.conf \
	$(HOME)/.bashrc \
	$(HOME)/.config/flake8 \
	$(HOME)/.dircolors \
	$(HOME)/.elinks/elinks.conf \
	$(HOME)/.fonts.conf \
	$(HOME)/.gemrc \
	$(HOME)/.gitconfig \
	$(HOME)/.gitignore \
	$(HOME)/.gtkrc-2.0 \
	$(HOME)/.inputrc \
	$(HOME)/.jqcolors \
	$(HOME)/.profile \
	$(HOME)/.repl.py \
	$(HOME)/.ssh \
	$(HOME)/.tmux.conf \
	$(HOME)/.vim \
	$(HOME)/.vimrc \
	$(HOME)/.xsession \

MACOS_DOTFILES = \
	$(HOME)/.aqua-session \

MACOS_USER_CONFIGS = \
	$(HOME)/Library/KeyBindings/DefaultKeyBinding.dict \
	$(HOME)/Library/LaunchAgents/com.codevat.aqua-session.plist \

# Local dotfiles are secondary configuration files that are not tracked by the
# EMUS repository. These are loaded by the main configuration file (the path
# with ".local" removed from the basename) and generally contain configuration
# options that shouldn't be used everywhere e.g. for changing the Git author
# email on work systems.
LOCAL_DOTFILES = \
	$(HOME)/.local.bashrc \
	$(HOME)/.local.gitconfig \
	$(HOME)/.local.profile \
	$(HOME)/.local.xsession \

LOCAL_MACOS_DOTFILES = \
	$(HOME)/.local.aqua-session \

# The $(LOCAL_DOTFILES) symlinks will be pointed to paths in this directory.
LOCALCONFIGS = $(HOME)/localconfigs

# Templated configuration files are installed by copying a template to the
# installation directory or by generating the output file from a script.
TEMPLATED = \
	$(HOME)/.config/Trolltech.conf \
	$(HOME)/.gimp-2.8/sessionrc \
	$(HOME)/.terminfo \

# The ..._HOST_TARGETS are platform-specific templated targets that make
# system-wide changes. When running "make host", only the targets that apply to
# the current platform are installed.
LINUX_HOST_TARGETS = \
	/etc/fstab \
	/etc/modprobe.d/local.conf \
	/etc/sudoers.d/keyboard-leds \
	/etc/systemd/system/lock-on-suspend.service \

# Absolute path to the program used to lock X11 displays.
SCREEN_LOCKER = /usr/local/bin/screen-locker

#                                     ---

ALL_USER_TARGETS = \
	$(DOTFILES) \
	$(LOCAL_DOTFILES) \
	$(LOCAL_MACOS_DOTFILES) \
	$(MACOS_DOTFILES) \
	$(MACOS_USER_CONFIGS) \
	$(TEMPLATED) \

user: $(DOTFILES) $(LOCAL_DOTFILES) $(TEMPLATED)
	chmod 700 ~
	case "$$(uname)" in \
	  Darwin) \
		$(MAKE) macos-user; \
	  ;; \
	esac

macos-user: $(LOCAL_MACOS_DOTFILES) $(MACOS_DOTFILES) $(MACOS_USER_CONFIGS)

host:
	case "$$(uname)" in \
	  Darwin) \
		$(MAKE) $(MACOS_HOST_TARGETS); \
	  ;; \
	  Linux) \
		$(MAKE) $(LINUX_HOST_TARGETS); \
	  ;; \
	esac

prepare uninstall:
	for path in $(ALL_USER_TARGETS); do \
		if [ -h "$$path" ]; then \
			echo rm "$$path"; \
			rm "$$path"; \
		elif [ "$@" = "prepare" ] && [ -e "$$path" ]; then \
			dirname="$$(dirname "$$path")"; \
			basename="$$(basename "$$path")"; \
			destination="$$dirname/_$${basename#.}"; \
			printf "%s -> %s\n" "$$path" "$$destination"; \
			mv -i "$$path" "$$destination"; \
		fi; \
	done

$(DOTFILES) $(MACOS_DOTFILES):
	echo "- $@"
	mkdir -p $(@D)
	basename="$(@F)"; \
	source="$$PWD/$${basename#.}"; \
	test -e "$$source" || source="$$PWD/*/$${basename#.}"; \
	ln -s $$source $@
	if [ -e "$@/Makefile" ]; then \
		(cd "$@" && $(MAKE)); \
	fi

$(LOCAL_DOTFILES) $(LOCAL_MACOS_DOTFILES):
	echo "- $@"
	mkdir -p $(LOCALCONFIGS)
	target="$@"; \
	source="$(HOME)/localconfigs/$${target##*.local.}"; \
	test -e "$$source" || touch "$$source"; \
	ln -f -s "$$source" $@

$(HOME)/.config/Trolltech.conf: qt.conf
	echo "- $@"
	mkdir -p $(@D)
	cp $? $@

$(HOME)/.gimp-2.8/sessionrc: gimp.sessionrc
	echo "- $@"
	mkdir -p $(@D)
	cp $? $@

$(HOME)/.terminfo: terminfo/st.info terminfo/tmux.info
	for path in $?; do \
		echo "- $$path"; \
		TERMINFO="$@" tic -x "$$path"; \
	done
	touch $@

/etc/fstab: linux/fstab.awk
	echo "- $@"
	trap "rm -f $@.tmp" EXIT && $? > $@.tmp && mv $@.tmp $@
	chmod 644 $@
	getent group procfs >/dev/null || groupadd procfs
	usermod -a -G procfs $(SUDO_USER)
	mount -o remount /proc

/etc/modprobe.d/local.conf: linux/modprobe.conf
	echo "- $@"
	cp $? $@

/etc/sudoers.d/keyboard-leds: linux/keyboard-leds.sudoers
	echo "- $@"
	mkdir -p $(@D)
	cp $? $@

/etc/systemd/system/lock-on-suspend.service: linux/lock-on-suspend.service.sh
	echo "- $@"
	trap "rm -f $@.tmp" EXIT && \
	SCREEN_LOCKER=$(SCREEN_LOCKER) SUDO_USER=$(SUDO_USER) $? > $@.tmp && \
	mv $@.tmp $@
	case "$$(systemctl list-unit-files $(@F))" in \
	  *$(@F)*enabled*) \
		systemctl daemon-reload; \
	  ;; \
	  *) \
		systemctl enable $(@F); \
	  ;; \
	esac

$(HOME)/Library/LaunchAgents/com.codevat.aqua-session.plist: macos/aqua-session.plist
	echo "- $@"
	mkdir -p $(@D)
	ln -s $(PWD)/$? $@

$(HOME)/Library/KeyBindings/DefaultKeyBinding.dict: macos/DefaultKeyBinding.dict
	echo "- $@"
	mkdir -p $(@D)
	ln -s $(PWD)/$? $@
