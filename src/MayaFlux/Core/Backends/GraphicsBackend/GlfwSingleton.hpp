#pragma once

#include "GlobalGraphicsInfo.hpp"

namespace MayaFlux::Core {

/**
 * @class GLFWSingleton
 * @brief Singleton utility for managing global GLFW initialization and termination
 *
 * GLFWSingleton ensures that the GLFW library is initialized exactly once per process,
 * and is properly terminated when no more windows are in use. This prevents redundant
 * initialization and resource leaks, and provides a safe, centralized mechanism for
 * managing the GLFW global state.
 *
 * Usage:
 * - Call GLFWSingleton::initialize() before creating any GLFW windows or contexts.
 * - Use mark_window_created() and mark_window_destroyed() to track window lifetimes.
 * - Call GLFWSingleton::terminate() when all windows are destroyed to clean up resources.
 *
 * This class is not intended to be instantiated; all methods and state are static.
 */
class GLFWSingleton {
public:
    /**
     * @brief Initializes the GLFW library if not already initialized
     * @return True if initialization succeeded or was already done, false on failure
     *
     * Sets up the GLFW error callback and calls glfwInit() if needed.
     * Safe to call multiple times; initialization occurs only once.
     */
    static bool initialize();

    /**
     * @brief Terminates the GLFW library if initialized and no windows remain
     *
     * Calls glfwTerminate() only if GLFW was previously initialized and all
     * tracked windows have been destroyed. Resets the initialization state.
     */
    static void terminate();

    /**
     * @brief Increments the count of active GLFW windows
     *
     * Should be called whenever a new GLFW window is created.
     */
    static void mark_window_created() { s_window_count++; }

    /**
     * @brief Decrements the count of active GLFW windows
     *
     * Should be called whenever a GLFW window is destroyed.
     * Ensures the window count does not go below zero.
     */
    static void mark_window_destroyed()
    {
        if (s_window_count > 0)
            s_window_count--;
    }

    /**
     * @brief Enumerates all connected monitors and their information
     * @return A vector of MonitorInfo structs for each connected monitor
     *
     * Queries GLFW for the list of connected monitors and retrieves their
     * properties such as name, dimensions, and current video mode.
     */
    static std::vector<MonitorInfo> enumerate_monitors();

    /**
     * @brief Retrieves information about the primary monitor
     * @return MonitorInfo struct for the primary monitor
     *
     * Queries GLFW for the primary monitor and retrieves its properties.
     */
    static MonitorInfo get_primary_monitor();

    /**
     * @brief Retrieves information about a specific monitor by ID
     * @param id The ID of the monitor to query
     * @return MonitorInfo struct for the specified monitor, or an empty struct if not found
     *
     * Queries GLFW for the list of monitors and returns the one matching the given ID.
     */
    static MonitorInfo get_monitor(int32_t id);

    /**
     * @brief Sets a custom error callback for GLFW errors
     * @param callback A function to be called on GLFW errors, receiving an error code and description
     *
     * Overrides the default GLFW error callback with the provided function.
     * The callback will be invoked whenever a GLFW error occurs.
     */
    static void set_error_callback(std::function<void(int, const char*)> callback);

    /**
     * @brief Checks if GLFW has been initialized
     * @return True if initialized, false otherwise
     */
    static bool is_initialized() { return s_initialized; }

    /**
     * @brief Gets the current count of active GLFW windows
     * @return The number of currently active GLFW windows
     */
    static u_int32_t get_window_count() { return s_window_count; }

private:
    /**
     * @brief Tracks whether GLFW has been initialized
     */
    static bool s_initialized;

    /**
     * @brief Number of currently active GLFW windows
     */
    static u_int32_t s_window_count;

    /**
     * @brief Internal GLFW error callback that forwards to the user-defined callback if set
     * @param error The GLFW error code
     * @param description A human-readable description of the error
     */
    static std::function<void(int, const char*)> s_error_callback;
};

}
