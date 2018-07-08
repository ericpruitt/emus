# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:
.SILENT:

USER_TARGETS = \
	$(HOME)/.abcde.conf \
	$(HOME)/.bashrc \
	$(HOME)/.dir_colors \
	$(HOME)/.elinks/elinks.conf \
	$(HOME)/.gimp/sessionrc \
	$(HOME)/.gitconfig \
	$(HOME)/.gitignore \
	$(HOME)/.inputrc \
	$(HOME)/.profile \
	$(HOME)/.repl.py \
	$(HOME)/.ssh \
	$(HOME)/.terminfo \
	$(HOME)/.terminfo/t/tmux \
	$(HOME)/.tmux.conf \
	$(HOME)/.vim \
	$(HOME)/.vimrc \
	$(HOME)/bin \

LOCAL_USER_TARGETS = \
	$(HOME)/.local.bashrc \
	$(HOME)/.local.gitconfig \
	$(HOME)/.local.profile \

HOST_TARGETS = \
	/etc/modprobe.d/local.conf \
	/etc/systemd/system/lock-on-suspend.service \
	linux-tmpfs-tmp \
	macos-ssh-server \

TARGETS = \
	$(HOST_TARGETS) \
	$(LOCAL_USER_TARGETS) \
	$(USER_TARGETS) \

SCREEN_LOCKER = /usr/local/bin/screen-locker

BACKUP_FOLDER = $(HOME)/config-backups

PLATFORM = $$(uname -s -m | tr "A-Z " "a-z-" | sed "s/x86_64/amd64/g")
USERLAND = https://www.codevat.com/downloads/static-unix-userland-$(PLATFORM)

# When installing the default GIMP session configuration, "gimp --version" is
# used to find out which version of GIMP is installed since the configuration
# path is version dependent. If GIMP has not been installed, this value is used
# as the fallback version number.
FALLBACK_GIMP_VERSION = 2.8

all: user
	if ! command -v bash | fgrep -q "$(HOME)" && \
	  wget -q --spider $(USERLAND); then \
		$(MAKE) userland; \
	fi
	$(MAKE) host

user: $(USER_TARGETS) $(LOCAL_USER_TARGETS)

host:
	for target in $(HOST_TARGETS); do \
		case "$$target" in \
		  *linux-*) uname | grep -iq linux || continue ;; \
		  *macos-*) uname | grep -iq darwin || continue ;; \
		  *) test -d "$$(dirname -- $$target)" || continue ;; \
		esac; \
		targets="$$targets $$target"; \
	done; \
	test -n "$$targets" || exit 0; \
	if [ "$$(id -u)" -ne 0 ]; then \
		command -v sudo > /dev/null && exec sudo $(MAKE) $$targets; \
		echo "make: $@: target must executed as root" >&2; \
		exit 1; \
	fi; \
	$(MAKE) $$targets; \

userland:
	trap 'cd / && rm -rf "$$tempdir"' EXIT; \
	tempdir="$$(mktemp -d)"; \
	mkdir "$$tempdir/.gpg"; \
	export GNUPGHOME="$$tempdir/.gpg"; \
	gpg --import "public-keys/eric-pruitt.asc"; \
	cd "$$tempdir"; \
	wget --max-redirect=0 $(USERLAND); \
	gpg --use-embedded-filename *; \
	test ! -e *.xz || unxz *.xz; \
	tar -x -f *.tar; \
	cd */; \
	$(MAKE); \

clean:
	mkdir -p "$(BACKUP_FOLDER)"
	for target in $(TARGETS); do \
		test -n "$${target##*terminfo*}" || continue; \
		if [ -h "$$target" ]; then \
			rm "$$target"; \
		elif [ -e "$$target" ] && ! [ -d "$$target" ]; then \
			mv "$$target" "$(BACKUP_FOLDER)"; \
		fi; \
	done
	if ! rmdir "$(BACKUP_FOLDER)" 2>/dev/null; then \
		echo "Existing files backed up in $(BACKUP_FOLDER):"; \
		ls -l $(BACKUP_FOLDER); \
	fi

.DEFAULT:
	for target in $(TARGETS) ""; do \
		if [ -z "$$target" ]; then \
			echo "make: no rule to make target '$@'" >&2; \
			exit 1; \
		elif [ "$@" = "$$target" ]; then \
			echo "- $@"; \
			break; \
		fi \
	done
	basename="$(@F)" && ln -s "$$PWD/$${basename#.}" $@
	if [ -e "$@/Makefile" ]; then \
		(cd "$@" && $(MAKE)); \
	fi

$(LOCAL_USER_TARGETS):
	mkdir -p $(HOME)/localconfigs
	stem="$$(echo '$@' | sed 's/.*\.local\.//')" && \
	source="$(HOME)/localconfigs/$$stem" &&  \
	test -e "$$source" || touch "$$source" && \
	ln -s "$$source" $@

# When using "-B" with GNU Make, it attempts to create the target "Makefile"
# when a .DEFAULT build rule is defined. This no-op build target eliminates
# that behavior.
Makefile:

$(HOME)/bin:
	mkdir $@

$(HOME)/.elinks:
	mkdir $(HOME)/.elinks

$(HOME)/.elinks/elinks.conf: elinks.conf
	echo "- $@"
	mkdir -p $(@D)
	test ! -h $@ || rm $@
	ln -s "$$PWD/$?" $@

$(HOME)/.gimp/sessionrc: gimp.sessionrc
	if ! [ -e $(@D) ]; then \
		major_minor_version="$$( \
		    LC_ALL=C gimp --version \
		    | sed -e 's/.* //' -e 's/^\([0-9]\+[.][0-9]\+\)[.].*/\1/' \
		)"; \
		test -n "$${major_minor_version:=$(FALLBACK_GIMP_VERSION)}"; \
		config_directory=".gimp-$$major_minor_version"; \
		mkdir -p $(HOME)/"$$config_directory"; \
		ln -s "$$config_directory" $(@D); \
	fi
	if [ -e $@ ]; then \
		touch $@; \
	else \
		cp $^ $@; \
	fi

$(HOME)/.terminfo/t/tmux: terminfo/tmux.info
	echo "- $@"
	mkdir -p $(@D)
	find terminfo/ -name "*.info" -print -exec tic {} ";"
	touch $@

#
#                                --- Linux ---
#

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

#
#                                --- macOS ---
#

/etc/ssh/com.codevat.macos-sshd_config: macos/sshd_config
	cp $? $@

/Library/LaunchDaemons/com.codevat.macos-sshd.plist: macos/sshd.plist
	cp $? $@

macos-ssh-server: /etc/ssh/com.codevat.macos-sshd_config /Library/LaunchDaemons/com.codevat.macos-sshd.plist
	launchctl load -w /Library/LaunchDaemons/com.codevat.macos-sshd.plist
