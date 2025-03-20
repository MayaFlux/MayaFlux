#include "Core/Engine.hpp"

namespace MayaFlux {

const Core::Engine* engine_ref;
Core::GlobalStreamInfo* streaminfo;

void set_context(const Core::Engine& instance)
{
    ::MayaFlux::engine_ref = &instance;
    *::MayaFlux::streaminfo = instance.get_stream_settings()->get_global_stream_info();
}

const Core::GlobalStreamInfo& get_global_stream_info()
{
    return *::MayaFlux::streaminfo;
}
}
