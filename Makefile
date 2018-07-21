.POSIX:
.SILENT: cattle cattle-ascii-art default emus-ascii-art .git/info/exclude pet \
	pet-ascii-art

GIT_INFO_EXCLUDE = .git/info/exclude
GIT_EXCLUDE_PATTERNS= \
	core/*-*[0-9] \
	core/*-patches \
	core/*-src \
	core/*.tar.* \
	core/makefile \
	core/common \
	core/GNUmakefile \
	core/gnupghome \
	core/libevent* \
	desktop-environment/*-src \
	desktop-environment/bin \
	desktop-environment/screen-locker/screen-locker \

PLATFORM = $$(uname -s -m | tr "A-Z " "a-z-" | sed "s/x86_64/amd64/g")
PRECOMPILED_BINARIES = https://www.codevat.com/downloads/emus-core-$(PLATFORM)

default: ascii-art-mural
	-tput bold 2>/dev/null
	echo "Run \"make pet\" or \"make cattle\" to begin installation." >&2
	-tput sgr0 2>/dev/null
	exit 1

cattle: cattle-ascii-art
	$(MAKE) install-precompiled-binaries
	$(MAKE) user-configuration

pet: pet-ascii-art
	case "$${uname:=$$(uname)}" in \
	  Darwin|FreeBSD|Linux) \
		sudo $(MAKE) host-configuration; \
		if [ "$$uname" != "Darwin" ]; then \
			sudo $(MAKE) core-make-deps; \
		fi; \
		if command -v Xorg >/dev/null 2>&1; then \
			sudo $(MAKE) desktop-environment-make-deps; \
			$(MAKE) build-desktop-environment; \
			$(MAKE) install-desktop-environment; \
		fi; \
		if [ "$$uname" = "Darwin" ]; then \
			$(MAKE) core-make-deps; \
		fi; \
	  ;; \
	  OpenBSD) \
		doas $(MAKE) host-configuration; \
		doas $(MAKE) core-make-deps; \
	  ;; \
	esac
	$(MAKE) $(GIT_INFO_EXCLUDE)
	$(MAKE) build-core
	$(MAKE) install-core
	$(MAKE) user-configuration

install-precompiled-binaries:
	trap 'cd / && rm -rf "$$tempdir"' EXIT; \
	tempdir="$$(mktemp -d)"; \
	mkdir "$$tempdir/.gpg"; \
	export GNUPGHOME="$$tempdir/.gpg"; \
	gpg --import "configuration/public-keys/eric-pruitt.asc"; \
	cd "$$tempdir"; \
	wget --max-redirect=0 $(PRECOMPILED_BINARIES); \
	gpg --use-embedded-filename *; \
	test ! -e *.xz || unxz *.xz; \
	tar -x -f *.tar; \
	cd */; \
	$(MAKE)

git: $(GIT_INFO_EXCLUDE)

$(GIT_INFO_EXCLUDE): ALWAYS_RUN
	(set -f && \
	for pattern in $(GIT_EXCLUDE_PATTERNS); do \
		for entry in $$(cat $@ 2>/dev/null); do \
			test "$$entry" = "$$pattern" && continue 2; \
		done; \
		echo "$$pattern" >> $@; \
	done)
	touch $@

user-configuration:
	(cd configuration && $(MAKE) prepare && $(MAKE) user)

host-configuration:
	(cd configuration && $(MAKE) host)

core-make-deps:
	(cd core && $(MAKE) deps)

desktop-environment-make-deps:
	(cd desktop-environment && $(MAKE) deps)

build-core:
	(cd core && ./configure && $(MAKE))

install-core:
	(cd core && $(MAKE) install)

build-desktop-environment:
	(cd desktop-environment && $(MAKE))

install-desktop-environment:
	(cd desktop-environment && $(MAKE) install)

ascii-art-mural: mural.txt
	cat $?

# Delete the cat from the mural.
cattle-ascii-art: mural.txt
	sed < $? \
		-e '3s/|\\.*\\/             /' \
		-e '4s/|o.* )/              /' \
		-e '5s/_\..* \//               /' \
		-e '6s/((.*<  \\/__              /' \
		-e '7s/`.*(\//             /'

# Delete the cow from the mural.
pet-ascii-art: mural.txt
	sed < $? \
		-e '3s/\^.*[?]/         /' \
		-e '4s/([^ ]*_/            /' \
		-e '5s/(_.*\\/                /' \
		-e '6s/|.*|/         /' \
		-e '7s/|| .*|/         /'

# Dummy target used to ensure a recipe is always executed even if it is
# otherwise up to date.
ALWAYS_RUN:
