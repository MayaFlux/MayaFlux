#ifdef MAYAFLUX_PLATFORM_MACOS

#include "CocoaWindow.hpp"
#include "KeyMapping.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

namespace MFC = MayaFlux::Core;

namespace {

double event_time(NSEvent* event)
{
    return event ? event.timestamp : NSProcessInfo.processInfo.systemUptime;
}

int32_t translate_mods(NSEventModifierFlags flags)
{
    int32_t mods = 0;
    if (flags & NSEventModifierFlagShift)
        mods |= 0x0001;
    if (flags & NSEventModifierFlagControl)
        mods |= 0x0002;
    if (flags & NSEventModifierFlagOption)
        mods |= 0x0004;
    if (flags & NSEventModifierFlagCommand)
        mods |= 0x0008;
    if (flags & NSEventModifierFlagCapsLock)
        mods |= 0x0010;
    return mods;
}

NSEventModifierFlags modifier_mask(MayaFlux::IO::Keys key)
{
    using MayaFlux::IO::Keys;
    switch (key) {
    case Keys::LShift:
    case Keys::RShift:
        return NSEventModifierFlagShift;
    case Keys::LCtrl:
    case Keys::RCtrl:
        return NSEventModifierFlagControl;
    case Keys::LAlt:
    case Keys::RAlt:
        return NSEventModifierFlagOption;
    case Keys::LSuper:
    case Keys::RSuper:
        return NSEventModifierFlagCommand;
    case Keys::CapsLock:
        return NSEventModifierFlagCapsLock;
    default:
        return 0;
    }
}

bool input_allowed(const MFC::InputConfig& config, MFC::WindowEventType type)
{
    using MFC::WindowEventType;
    switch (type) {
    case WindowEventType::KEY_PRESSED:
    case WindowEventType::KEY_RELEASED:
    case WindowEventType::KEY_REPEAT:
    case WindowEventType::TEXT_INPUT:
        return config.keyboard_enabled;
    case WindowEventType::MOUSE_MOTION:
    case WindowEventType::MOUSE_BUTTON_PRESSED:
    case WindowEventType::MOUSE_BUTTON_RELEASED:
    case WindowEventType::MOUSE_SCROLLED:
    case WindowEventType::MOUSE_ENTERED:
    case WindowEventType::MOUSE_EXITED:
        return config.mouse_enabled;
    default:
        return true;
    }
}

void ensure_application()
{
    static bool ready = false;
    if (ready)
        return;

    [NSApplication sharedApplication];
    if (NSApp.activationPolicy == NSApplicationActivationPolicyProhibited)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
                                          handler:^NSEvent*(NSEvent* event) {
                                              if (event.modifierFlags & NSEventModifierFlagCommand)
                                                  [NSApp.keyWindow sendEvent:event];
                                              return event;
                                          }];

    ready = true;
}

void pump_app_events()
{
    if (!NSApp)
        return;

    @autoreleasepool {
        while (NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:[NSDate distantPast]
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES]) {
            [NSApp sendEvent:event];
        }
    }
}

NSCursor* blank_cursor()
{
    static NSCursor* cursor = [[NSCursor alloc] initWithImage:[[NSImage alloc] initWithSize:NSMakeSize(16, 16)]
                                                      hotSpot:NSZeroPoint];
    return cursor;
}

void activate_application()
{
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

}

@interface MFCocoaNativeWindow : NSWindow
@end

@implementation MFCocoaNativeWindow

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

@end

@interface MFCocoaContentView : NSView <NSTextInputClient>
- (void)setSink:(std::function<void(MFC::WindowEvent)>)sink;
- (void)emit:(MFC::WindowEventType)type time:(double)time data:(MFC::WindowEvent::EventData)data;
- (void)primeSizes;
- (void)emitSizes;
- (void)beginLiveResize;
- (void)endLiveResize;
- (void)releaseHeldKeys;
- (void)setCursorMode:(MFC::CursorMode)mode;
- (void)applyCursorLock;
- (void)releaseCursorLock;
@end

