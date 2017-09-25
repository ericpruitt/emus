#!/usr/bin/make -f
.POSIX:

KNOWN_HOSTS = $(HOME)/.ssh/known-hosts
SSH_KEYS_SECRET = $(HOME)/.ssh/keys.secret

DEFAULT_IDENTITY = $(SSH_KEYS_SECRET)/default
SSH_KEYS_SECRET_FOLDER = $(SSH_KEYS_SECRET)/.FOLDER

# Folders must have a trailing "/" or "/.FOLDER" to ensure an empty directory
# is created instead of a file.
UNTRACKED_FILES = \
	$(HOME)/.ssh/.known-hosts.untracked \
	$(SSH_KEYS_SECRET_FOLDER) \

HOST_KEYS = \
	$(KNOWN_HOSTS)/github.com \
	$(KNOWN_HOSTS)/lan.codevat.com \
	$(KNOWN_HOSTS)/rsync.net \
	$(KNOWN_HOSTS)/suckless.org \

TYPED_SSH_KEYS = \
	$(SSH_KEYS_SECRET)/default-ed25519 \
	$(SSH_KEYS_SECRET)/default-rsa \

LOCAL_CONFIGS = \
	$(HOME)/localconfigs/ssh-pre-config.local \
	$(HOME)/localconfigs/ssh-post-config.local \

TARGETS = \
	$(DEFAULT_IDENTITY) \
	$(HOST_KEYS) \
	$(LOCAL_CONFIGS) \
	keygen \
	keyscan \
	untracked \

all: $(TARGETS)

keygen: $(DEFAULT_IDENTITY) $(TYPED_SSH_KEYS)

keyscan: $(HOST_KEYS)

untracked: $(UNTRACKED_FILES)

clean:
	rm -i -r $(HOST_KEYS)

$(LOCAL_CONFIGS):
	untracked="$$( \
		echo '$@' \
		| sed -e 's:\.local$$:.untracked:' \
		      -e 's:.*localconfigs/ssh:local:' \
	)" && \
	test -e "$$untracked" || ln -s "$@" "$$untracked" && \
	touch $@

$(UNTRACKED_FILES):
	target='$(@:/.FOLDER=)'; \
	symlink="$${target%.*}"; \
	if [ -e "$$symlink" ] || [ -L "$$symlink" ]; then \
	    echo "$$symlink: already exists" >&2 && exit 1; \
	fi; \
	case "$@" in \
	  */.FOLDER)    mkdir $(@D) && touch $@ ;; \
	  */)           mkdir $@ ;; \
	  *)            (set -o noclobber && > $@) ;; \
	esac; \
	ln -s "$${target##*/}" "$$symlink"

$(TYPED_SSH_KEYS): $(SSH_KEYS_SECRET_FOLDER)
	case "$@" in \
	  *-rsa)        ssh-keygen -t rsa     -b 16384 -N "" -f $@ ;; \
	  *-ed25519)    ssh-keygen -t ed25519          -N "" -f $@ ;; \
	  *)            echo "$@: unknown key format" >&2; exit 1  ;; \
	esac

$(DEFAULT_IDENTITY): $(SSH_KEYS_SECRET)/default-ed25519
	test ! -L $@.pub || rm -f $@.pub
	ln -s default-ed25519.pub $@.pub
	test ! -L $@ || rm -f $@
	ln -s default-ed25519 $@

$(KNOWN_HOSTS)/github.com:
	mkdir -p $(@D)
	ssh-keyscan $(@F) > $@.tmp
	links -dump "https://help.github.com/articles/github-s-ssh-key-fingerprints/" > $@.txt
	trap 'rm -f $@.txt' EXIT; \
	for key in $$(ssh-keygen -l -f $@.tmp | awk '{print $$2}'); do \
		fgrep -i -q -w -e "$$key" $@.txt && continue; \
		echo "$$key: not found in fingerprint file for $(@F)" >&2; \
		exit 1; \
	done
	mv $@.tmp $@

# The IP addresses for keys pulled from the LAN are replaced with "*" so things
# like changing IP addresses and identical SSH keys used on different hosts do
# not produce warnings or errors.
$(KNOWN_HOSTS)/lan.codevat.com:
	mkdir -p $(@D)
	networks="$$(ip route show default | awk '/src/ { print $$1 }')"; \
	test -n "$$networks"; \
	nmap -oG - -n -p 22 --open $$networks \
	| awk '/Status: Up/ {print $$2}' \
	| ssh-keyscan -f - \
	| awk '{ $$1 = "*"; print }' > $@.tmp
	mv $@.tmp $@

$(KNOWN_HOSTS)/suckless.org:
	mkdir -p $(@D)
	ssh-keyscan $(@F) > $@.tmp
	mv $@.tmp $@

$(KNOWN_HOSTS)/rsync.net:
	mkdir -p $(@D)
	wget -qO- https://www.rsync.net/resources/fingerprints.txt \
	| tee $@.txt \
	| awk -F ":" '$$1 ~ /rsync.net$$/ {print $$1}' \
	| ssh-keyscan -f - > $@.tmp
	trap 'rm -f $@.txt' EXIT; \
	for key in $$(ssh-keygen -l -f $@.tmp | awk '{print $$2}'); do \
		fgrep -i -q -w -e "$$key" $@.txt && continue; \
		echo "$$key: not found in fingerprint file for $(@F)" >&2; \
		exit 1; \
	done
	mv $@.tmp $@
