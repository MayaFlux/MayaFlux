#pragma once

#include "GLFW/glfw3.h"

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
    static bool initialize()
    {
        if (s_initialized)
            return true;

        glfwSetErrorCallback([](int error, const char* description) {
            std::cerr << "GLFW Error " << error << ": " << description << std::endl;
        });

        if (!glfwInit()) {
            return false;
        }

        s_initialized = true;
        s_window_count = 0;
        return true;
    }

    /**
     * @brief Terminates the GLFW library if initialized and no windows remain
     *
     * Calls glfwTerminate() only if GLFW was previously initialized and all
     * tracked windows have been destroyed. Resets the initialization state.
     */
    static void terminate()
    {
        if (s_initialized && s_window_count == 0) {
            glfwTerminate();
            s_initialized = false;
        }
    }

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

private:
    /**
     * @brief Tracks whether GLFW has been initialized
     */
    static bool s_initialized;

    /**
     * @brief Number of currently active GLFW windows
     */
    static u_int32_t s_window_count;
};

}