@implementation MFCocoaContentView {
    std::function<void(MFC::WindowEvent)> _sink;
    std::unordered_map<int16_t, MFC::WindowEvent::KeyData> _held_keys;
    NSTrackingArea* _tracking;
    NSMutableAttributedString* _marked_text;
    MFC::CursorMode _cursor_mode;
    bool _cursor_locked;
    bool _in_live_resize;
    double _virtual_x;
    double _virtual_y;
    uint32_t _logical_width;
    uint32_t _logical_height;
    uint32_t _pixel_width;
    uint32_t _pixel_height;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _marked_text = [[NSMutableAttributedString alloc] init];
        _cursor_mode = MFC::CursorMode::NORMAL;
        _cursor_locked = false;
        _in_live_resize = false;
        _virtual_x = 0.0;
        _virtual_y = 0.0;
        _logical_width = 0;
        _logical_height = 0;
        _pixel_width = 0;
        _pixel_height = 0;
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
    }
    return self;
}

- (CALayer*)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)isOpaque
{
    return self.window.isOpaque;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
    return YES;
}

- (void)setSink:(std::function<void(MFC::WindowEvent)>)sink
{
    _sink = std::move(sink);
}

- (void)emit:(MFC::WindowEventType)type time:(double)time data:(MFC::WindowEvent::EventData)data
{
    if (!_sink)
        return;

    MFC::WindowEvent ev;
    ev.type = type;
    ev.timestamp = time;
    ev.data = std::move(data);
    _sink(std::move(ev));
}

- (void)primeSizes
{
    const NSRect bounds = self.bounds;
    const NSRect pixels = [self convertRectToBacking:bounds];
    self.layer.contentsScale = self.window.backingScaleFactor;
    _logical_width = static_cast<uint32_t>(bounds.size.width);
    _logical_height = static_cast<uint32_t>(bounds.size.height);
    _pixel_width = static_cast<uint32_t>(pixels.size.width);
    _pixel_height = static_cast<uint32_t>(pixels.size.height);
}

- (void)emitSizes
{
    const uint32_t logical_width = _logical_width;
    const uint32_t logical_height = _logical_height;
    const uint32_t pixel_width = _pixel_width;
    const uint32_t pixel_height = _pixel_height;

    [self primeSizes];

    if (_in_live_resize)
        return;

    const double time = event_time(nil);

    if (_logical_width != logical_width || _logical_height != logical_height) {
        [self emit:MFC::WindowEventType::WINDOW_RESIZED
              time:time
              data:MFC::WindowEvent::ResizeData { .width = _logical_width, .height = _logical_height }];
    }

    if (_pixel_width != pixel_width || _pixel_height != pixel_height) {
        [self emit:MFC::WindowEventType::FRAMEBUFFER_RESIZED
              time:time
              data:MFC::WindowEvent::ResizeData { .width = _pixel_width, .height = _pixel_height }];
    }
}

- (void)beginLiveResize
{
    _in_live_resize = true;
}

- (void)endLiveResize
{
    _in_live_resize = false;
    [self primeSizes];

    const double time = event_time(nil);
    [self emit:MFC::WindowEventType::WINDOW_RESIZED
          time:time
          data:MFC::WindowEvent::ResizeData { .width = _logical_width, .height = _logical_height }];
    [self emit:MFC::WindowEventType::FRAMEBUFFER_RESIZED
          time:time
          data:MFC::WindowEvent::ResizeData { .width = _pixel_width, .height = _pixel_height }];
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self emitSizes];
}

- (void)updateTrackingAreas
{
    if (_tracking)
        [self removeTrackingArea:_tracking];

    _tracking = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
             | NSTrackingEnabledDuringMouseDrag | NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self addTrackingArea:_tracking];
    [super updateTrackingAreas];
}

- (void)resetCursorRects
{
    if (_cursor_mode == MFC::CursorMode::NORMAL) {
        [super resetCursorRects];
        return;
    }
    [self addCursorRect:self.bounds cursor:blank_cursor()];
}

- (void)setCursorMode:(MFC::CursorMode)mode
{
    _cursor_mode = mode;
    [self.window invalidateCursorRectsForView:self];
    [self applyCursorLock];
}

