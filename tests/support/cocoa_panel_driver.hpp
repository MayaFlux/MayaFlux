#pragma once

#ifdef MAYAFLUX_PLATFORM_MACOS

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @namespace MayaFlux::Test::CocoaPanel
 * @brief Observes and dismisses the next modal NSOpenPanel or NSSavePanel.
 *
 * Panels run in a modal session that blocks the calling thread, so a test
 * cannot interact with one after starting it. Schedule the driver first. It
 * polls from main run loop timers, which keep firing inside the modal loop
 * even when the panel was started from a main-queue block. Once the panel is
 * modal it records how the panel was configured, then accepts or cancels it.
 */
namespace MayaFlux::Test::CocoaPanel {

/**
 * @brief How the driver dismisses the panel.
 */
enum class PanelAction : uint8_t {
    Cancel,
    Accept,
};

/**
 * @brief Panel configuration captured when the panel became modal.
 *
 * Written on the main thread. Read it only after the dialog call has returned.
 */
struct PanelObservation {
    bool seen { false };
    bool is_open_panel { false };
    bool can_choose_files { false };
    bool can_choose_directories { false };
    bool allows_multiple_selection { false };
    std::string directory;
    std::string name;
    std::vector<std::string> extensions;
};

/**
 * @brief Schedule the driver for the next modal panel.
 * @param action How to dismiss the panel once observed.
 * @return Observation filled when the panel is presented.
 */
[[nodiscard]] std::shared_ptr<PanelObservation> drive_next_panel(PanelAction action);

}

#endif
