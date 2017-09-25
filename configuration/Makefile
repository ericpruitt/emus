# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:
.SILENT:

USER_TARGETS = \
	$(HOME)/.abcde.conf \
	$(HOME)/.bashrc \
	$(HOME)/.dir_colors \
	$(HOME)/.elinks/elinks.conf \
	$(HOME)/.gitconfig \
	$(HOME)/.gitignore \
	$(HOME)/.inputrc \
	$(HOME)/.pyrepl.py \
	$(HOME)/.profile \
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
	linux-lock-on-suspend \
	linux-tmpfs-tmp \

SLOCK_BINARY = /usr/local/bin/slock

BACKUP_FOLDER = $(HOME)/config-backups

PLATFORM = $$(uname -s -m | tr "A-Z " "a-z-" | sed "s/x86_64/amd64/g")
USERLAND = https://www.codevat.com/downloads/static-unix-userland-$(PLATFORM)

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
		  linux-*) uname | grep -iq linux || continue ;; \
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
	if ! echo "$@" | grep -qe "$$(printf '%s\n' $(TARGETS))"; then \
		echo "make: $@: unknown target" >&2; \
		exit 1; \
	fi
	test ! -h $@ || rm $@
	echo "- $@"
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

$(HOME)/.terminfo/t/tmux: terminfo/tmux.info
	echo "- $@"
	mkdir -p $(@D)
	find terminfo/ -name "*.info" -print -exec tic {} ";"
	touch $@

/etc/modprobe.d/local.conf:
	echo "- $@"
	rm -f $@.tmp
	# Disable internal speaker
	echo "blacklist pcspkr" >> $@.tmp
	# Disable floppy disk support since having this enabled can cause slow
	# boot times on systems that support floppy disks but don't have a
	# physical dive present.
	echo "blacklist floppy" >> $@.tmp
	# Depending on how the driver is compiled, FireWire may make the host
	# vulnerable to DMA exploits, so it is disabled since I have no devices
	# that rely on FireWire.
	echo "blacklist firewire_core" >> $@.tmp
	# Disable blinking activity LED for Intel wireless cards.
	case "$$(lsmod)" in *iwlwifi*) \
	  echo "options iwlwifi led_mode=1" >> $@.tmp ;; \
	esac
	# If there's a discrete Sound Blaster sound card installed, blacklist
	# every other PCI audio module to avoid load order / driver precedence
	# issues if there happens to be another device that also has the
	# ability to act as a sound card (e.g. a video card that supports audio
	# over HDMI).
	case "$$(lsmod)" in *snd_emu10k1*) \
	  find "/lib/modules/$$(uname -r)/kernel/sound/pci" -type f \
	    | sed -e '/emu10k/d' \
	          -e 's/\.ko$$//' \
	          -e 's/-/_/g' \
	          -e 's:.*/:blacklist :' >> $@.tmp ;; \
	esac
	chmod 644 $@.tmp
	mv $@.tmp $@

/etc/systemd/system/lock-on-suspend.service:
	echo "- $@"
	if [ -z "$(SUDO_USER)" ]; then \
		echo "make: $@: SUDO_USER must be set to create file" >&2; \
		exit 1; \
	fi
	printf '%s\n' \
		'[Unit]' \
		'Description=Lock screen on suspend' \
		'Before=sleep.target' \
		'[Service]' \
		'User=$(SUDO_USER)' \
		'Type=oneshot' \
		'Environment=DISPLAY=:0' \
		'ExecStart=/bin/sh -c \
			"$(SLOCK_BINARY) & sleep 1 && pgrep -x slock"' \
		'KillMode=none' \
		'[Install]' \
		'WantedBy=sleep.target' \
	  | sudo sh -c "touch $@ && chmod 644 $@ && cat > $@"
	case "$$(systemctl list-unit-files $(@F))" in \
	  *$(@F)*enabled*) systemctl daemon-reload ;; \
	  *)               systemctl enable $(@F) ;; \
	esac

/usr/lib/pm-utils/sleep.d/00lock-screen-on-suspend:
	echo "- $@"
	if [ -z "$(SUDO_USER)" ]; then \
		echo "make: $@: SUDO_USER must be set to create file" >&2; \
		exit 1; \
	fi
	printf '%s\n' \
		'#!/bin/sh' \
		'set -e -u' \
		'case "$${1:-}" in' \
		'  hibernate|suspend)' \
		'    DISPLAY=:0 sudo -u $(SUDO_USER) $(SLOCK_BINARY) &' \
		'    sleep 1' \
		'    pgrep -x slock' \
		'  ;;' \
		'esac' \
	  | sudo sh -c "touch $@ && chmod 755 $@ && cat > $@"

linux-lock-on-suspend:
	if systemctl --version > /dev/null 2>&1; then \
		target="/etc/systemd/system/lock-on-suspend.service"; \
	else \
		target="/usr/lib/pm-utils/sleep.d/00lock-screen-on-suspend"; \
	fi; \
	exec $(MAKE) $$target

linux-tmpfs-tmp:
	if ! egrep -q "^\s*tmpfs\s+/tmp/?\s" /etc/fstab; then \
		echo "tmpfs /tmp/ tmpfs defaults 0" >> /etc/fstab; \
		echo "- mount -t tmpfs tmpfs /tmp/ (reboot required)"; \
	fi