- (void)applyCursorLock
{
    const bool lock = (_cursor_mode == MFC::CursorMode::DISABLED || _cursor_mode == MFC::CursorMode::CAPTURED)
        && self.window.isKeyWindow;

    if (lock == _cursor_locked)
        return;

    _cursor_locked = lock;

    if (!lock) {
        CGAssociateMouseAndMouseCursorPosition(true);
        return;
    }

    const NSRect content = [self.window convertRectToScreen:[self convertRect:self.bounds toView:nil]];
    const CGFloat primary_height = NSMaxY(NSScreen.screens.firstObject.frame);
    CGWarpMouseCursorPosition(CGPointMake(NSMidX(content), primary_height - NSMidY(content)));
    CGAssociateMouseAndMouseCursorPosition(false);

    _virtual_x = NSMidX(self.bounds);
    _virtual_y = NSMidY(self.bounds);
}

- (void)releaseCursorLock
{
    if (!_cursor_locked)
        return;

    _cursor_locked = false;
    CGAssociateMouseAndMouseCursorPosition(true);
}

- (void)releaseHeldKeys
{
    const double time = event_time(nil);
    for (const auto& [key, data] : _held_keys)
        [self emit:MFC::WindowEventType::KEY_RELEASED time:time data:data];
    _held_keys.clear();
}

- (void)keyDown:(NSEvent*)event
{
    if (!event.isARepeat) {
        const MFC::WindowEvent::KeyData data {
            .key = static_cast<int16_t>(MFC::from_cocoa_key(event.keyCode)),
            .scancode = static_cast<int32_t>(event.keyCode),
            .mods = translate_mods(event.modifierFlags)
        };

        _held_keys[data.key] = data;
        [self emit:MFC::WindowEventType::KEY_PRESSED time:event_time(event) data:data];
    }

    [self interpretKeyEvents:@[ event ]];
}

- (void)keyUp:(NSEvent*)event
{
    const MFC::WindowEvent::KeyData data {
        .key = static_cast<int16_t>(MFC::from_cocoa_key(event.keyCode)),
        .scancode = static_cast<int32_t>(event.keyCode),
        .mods = translate_mods(event.modifierFlags)
    };

    _held_keys.erase(data.key);
    [self emit:MFC::WindowEventType::KEY_RELEASED time:event_time(event) data:data];
}

- (void)flagsChanged:(NSEvent*)event
{
    const auto key = MFC::from_cocoa_key(event.keyCode);
    const NSEventModifierFlags mask = modifier_mask(key);
    if (mask == 0)
        return;

    const MFC::WindowEvent::KeyData data {
        .key = static_cast<int16_t>(key),
        .scancode = static_cast<int32_t>(event.keyCode),
        .mods = translate_mods(event.modifierFlags)
    };

    if (key == MayaFlux::IO::Keys::CapsLock) {
        [self emit:MFC::WindowEventType::KEY_PRESSED time:event_time(event) data:data];
        [self emit:MFC::WindowEventType::KEY_RELEASED time:event_time(event) data:data];
        return;
    }

    bool pressed = (event.modifierFlags & mask) != 0;
    if (pressed && _held_keys.contains(data.key))
        pressed = false;

    if (pressed) {
        _held_keys[data.key] = data;
        [self emit:MFC::WindowEventType::KEY_PRESSED time:event_time(event) data:data];
    } else {
        _held_keys.erase(data.key);
        [self emit:MFC::WindowEventType::KEY_RELEASED time:event_time(event) data:data];
    }
}

- (void)emitMotion:(NSEvent*)event
{
    double x = 0.0;
    double y = 0.0;

    if (_cursor_locked) {
        _virtual_x += event.deltaX;
        _virtual_y += event.deltaY;
        x = _virtual_x;
        y = _virtual_y;
    } else {
        const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
        x = point.x;
        y = point.y;
    }

    [self emit:MFC::WindowEventType::MOUSE_MOTION
          time:event_time(event)
          data:MFC::WindowEvent::MousePosData { .x = x, .y = y }];
}

