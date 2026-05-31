#include "Chooser.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/System/System.hpp"

namespace MayaFlux::Portal::System::Dialog {

void open_file(
    ChooserCallback callback,
    std::vector<ChooserFilter> filters,
    std::filesystem::path start_dir)
{
    auto* backend = get_backend();
    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Chooser::open_file: Portal::System not initialized");
        callback(std::unexpected(Core::SystemDialogError::BackendError));
        return;
    }
    backend->open_file(std::move(callback), std::move(filters), std::move(start_dir));
}

void save_file(
    ChooserCallback callback,
    std::string suggested_name,
    std::vector<ChooserFilter> filters,
    std::filesystem::path start_dir)
{
    auto* backend = get_backend();
    if (!backend) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "Chooser::save_file: Portal::System not initialized");
        callback(std::unexpected(Core::SystemDialogError::BackendError));
        return;
    }
    backend->save_file(std::move(callback), std::move(suggested_name), std::move(filters), std::move(start_dir));
}

} // namespace MayaFlux::Portal::System::Dialog
