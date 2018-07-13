.POSIX:
.SILENT: cattle cattle-ascii-art default emus-ascii-art .git/info/exclude pet \
	pet-ascii-art

GIT_INFO_EXCLUDE = .git/info/exclude
GIT_EXCLUDE_PATTERNS= \
	core/*-*[0-9] \
	core/*-patches \
	core/*-src \
	core/*.tar.* \
	core/Makefile \
	core/common \
	core/gnupghome \
	core/libevent* \
	desktop-environment/*-src \
	desktop-environment/bin \
	desktop-environment/screen-locker/screen-locker \

PLATFORM = $$(uname -s -m | tr "A-Z " "a-z-" | sed "s/x86_64/amd64/g")
PRECOMPILED_BINARIES = https://www.codevat.com/downloads/emus-core-$(PLATFORM)

default: emus-ascii-art
	-tput bold 2>/dev/null
	echo "Run \"make pet\" or \"make cattle\" to begin installation." >&2
	-tput sgr0 2>/dev/null
	exit 1

cattle: cattle-ascii-art
	$(MAKE) install-precompiled-binaries
	$(MAKE) user-configuration

pet: pet-ascii-art
	$(MAKE) host-configuration
	case "$$(uname)" in \
	  Darwin|FreeBSD|Linux) \
		sudo $(MAKE) core-make-deps; \
		if command -v Xorg >/dev/null 2>&1; then \
			sudo $(MAKE) desktop-environment-make-deps; \
			$(MAKE) build-desktop-environment; \
			$(MAKE) install-desktop-environment; \
		fi; \
	  ;; \
	  OpenBSD) \
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

host-configuration user-configuration:
	(cd configuration && $(MAKE) $(@:-configuration=))

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

cattle-ascii-art:
	printf "%s\n" \
	"$$(hostname | tr a-z A-Z): Cattle!" \
	"     ___" \
	" _.-|   |       ^__^  ???" \
	"{   |   |       (oo)\\_______" \
	" \"-.|___|       (__)\\       )\\/\\" \
	"  .--'-\`-.     ___  ||----w |" \
	".+|______|__.-||__) ||     ||"

emus-ascii-art:
	printf "%s\n" \
	"O\\    O-             EMUS: Eric's Multi-platform UNIX Stack            -O    /O" \
	"\\|__ /                                                                   \\ __|/" \
	" ( _)                                                                     (_ )" \
	" /                                                                           \\" \
	" \\_                                                                         _/" \

pet-ascii-art:
	printf "%s\n" \
	"$$(hostname | tr a-z A-Z): Pet!" \
	"     ___" \
	" _.-|   |          |\\__/,|   (\`\\" \
	"{   |   |          |o o  |__ _) )" \
	" \"-.|___|        _.( T   )  \`  /" \
	"  .--'-\`-.     _((_ \`^--' /_<  \\" \
	".+|______|__.-||__)\`-'(((/  (((/"


# Dummy target used to ensure a recipe is always executed even if it is
# otherwise up to date.
ALWAYS_RUN:
