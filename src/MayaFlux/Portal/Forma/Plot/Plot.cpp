#include "Plot.hpp"

#include "MayaFlux/Portal/Forma/Plot/SeriesBuilder.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma::Plot {

Mapped<std::shared_ptr<Kakshya::PlotContainer>> place(
    Surface& surface,
    std::shared_ptr<Buffers::FormaBuffer> buf,
    SeriesSpec spec,
    std::shared_ptr<Kakshya::PlotContainer> container)
{
    auto mapped = make_mapped<std::shared_ptr<Kakshya::PlotContainer>>(
        container, std::move(spec.fn), std::move(buf));

    mapped.element.id = surface.layer().add(mapped.element);
    mapped.state->id = mapped.element.id;
    mapped.force_redraw_on_sync = true;
    mapped.sync();
    return mapped;
}

} // namespace MayaFlux::Portal::Forma::Plot
