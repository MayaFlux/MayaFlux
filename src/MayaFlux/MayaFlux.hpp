#include "config.h"

namespace MayaFlux {

namespace Core {
    struct GlobalStreamInfo;
    class Engine;
}

namespace Nodes {
    class NodeGraphManager;
}

const Core::GlobalStreamInfo get_global_stream_info();
extern Nodes::NodeGraphManager* graph_manager;
extern const Core::Engine* engine_ref;
extern Core::GlobalStreamInfo streaminfo;

inline const Core::Engine* get_context()
{
    return engine_ref;
}

inline Nodes::NodeGraphManager& get_node_graph_manager()
{
    return *graph_manager;
}

void set_context(const Core::Engine& instance);
}