- (void)emitButton:(NSEvent*)event type:(MFC::WindowEventType)type
{
    const NSInteger number = event.buttonNumber;
    if (number < 0 || number > static_cast<NSInteger>(MayaFlux::IO::MouseButtons::Button8))
        return;

    [self emit:type
          time:event_time(event)
          data:MFC::WindowEvent::MouseButtonData {
                   .button = static_cast<int8_t>(number),
                   .mods = translate_mods(event.modifierFlags) }];
}

- (void)mouseMoved:(NSEvent*)event
{
    [self emitMotion:event];
}

- (void)mouseDragged:(NSEvent*)event
{
    [self emitMotion:event];
}

- (void)rightMouseDragged:(NSEvent*)event
{
    [self emitMotion:event];
}

- (void)otherMouseDragged:(NSEvent*)event
{
    [self emitMotion:event];
}

- (void)mouseDown:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_PRESSED];
}

- (void)mouseUp:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_RELEASED];
}

- (void)rightMouseDown:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_PRESSED];
}

- (void)rightMouseUp:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_RELEASED];
}

- (void)otherMouseDown:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_PRESSED];
}

- (void)otherMouseUp:(NSEvent*)event
{
    [self emitButton:event type:MFC::WindowEventType::MOUSE_BUTTON_RELEASED];
}

- (void)scrollWheel:(NSEvent*)event
{
    double dx = event.scrollingDeltaX;
    double dy = event.scrollingDeltaY;

    if (event.hasPreciseScrollingDeltas) {
        dx *= 0.1;
        dy *= 0.1;
    }

    if (dx == 0.0 && dy == 0.0)
        return;

    [self emit:MFC::WindowEventType::MOUSE_SCROLLED
          time:event_time(event)
          data:MFC::WindowEvent::ScrollData { .x_offset = dx, .y_offset = dy }];
}

- (void)mouseEntered:(NSEvent*)event
{
    [self emit:MFC::WindowEventType::MOUSE_ENTERED time:event_time(event) data:std::monostate {}];
}

- (void)mouseExited:(NSEvent*)event
{
    [self emit:MFC::WindowEventType::MOUSE_EXITED time:event_time(event) data:std::monostate {}];
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    [_marked_text.mutableString setString:@""];

    NSEvent* event = NSApp.currentEvent;
    if (event.modifierFlags & NSEventModifierFlagCommand)
        return;

    NSString* characters = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
    const double time = event_time(event);
    const NSUInteger length = characters.length;

    for (NSUInteger i = 0; i < length; ++i) {
        const unichar unit = [characters characterAtIndex:i];
        uint32_t codepoint = unit;

        if (CFStringIsSurrogateHighCharacter(unit) && i + 1 < length) {
            const unichar low = [characters characterAtIndex:i + 1];
            if (CFStringIsSurrogateLowCharacter(low)) {
                codepoint = CFStringGetLongCharacterForSurrogatePair(unit, low);
                ++i;
            }
        }

        if (MFC::is_text_input_codepoint(codepoint)) {
            [self emit:MFC::WindowEventType::TEXT_INPUT
                  time:time
                  data:MFC::WindowEvent::TextData { .codepoint = codepoint }];
        }
    }
}

- (void)doCommandBySelector:(SEL)selector
{
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    if ([string isKindOfClass:[NSAttributedString class]])
        [_marked_text setAttributedString:string];
    else
        [_marked_text.mutableString setString:string];
}

- (void)unmarkText
{
    [_marked_text.mutableString setString:@""];
}

- (BOOL)hasMarkedText
{
    return _marked_text.length > 0;
}

