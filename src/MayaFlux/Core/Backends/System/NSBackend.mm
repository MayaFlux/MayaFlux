#ifdef MAYAFLUX_PLATFORM_MACOS

#include "NSBackend.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"

#import <AppKit/AppKit.h>

namespace MayaFlux::Core {

namespace {

    /**
     * @brief Build an NSArray of NSString extension entries from SystemFileFilter list.
     */
    NSArray<NSString*>* build_allowed_types(const std::vector<SystemFileFilter>& filters)
    {
        if (filters.empty()) {
            return nil;
        }

        NSMutableArray<NSString*>* types = [NSMutableArray array];
        for (const auto& f : filters) {
            for (const auto& ext : f.extensions) {
                if (ext != "*") {
                    [types addObject:[NSString stringWithUTF8String:ext.c_str()]];
                }
            }
        }
        return types.count > 0 ? types : nil;
    }

    /**
     * @brief Run NSOpenPanel on the main thread and deliver result via @p callback.
     */
    void run_open_panel(
        FileDialogCallback callback,
        const std::vector<SystemFileFilter>& filters,
        const std::filesystem::path& start_dir)
    {
        if (!NSApp) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
            [NSApp finishLaunching];
        }
        auto work = [&] {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.canChooseFiles = YES;
            panel.canChooseDirectories = NO;
            panel.allowsMultipleSelection = NO;

            NSArray<NSString*>* types = build_allowed_types(filters);
            if (types) {
                if (@available(macOS 11.0, *)) {
                    NSMutableArray<UTType*>* ut_types = [NSMutableArray array];
                    for (NSString* ext in types) {
                        UTType* ut = [UTType typeWithFilenameExtension:ext];
                        if (ut) {
                            [ut_types addObject:ut];
                        }
                    }
                    if (ut_types.count > 0) {
                        panel.allowedContentTypes = ut_types;
                    }
                } else {
                    panel.allowedFileTypes = types;
                }
            }

            if (!start_dir.empty()) {
                NSString* dir_str = [NSString stringWithUTF8String:start_dir.c_str()];
                panel.directoryURL = [NSURL fileURLWithPath:dir_str isDirectory:YES];
            }

            NSModalResponse response = [panel runModal];

            if (response != NSModalResponseOK) {
                callback(std::unexpected(SystemDialogError::Cancelled));
                return;
            }

            NSURL* url = panel.URL;
            if (!url) {
                callback(std::unexpected(SystemDialogError::BackendError));
                return;
            }

            callback(std::filesystem::path(url.fileSystemRepresentation));
        };
        if ([NSThread isMainThread]) {
           work();
        } else {
            Parallel::dispatch_main_sync(work);
        }
    }

    /**
     * @brief Run NSSavePanel on the main thread and deliver result via @p callback.
     */
    void run_save_panel(
        FileDialogCallback callback,
        const std::string& suggested_name,
        const std::vector<SystemFileFilter>& filters,
        const std::filesystem::path& start_dir)
    {
        if (!NSApp) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
            [NSApp finishLaunching];
        }
        auto work = [&] {
            NSSavePanel* panel = [NSSavePanel savePanel];

            NSArray<NSString*>* types = build_allowed_types(filters);
            if (types) {
                if (@available(macOS 11.0, *)) {
                    NSMutableArray<UTType*>* ut_types = [NSMutableArray array];
                    for (NSString* ext in types) {
                        UTType* ut = [UTType typeWithFilenameExtension:ext];
                        if (ut) {
                            [ut_types addObject:ut];
                        }
                    }
                    if (ut_types.count > 0) {
                        panel.allowedContentTypes = ut_types;
                    }
                } else {
                    panel.allowedFileTypes = types;
                }
            }

            if (!suggested_name.empty()) {
                panel.nameFieldStringValue = [NSString stringWithUTF8String:suggested_name.c_str()];
            }

            if (!start_dir.empty()) {
                NSString* dir_str = [NSString stringWithUTF8String:start_dir.c_str()];
                panel.directoryURL = [NSURL fileURLWithPath:dir_str isDirectory:YES];
            }

            NSModalResponse response = [panel runModal];

            if (response != NSModalResponseOK) {
                callback(std::unexpected(SystemDialogError::Cancelled));
                return;
            }

            NSURL* url = panel.URL;
            if (!url) {
                callback(std::unexpected(SystemDialogError::BackendError));
                return;
            }

            callback(std::filesystem::path(url.fileSystemRepresentation));
        };
        if ([NSThread isMainThread]) {
           work();
        } else {
            Parallel::dispatch_main_sync(work);
        }
    }

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

bool NSBackend::initialize()
{
    if (m_initialized) {
        return true;
    }
    m_initialized = true;
    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "NSBackend initialized");
    return true;
}

void NSBackend::shutdown()
{
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
    MF_INFO(Journal::Component::Core, Journal::Context::API,
        "NSBackend shutdown");
}

// =============================================================================
// Dialog operations
// =============================================================================

void NSBackend::open_file(
    FileDialogCallback callback,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    if (!m_initialized) {
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }
    run_open_panel(std::move(callback), filters, start_dir);
}

void NSBackend::save_file(
    FileDialogCallback callback,
    std::string suggested_name,
    std::vector<SystemFileFilter> filters,
    std::filesystem::path start_dir)
{
    if (!m_initialized) {
        callback(std::unexpected(SystemDialogError::BackendError));
        return;
    }
    run_save_panel(std::move(callback), suggested_name, filters, start_dir);
}

} // namespace MayaFlux::Core

#endif // MAYAFLUX_PLATFORM_MACOS
