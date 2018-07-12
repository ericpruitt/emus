/**
 * Discrete Scroll
 *
 * Discrete Scroll runs in the background and allows you to scroll a fixed
 * number of lines with each tick of the wheel.
 *
 * License: MIT (https://opensource.org/licenses/MIT)
 * Original Project: https://github.com/emreyolcu/discrete-scroll
 * Copyright: Emre Yolcu
 * Make: clang -x objective-c -arch x86_64 -fobjc-arc -fmodules \
 *     -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk \
 *      $? -o $@
 */
#import <ApplicationServices/ApplicationServices.h>

#define LINES (3)
#define SIGN(x) ((x) < 0 ? -1 : 1)

CGEventRef cgEventCallback(CGEventTapProxy proxy, CGEventType type,
                           CGEventRef event, void *refcon)
{
    if (!CGEventGetIntegerValueField(event, kCGScrollWheelEventIsContinuous)) {
        int64_t delta = CGEventGetIntegerValueField(event, kCGScrollWheelEventPointDeltaAxis1);

        CGEventSetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1, SIGN(delta) * LINES);
    }

    return event;
}

int main(void)
{
    CFMachPortRef eventTap;
    CFRunLoopSourceRef runLoopSource;

    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, 0,
                                1 << kCGEventScrollWheel, cgEventCallback, NULL);
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);

    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);
    CFRunLoopRun();

    CFRelease(eventTap);
    CFRelease(runLoopSource);

    return 0;
}