- (NSRange)markedRange
{
    return _marked_text.length > 0 ? NSMakeRange(0, _marked_text.length) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText
{
    return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    const NSRect content = [self.window convertRectToScreen:[self convertRect:self.bounds toView:nil]];
    return NSMakeRect(content.origin.x, content.origin.y, 0.0, 0.0);
}

@end

@interface MFCocoaWindowDelegate : NSObject <NSWindowDelegate>
- (instancetype)initWithView:(MFCocoaContentView*)view;
@end

@implementation MFCocoaWindowDelegate {
    __weak MFCocoaContentView* _view;
    BOOL _zoomed;
}

- (instancetype)initWithView:(MFCocoaContentView*)view
{
    self = [super init];
    if (self) {
        _view = view;
        _zoomed = NO;
    }
    return self;
}

- (void)emit:(MFC::WindowEventType)type
{
    [_view emit:type time:event_time(nil) data:std::monostate {}];
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    [self emit:MFC::WindowEventType::WINDOW_CLOSED];
    return NO;
}

- (void)windowDidResize:(NSNotification*)notification
{
    [_view emitSizes];

    NSWindow* window = notification.object;
    const BOOL zoomed = window.isZoomed;
    if (zoomed != _zoomed) {
        _zoomed = zoomed;
        [self emit:(zoomed ? MFC::WindowEventType::WINDOW_MAXIMIZED : MFC::WindowEventType::WINDOW_RESTORED)];
    }
}

- (void)windowWillStartLiveResize:(NSNotification*)notification
{
    [_view beginLiveResize];
}

- (void)windowDidEndLiveResize:(NSNotification*)notification
{
    [_view endLiveResize];
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
    [_view emitSizes];
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
    [_view emitSizes];
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
    [self emit:MFC::WindowEventType::WINDOW_FOCUS_GAINED];
    [_view applyCursorLock];
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    [_view releaseHeldKeys];
    [_view applyCursorLock];
    [self emit:MFC::WindowEventType::WINDOW_FOCUS_LOST];
}

- (void)windowDidMiniaturize:(NSNotification*)notification
{
    [self emit:MFC::WindowEventType::WINDOW_MINIMIZED];
}

- (void)windowDidDeminiaturize:(NSNotification*)notification
{
    [self emit:MFC::WindowEventType::WINDOW_RESTORED];
}

@end

namespace MayaFlux::Core {

struct CocoaWindow::Native {
    MFCocoaNativeWindow* window { nil };
    MFCocoaContentView* view { nil };
    MFCocoaWindowDelegate* delegate { nil };
    CAMetalLayer* layer { nil };
};

CocoaWindow::CocoaWindow(const WindowCreateInfo& create_info,
    const GlobalGraphicsConfig& graphics_config)
    : m_native(std::make_shared<Native>())
    , m_create_info(create_info)
    , m_key_repeat_config(graphics_config.key_repeat_config)
{
    m_state.current_width = create_info.width;
    m_state.current_height = create_info.height;
    m_state.is_visible = false;

    auto sink = [this](WindowEvent ev) { push_event(std::move(ev)); };

    Parallel::dispatch_main_sync([&] {
        ensure_application();

        const auto& info = m_create_info;
        NSArray<NSScreen*>* screens = NSScreen.screens;
        NSScreen* screen = (info.monitor_id >= 0 && static_cast<NSUInteger>(info.monitor_id) < screens.count)
            ? screens[static_cast<NSUInteger>(info.monitor_id)]
            : NSScreen.mainScreen;

        NSWindowStyleMask style = NSWindowStyleMaskBorderless;
        if (info.decorated && !info.fullscreen) {
            style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
            if (info.resizable)
                style |= NSWindowStyleMaskResizable;
        }

        const NSRect content = NSMakeRect(0, 0, info.width, info.height);

        MFCocoaNativeWindow* window = [[MFCocoaNativeWindow alloc] initWithContentRect:content
                                                                             styleMask:style
                                                                               backing:NSBackingStoreBuffered
                                                                                 defer:NO];
        if (!window)
            return;

        window.releasedWhenClosed = NO;
        window.restorable = NO;
        window.acceptsMouseMovedEvents = YES;
        window.title = [NSString stringWithUTF8String:info.title.c_str()];

        if (info.fullscreen) {
            [window setFrame:screen.frame display:NO];
            window.level = NSMainMenuWindowLevel + 1;
        } else {
            const NSRect visible = screen.visibleFrame;
            const NSRect frame = [window frameRectForContentRect:content];
            [window setFrameOrigin:NSMakePoint(
                                       NSMidX(visible) - frame.size.width * 0.5,
                                       NSMidY(visible) - frame.size.height * 0.5)];
            if (info.floating)
                window.level = NSFloatingWindowLevel;
        }

        MFCocoaContentView* view = [[MFCocoaContentView alloc] initWithFrame:content];
        window.contentView = view;
        [window makeFirstResponder:view];

        CAMetalLayer* layer = (CAMetalLayer*)view.layer;
        if (info.transparent) {
            window.opaque = NO;
            window.backgroundColor = NSColor.clearColor;
            layer.opaque = NO;
        } else {
            layer.opaque = YES;
        }

        MFCocoaWindowDelegate* delegate = [[MFCocoaWindowDelegate alloc] initWithView:view];
        window.delegate = delegate;

        [view setSink:sink];
        [view primeSizes];

        m_native->window = window;
        m_native->view = view;
        m_native->delegate = delegate;
        m_native->layer = layer;
    });

    if (!m_native->window) {
        MF_ERROR(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Failed to create Cocoa window '{}'", m_create_info.title);
        return;
    }

    MF_INFO(Journal::Component::Core, Journal::Context::WindowingSubsystem,
        "Cocoa window '{}' created ({}x{})", m_create_info.title, m_create_info.width, m_create_info.height);
}

CocoaWindow::~CocoaWindow()
{
    destroy();
}

void CocoaWindow::destroy()
{
    auto native = m_native;
    if (!native)
        return;

    Parallel::dispatch_main_sync([native] {
        if (!native->window)
            return;

        [native->view setSink:nullptr];
        [native->view releaseCursorLock];
        native->window.delegate = nil;
        [native->window orderOut:nil];
        [native->window close];

        native->layer = nil;
        native->delegate = nil;
        native->view = nil;
        native->window = nil;
    });
}

void CocoaWindow::poll()
{
    Parallel::dispatch_main_sync([] { pump_app_events(); });

    const auto callback = m_event_callback;
    const auto now = std::chrono::steady_clock::now();

    while (auto ev = m_event_queue.pop()) {
        switch (ev->type) {
        case WindowEventType::KEY_PRESSED:
            if (const auto* key = std::get_if<WindowEvent::KeyData>(&ev->data)) {
                m_held_keys[key->key] = *key;
                m_repeat_deadline = now + std::chrono::milliseconds(m_key_repeat_config.initial_delay_ms);
            }
            break;
        case WindowEventType::KEY_RELEASED:
            if (const auto* key = std::get_if<WindowEvent::KeyData>(&ev->data))
                m_held_keys.erase(key->key);
            break;
        case WindowEventType::WINDOW_RESIZED:
            if (const auto* size = std::get_if<WindowEvent::ResizeData>(&ev->data)) {
                m_state.current_width = size->width;
                m_state.current_height = size->height;
            }
            break;
        case WindowEventType::WINDOW_FOCUS_GAINED:
            m_state.is_focused = true;
            break;
        case WindowEventType::WINDOW_FOCUS_LOST:
            m_state.is_focused = false;
            break;
        case WindowEventType::WINDOW_MINIMIZED:
            m_state.is_minimized = true;
            break;
        case WindowEventType::WINDOW_MAXIMIZED:
            m_state.is_maximized = true;
            break;
        case WindowEventType::WINDOW_RESTORED:
            m_state.is_minimized = false;
            m_state.is_maximized = false;
            break;
        case WindowEventType::MOUSE_ENTERED:
            m_state.is_hovered = true;
            break;
        case WindowEventType::MOUSE_EXITED:
            m_state.is_hovered = false;
            break;
        default:
            break;
        }

        if (!input_allowed(m_input_config, ev->type))
            continue;

        m_event_source.signal(*ev);
        if (callback)
            callback(*ev);
    }

    if (m_held_keys.empty() || now < m_repeat_deadline)
        return;

    const auto interval = std::chrono::milliseconds(
        m_key_repeat_config.interval_ms > 0 ? m_key_repeat_config.interval_ms : 16);
    m_repeat_deadline = now + interval;

    if (!input_allowed(m_input_config, WindowEventType::KEY_REPEAT))
        return;

    const double time = event_time(nil);
    for (const auto& [key, data] : m_held_keys) {
        WindowEvent ev;
        ev.type = WindowEventType::KEY_REPEAT;
        ev.timestamp = time;
        ev.data = data;
        m_event_source.signal(ev);
        if (callback)
            callback(ev);
    }
}

void CocoaWindow::push_event(WindowEvent ev)
{
    if (ev.type == WindowEventType::WINDOW_CLOSED)
        m_should_close.store(true);

    (void)m_event_queue.push(ev);
}

bool CocoaWindow::should_close() const
{
    return m_should_close.load();
}

void CocoaWindow::show()
{
    m_state.is_visible = true;

    auto native = m_native;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!native->window)
            return;
        activate_application();
        [native->window makeKeyAndOrderFront:nil];
    });
}

