.POSIX:
.SILENT: all

BIN = bin

MACOS_TARGETS = \
	$(BIN)/discrete-scroll \

all:
	$(MAKE) "$$(uname | tr A-Z a-z | sed 's/darwin/macos/')"

clean:
	rm -f $(MACOS_TARGETS)
	-test ! -e $(BIN) || rmdir "$(BIN)"

linux: $(LINUX_TARGETS)

macos: $(MACOS_TARGETS)

openbsd: $(OPENBSD_TARGETS)

freebsd: $(FREEBSD_TARGETS)

$(BIN)/discrete-scroll: discrete-scroll.m
	mkdir -p $(@D)
	clang -x objective-c -arch x86_64 -fobjc-arc -fmodules \
		-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
		$? -o $@
