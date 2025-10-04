#include "Windowing.hpp"

#include "Core.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Core/Windowing/WindowManager.hpp"

namespace MayaFlux {

Core::WindowManager& get_window_manager()
{
    return *get_context().get_window_manager();
}

std::shared_ptr<Core::Window> create_window(const Core::WindowCreateInfo& create_info)
{
    return get_window_manager().create_window(create_info);
}

}
