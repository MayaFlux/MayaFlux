#ifdef MAYAFLUX_PLATFORM_MACOS

#include "cocoa_event_injector.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

namespace MayaFlux::Test::CocoaInput {

namespace {

    NSWindow* as_window(void* handle)
    {
        return (__bridge NSWindow*)handle;
    }

    NSView* content_view(void* handle)
    {
        return as_window(handle).contentView;
    }

    NSTimeInterval now()
    {
        return NSProcessInfo.processInfo.systemUptime;
    }

    NSEventModifierFlags to_ns_flags(Modifier mods)
    {
        const auto has = [mods](Modifier flag) {
            return (static_cast<uint8_t>(mods) & static_cast<uint8_t>(flag)) != 0;
        };

        NSEventModifierFlags flags = 0;
        if (has(Modifier::Shift))
            flags |= NSEventModifierFlagShift;
        if (has(Modifier::Control))
            flags |= NSEventModifierFlagControl;
        if (has(Modifier::Option))
            flags |= NSEventModifierFlagOption;
        if (has(Modifier::Command))
            flags |= NSEventModifierFlagCommand;
        if (has(Modifier::CapsLock))
            flags |= NSEventModifierFlagCapsLock;
        return flags;
    }

    NSEvent* key_event(void* window, NSEventType type, uint16_t key_code, Modifier mods, bool is_repeat)
    {
        return [NSEvent keyEventWithType:type
                                location:NSZeroPoint
                           modifierFlags:to_ns_flags(mods)
                               timestamp:now()
                            windowNumber:as_window(window).windowNumber
                                 context:nil
                              characters:@""
             charactersIgnoringModifiers:@""
                               isARepeat:is_repeat
                                 keyCode:key_code];
    }

    NSEvent* scroll_event(CGScrollEventUnit unit, bool continuous, int32_t delta_x, int32_t delta_y)
    {
        CGEventRef cg = CGEventCreateScrollWheelEvent2(nullptr, unit, 2, delta_y, delta_x, 0);
        CGEventSetIntegerValueField(cg, kCGScrollWheelEventIsContinuous, continuous ? 1 : 0);
        NSEvent* event = [NSEvent eventWithCGEvent:cg];
        CFRelease(cg);
        return event;
    }

    id<NSWindowDelegate> delegate_of(void* window)
    {
        return as_window(window).delegate;
    }

    NSNotification* notification(NSNotificationName name, void* window)
    {
        return [NSNotification notificationWithName:name object:as_window(window)];
    }

}

bool display_available()
{
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (!session)
        return false;
    CFRelease(session);

    uint32_t count = 0;
    return CGGetActiveDisplayList(0, nullptr, &count) == kCGErrorSuccess && count > 0;
}

void pump_main_queue()
{
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, false);
}

std::shared_ptr<void> retain(void* window)
{
    if (!window)
        return {};
    return std::shared_ptr<void>(
        const_cast<void*>(CFBridgingRetain(as_window(window))),
        [](void* ref) { CFRelease(ref); });
}

void key_down(void* window, uint16_t key_code, Modifier mods, bool is_repeat)
{
    [content_view(window) keyDown:key_event(window, NSEventTypeKeyDown, key_code, mods, is_repeat)];
}

void key_up(void* window, uint16_t key_code, Modifier mods)
{
    [content_view(window) keyUp:key_event(window, NSEventTypeKeyUp, key_code, mods, false)];
}

void flags_changed(void* window, uint16_t key_code, Modifier flags)
{
    [content_view(window) flagsChanged:key_event(window, NSEventTypeFlagsChanged, key_code, flags, false)];
}

void mouse_move(void* window, double x, double y)
{
    NSView* view = content_view(window);
    const NSPoint location = [view convertPoint:NSMakePoint(x, y) toView:nil];
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                        location:location
                                   modifierFlags:0
                                       timestamp:now()
                                    windowNumber:as_window(window).windowNumber
                                         context:nil
                                     eventNumber:0
                                      clickCount:0
                                        pressure:0.0F];
    [view mouseMoved:event];
}