void CocoaWindow::hide()
{
    m_state.is_visible = false;

    auto native = m_native;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (native->window)
            [native->window orderOut:nil];
    });
}

void CocoaWindow::set_input_config(const InputConfig& config)
{
    m_input_config = config;

    if (config.raw_mouse_motion) {
        MF_WARN(Journal::Component::Core, Journal::Context::WindowingSubsystem,
            "Raw mouse motion is not supported by the Cocoa backend; using pointer deltas");
    }

    auto native = m_native;
    const CursorMode mode = config.cursor_mode;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (native->view)
            [native->view setCursorMode:mode];
    });
}

void CocoaWindow::set_event_callback(WindowEventCallback callback)
{
    m_event_callback = std::move(callback);
}

void* CocoaWindow::get_native_handle() const
{
    return (__bridge void*)m_native->window;
}

void* CocoaWindow::get_metal_layer() const
{
    return (__bridge void*)m_native->layer;
}

void CocoaWindow::set_title(const std::string& title)
{
    m_create_info.title = title;

    auto native = m_native;
    const std::string copy = title;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (native->window)
            native->window.title = [NSString stringWithUTF8String:copy.c_str()];
    });
}

void CocoaWindow::set_size(uint32_t width, uint32_t height)
{
    m_create_info.width = width;
    m_create_info.height = height;

    auto native = m_native;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindow* window = native->window;
        if (!window)
            return;

        NSRect content = [window contentRectForFrameRect:window.frame];
        content.origin.y += content.size.height - height;
        content.size = NSMakeSize(width, height);
        [window setFrame:[window frameRectForContentRect:content] display:YES];
    });
}

