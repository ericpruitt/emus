discrete-scroll: discrete-scroll.m
	clang -x objective-c -arch x86_64 -fobjc-arc -fmodules \
		-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
		$? -o $@

clean:
	rm -f discrete-scroll