void mouse_button(void* window, int button, bool down, Modifier mods)
{
    CGEventType type = kCGEventOtherMouseDown;
    CGMouseButton cg_button = kCGMouseButtonCenter;
    if (button == 0) {
        type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
        cg_button = kCGMouseButtonLeft;
    } else if (button == 1) {
        type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
        cg_button = kCGMouseButtonRight;
    } else {
        type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    }

    CGEventRef cg = CGEventCreateMouseEvent(nullptr, type, CGPointZero, cg_button);
    CGEventSetIntegerValueField(cg, kCGMouseEventButtonNumber, button);
    CGEventSetFlags(cg, static_cast<CGEventFlags>(to_ns_flags(mods)));
    NSEvent* event = [NSEvent eventWithCGEvent:cg];
    CFRelease(cg);

    NSView* view = content_view(window);
    if (button == 0) {
        if (down)
            [view mouseDown:event];
        else
            [view mouseUp:event];
    } else if (button == 1) {
        if (down)
            [view rightMouseDown:event];
        else
            [view rightMouseUp:event];
    } else {
        if (down)
            [view otherMouseDown:event];
        else
            [view otherMouseUp:event];
    }
}

void mouse_entered(void* window)
{
    NSEvent* event = [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:now()
                                        windowNumber:as_window(window).windowNumber
                                             context:nil
                                         eventNumber:0
                                      trackingNumber:0
                                            userData:nullptr];
    [content_view(window) mouseEntered:event];
}

void mouse_exited(void* window)
{
    NSEvent* event = [NSEvent enterExitEventWithType:NSEventTypeMouseExited
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:now()
                                        windowNumber:as_window(window).windowNumber
                                             context:nil
                                         eventNumber:0
                                      trackingNumber:0
                                            userData:nullptr];
    [content_view(window) mouseExited:event];
}

void scroll_precise(void* window, int32_t delta_x, int32_t delta_y)
{
    [content_view(window) scrollWheel:scroll_event(kCGScrollEventUnitPixel, true, delta_x, delta_y)];
}

void scroll_lines(void* window, int32_t delta_x, int32_t delta_y)
{
    [content_view(window) scrollWheel:scroll_event(kCGScrollEventUnitLine, false, delta_x, delta_y)];
}

void insert_text(void* window, std::u16string_view text)
{
    NSString* string = [NSString stringWithCharacters:reinterpret_cast<const unichar*>(text.data())
                                               length:text.size()];
    id<NSTextInputClient> client = (id<NSTextInputClient>)content_view(window);
    [client insertText:string replacementRange:NSMakeRange(NSNotFound, 0)];
}

void focus_gained(void* window)
{
    [delegate_of(window) windowDidBecomeKey:notification(NSWindowDidBecomeKeyNotification, window)];
}

void focus_lost(void* window)
{
    [delegate_of(window) windowDidResignKey:notification(NSWindowDidResignKeyNotification, window)];
}

void minimized(void* window)
{
    [delegate_of(window) windowDidMiniaturize:notification(NSWindowDidMiniaturizeNotification, window)];
}

void restored(void* window)
{
    [delegate_of(window) windowDidDeminiaturize:notification(NSWindowDidDeminiaturizeNotification, window)];
}

void live_resize_began(void* window)
{
    [delegate_of(window) windowWillStartLiveResize:notification(NSWindowWillStartLiveResizeNotification, window)];
}

void live_resize_ended(void* window)
{
    [delegate_of(window) windowDidEndLiveResize:notification(NSWindowDidEndLiveResizeNotification, window)];
}

bool close_requested(void* window)
{
    return [delegate_of(window) windowShouldClose:as_window(window)];
}

std::string title(void* window)
{
    return window ? std::string(as_window(window).title.UTF8String) : std::string {};
}

bool is_visible(void* window)
{
    return window && as_window(window).isVisible;
}

bool has_delegate(void* window)
{
    return window && as_window(window).delegate != nil;
}

std::pair<double, double> content_size(void* window)
{
    const NSSize size = content_view(window).bounds.size;
    return { size.width, size.height };
}

std::pair<double, double> content_origin(void* window)
{
    NSWindow* native = as_window(window);
    const NSRect content = [native contentRectForFrameRect:native.frame];
    const CGFloat primary_height = NSMaxY(NSScreen.screens.firstObject.frame);
    return { content.origin.x, primary_height - (content.origin.y + content.size.height) };
}

double backing_scale(void* window)
{
    return as_window(window).backingScaleFactor;
}

bool is_metal_layer(void* layer)
{
    return layer && [(__bridge CALayer*)layer isKindOfClass:[CAMetalLayer class]];
}

double layer_scale(void* layer)
{
    return layer ? ((__bridge CALayer*)layer).contentsScale : 0.0;
}

}

#endif
