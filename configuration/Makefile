# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:
.SILENT:

TARGETS = \
	$(HOME)/.abcde.conf \
	$(HOME)/.bashrc \
	$(HOME)/.dir_colors \
	$(HOME)/.elinks/elinks.conf \
	$(HOME)/.gitconfig \
	$(HOME)/.gitignore \
	$(HOME)/.inputrc \
	$(HOME)/.pyrepl.py \
	$(HOME)/.profile \
	$(HOME)/.terminfo \
	$(HOME)/.terminfo/t/tmux \
	$(HOME)/.tmux.conf \
	$(HOME)/.vim \
	$(HOME)/.vimrc \
	$(HOME)/bin \

BACKUP_FOLDER = $(HOME)/config-backups

PLATFORM = $$(uname -s -m | tr "A-Z " "a-z-" | sed "s/x86_64/amd64/g")
USERLAND = https://www.codevat.com/downloads/static-unix-userland-$(PLATFORM)

all: $(TARGETS)

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
