#include "config.h"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    class Engine;
}

extern const Core::Engine* engine_ref;
extern Core::GlobalStreamInfo* streaminfo;

inline const Core::Engine* get_context()
{
    return engine_ref;
}

void set_context(const Core::Engine& instance);

const Core::GlobalStreamInfo& get_global_stream_info();

}
