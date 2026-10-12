#ifdef MAYAFLUX_PLATFORM_MACOS

#include "cocoa_panel_driver.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace MayaFlux::Test::CocoaPanel {

namespace {

    constexpr NSTimeInterval k_poll_interval_s = 0.025;
    constexpr int k_max_attempts = 200;
    constexpr NSTimeInterval k_watchdog_s = 1.0;

    std::string to_string(NSString* value)
    {
        return value ? std::string(value.UTF8String) : std::string {};
    }

    void observe(NSSavePanel* panel, PanelObservation& observation)
    {
        observation.is_open_panel = [panel isKindOfClass:[NSOpenPanel class]];
        observation.directory = to_string(panel.directoryURL.path);
        observation.name = to_string(panel.nameFieldStringValue);

        for (UTType* type in panel.allowedContentTypes) {
            if (NSString* extension = type.preferredFilenameExtension)
                observation.extensions.push_back(to_string(extension));
        }

        if (observation.is_open_panel) {
            NSOpenPanel* open = (NSOpenPanel*)panel;
            observation.can_choose_files = open.canChooseFiles;
            observation.can_choose_directories = open.canChooseDirectories;
            observation.allows_multiple_selection = open.allowsMultipleSelection;
        }

        observation.seen = true;
    }

    void dismiss(NSSavePanel* panel, PanelAction action)
    {
        if (action == PanelAction::Accept)
            [NSApp stopModalWithCode:NSModalResponseOK];
        else
            [panel cancel:nil];

        NSTimer* watchdog = [NSTimer timerWithTimeInterval:k_watchdog_s
                                                   repeats:NO
                                                     block:^(NSTimer*) {
                                                         if (NSApp.modalWindow == panel)
                                                             [panel cancel:nil];
                                                     }];
        [[NSRunLoop mainRunLoop] addTimer:watchdog forMode:NSRunLoopCommonModes];
    }

    void install(const std::shared_ptr<PanelObservation>& observation, PanelAction action)
    {
        auto attempts = std::make_shared<int>(k_max_attempts);
        NSTimer* poller = [NSTimer timerWithTimeInterval:k_poll_interval_s
                                                 repeats:YES
                                                   block:^(NSTimer* timer) {
                                                       NSWindow* modal = NSApp.modalWindow;
                                                       if ([modal isKindOfClass:[NSSavePanel class]]) {
                                                           [timer invalidate];
                                                           observe((NSSavePanel*)modal, *observation);
                                                           dismiss((NSSavePanel*)modal, action);
                                                           return;
                                                       }
                                                       if (--(*attempts) <= 0)
                                                           [timer invalidate];
                                                   }];
        [[NSRunLoop mainRunLoop] addTimer:poller forMode:NSRunLoopCommonModes];
    }

}

std::shared_ptr<PanelObservation> drive_next_panel(PanelAction action)
{
    auto observation = std::make_shared<PanelObservation>();
    if ([NSThread isMainThread]) {
        install(observation, action);
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            install(observation, action);
        });
    }
    return observation;
}

}

#endif
