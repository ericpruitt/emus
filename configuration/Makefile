# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:
.SILENT:

BACKUP_FOLDER = $(HOME)/config-backups
LOCALCONFIGS = $(HOME)/localconfigs
SCREEN_LOCKER = /usr/local/bin/screen-locker

SYMLINKS = \
	$(HOME)/.abcde.conf \
	$(HOME)/.bashrc \
	$(HOME)/.dir_colors \
	$(HOME)/.elinks/elinks.conf \
	$(HOME)/.fonts.conf \
	$(HOME)/.gitconfig \
	$(HOME)/.gitignore \
	$(HOME)/.gtkrc-2.0 \
	$(HOME)/.inputrc \
	$(HOME)/.profile \
	$(HOME)/.repl.py \
	$(HOME)/.ssh \
	$(HOME)/.tmux.conf \
	$(HOME)/.vim \
	$(HOME)/.vimrc \
	$(HOME)/.xsession \

LOCALCONFIGS_SYMLINKS = \
	$(HOME)/.local.bashrc \
	$(HOME)/.local.gitconfig \
	$(HOME)/.local.profile \
	$(HOME)/.local.xsession \

TEMPLATES = \
	$(HOME)/.config/Trolltech.conf \
	$(HOME)/.gimp-2.8/sessionrc \
	$(HOME)/.terminfo \

LINUX_TARGETS = \
	/etc/fstab \
	/etc/modprobe.d/local.conf \
	/etc/systemd/system/lock-on-suspend.service \

MACOS_TARGETS = \
	/etc/ssh/com.codevat.macos-sshd_config \
	/Library/LaunchDaemons/com.codevat.macos-sshd.plist \
	macos-ssh-server \

user: $(LOCALCONFIGS_SYMLINKS) $(SYMLINKS) $(TEMPLATES)

host:
	case "$$(uname)" in \
	  Darwin) \
		$(MAKE) $(MACOS_TARGETS); \
	  ;; \
	  Linux) \
		$(MAKE) $(LINUX_TARGETS); \
	  ;; \
	esac

clean:
	for target in $(SYMLINKS) $(LOCALCONFIGS_SYMLINKS); do \
		if [ -h "$$target" ]; then \
			rm "$$target"; \
		elif [ -e "$$target" ] && ! [ -d "$$target" ]; then \
			mkdir -p "$(BACKUP_FOLDER)"; \
			mv "$$target" "$(BACKUP_FOLDER)"; \
		fi; \
	done
	if [ -e "$(BACKUP_FOLDER)" ]; then \
		echo "Existing files backed up in $(BACKUP_FOLDER):"; \
		ls -l $(BACKUP_FOLDER); \
	fi

$(SYMLINKS):
	echo "- $@"
	mkdir -p $(@D)
	basename="$(@F)" && ln -f -s "$$PWD/$${basename#.}" $@
	if [ -e "$@/Makefile" ]; then \
		(cd "$@" && $(MAKE)); \
	fi

$(LOCALCONFIGS_SYMLINKS):
	echo "- $@"
	mkdir -p $(LOCALCONFIGS)
	target="$@"; \
	source="$(HOME)/localconfigs/$${target##*.local.}"; \
	test -e "$$source" || touch "$$source"; \
	ln -f -s "$$source" $@

$(HOME)/.config/Trolltech.conf: qt.conf
	mkdir -p $(@D)
	cp $? $@

$(HOME)/.gimp-2.8/sessionrc: gimp.sessionrc
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

/etc/modprobe.d/local.conf: linux/modprobe.conf
	echo "- $@"
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

/etc/ssh/com.codevat.macos-sshd_config: macos/sshd_config
	cp $? $@

/Library/LaunchDaemons/com.codevat.macos-sshd.plist: macos/sshd.plist
	cp $? $@

macos-ssh-server: /etc/ssh/com.codevat.macos-sshd_config /Library/LaunchDaemons/com.codevat.macos-sshd.plist
	launchctl load -w /Library/LaunchDaemons/com.codevat.macos-sshd.plist
