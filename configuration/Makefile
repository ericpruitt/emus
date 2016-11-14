# Author: Eric Pruitt (https://www.codevat.com)
# License: 2-Clause BSD (http://opensource.org/licenses/BSD-2-Clause)
.POSIX:

TARGETS = \
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
PRECOMPILED = https://www.codevat.com/downloads/static-unix-userland-$$(uname | tr A-Z a-z)-$$(uname -m).tar.xz.gpg

all: $(TARGETS)

precompiled:
	@trap 'cd / && rm -rf "$$tempdir"' EXIT; \
	tempdir="$$(mktemp -d)"; \
	mkdir "$$tempdir/.gpg"; \
	export GNUPGHOME="$$tempdir/.gpg"; \
	cd "$$tempdir"; \
	gpg --import "$$OLDPWD/public-keys/eric-pruitt.asc"; \
	wget --max-redirect=0 -O dist.tar.xz.gpg -c $(PRECOMPILED); \
	gpg --decrypt -o dist.tar.xz dist.tar.xz.gpg; \
	unxz < dist.tar.xz | tar -x; \
	(cd */ && make); \

clean:
	@mkdir -p "$(BACKUP_FOLDER)"
	@for target in $(TARGETS); do \
		test -n "$${target##*terminfo*}" || continue; \
		if [ -h "$$target" ]; then \
			rm "$$target"; \
		elif [ -e "$$target" ] && ! [ -d "$$target" ]; then \
			mv "$$target" "$(BACKUP_FOLDER)"; \
		fi; \
	done
	@if ! rmdir "$(BACKUP_FOLDER)" 2>/dev/null; then \
		echo "Existing files backed up in $(BACKUP_FOLDER):"; \
		ls -l $(BACKUP_FOLDER); \
	fi

.DEFAULT:
	@if ! echo "$@" | grep -qe "$$(printf '%s\n' $(TARGETS))"; then \
		echo "make: $@: unknown target" >&2; \
		exit 1; \
	fi
	@test ! -h $@ || rm $@
	@echo "- $@"
	@basename="$(@F)" && ln -s "$$PWD/$${basename#.}" $@

$(HOME)/bin:
	@mkdir $@

$(HOME)/.elinks:
	@mkdir $(HOME)/.elinks

$(HOME)/.elinks/elinks.conf: elinks.conf
	@echo "- $@"
	@mkdir -p $(@D)
	@test ! -h $@ || rm $@
	@ln -s $(PWD)/$? $@

$(HOME)/.terminfo/t/tmux: terminfo/tmux.info
	@echo "- $@"
	@mkdir -p $(@D)
	@find terminfo/ -name "*.info" -print -exec tic {} ";"
	@touch $@
