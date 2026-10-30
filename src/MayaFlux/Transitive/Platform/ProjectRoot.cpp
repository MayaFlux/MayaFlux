namespace MayaFlux::Config {

namespace {
    std::string s_project_root;
}

std::string project_root()
{
    return s_project_root.empty() ? std::string(SOURCE_DIR) : s_project_root;
}

void set_project_root(std::string path)
{
    s_project_root = std::move(path);
}

}
