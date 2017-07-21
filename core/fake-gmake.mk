# Resolve build problems caused by "$TARGET" and "./$TARGET" being treated as
# distinct goals.
.DEFAULT:
	if [ -n "$(USED_FAKE_GMAKE_DEFAULT_TARGET)" ]; then \
		echo "fake-gmake: don't know how to make $@" >&2; \
		exit 1; \
	fi
	path='./$@'; case '$@' in ./*) path='$@'; path="$${path#./}";; esac; \
	$(MAKE) USED_FAKE_GMAKE_DEFAULT_TARGET=1 $$path