void CocoaWindow::set_position(uint32_t x, uint32_t y)
{
    auto native = m_native;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindow* window = native->window;
        if (!window)
            return;

        const CGFloat primary_height = NSMaxY(NSScreen.screens.firstObject.frame);
        NSRect content = [window contentRectForFrameRect:window.frame];
        content.origin = NSMakePoint(x, primary_height - y - content.size.height);
        [window setFrameOrigin:[window frameRectForContentRect:content].origin];
    });
}

void CocoaWindow::set_color(const std::array<float, 4>& color)
{
    m_create_info.clear_color = color;
}

void CocoaWindow::register_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer)
{
    std::lock_guard lock(m_render_tracking_mutex);
    m_rendering_buffers.push_back(buffer);
}

void CocoaWindow::unregister_rendering_buffer(std::shared_ptr<Buffers::VKBuffer> buffer)
{
    std::lock_guard lock(m_render_tracking_mutex);
    std::erase_if(m_rendering_buffers,
        [&buffer](const auto& wp) {
            auto sp = wp.lock();
            return !sp || sp == buffer;
        });
}

void CocoaWindow::track_frame_command(uint64_t cmd_id)
{
    m_frame_commands.push_back(cmd_id);
}

const std::vector<uint64_t>& CocoaWindow::get_frame_commands() const
{
    return m_frame_commands;
}

void CocoaWindow::clear_frame_commands()
{
    m_frame_commands.clear();
}

std::vector<std::shared_ptr<Buffers::VKBuffer>> CocoaWindow::get_rendering_buffers() const
{
    std::lock_guard lock(m_render_tracking_mutex);
    std::vector<std::shared_ptr<Buffers::VKBuffer>> out;
    out.reserve(m_rendering_buffers.size());
    for (const auto& wp : m_rendering_buffers) {
        if (auto sp = wp.lock())
            out.push_back(sp);
    }
    return out;
}

}

#endif
