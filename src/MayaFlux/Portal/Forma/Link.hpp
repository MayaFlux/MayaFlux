#pragma once

namespace MayaFlux::Portal::Forma {

/**
 * @brief A directed coupling between Forma participants.
 *
 * A Link has two distinct moments:
 *
 * establish - runs once at construction. This is where the coupling is
 *   formed: attaching a processor to a buffer, setting initial state,
 *   registering a relation. For some links this is the entire effect.
 *
 * activate - the repeatable body of the coupling. Closed over whatever
 *   source and target participants are relevant. Expresses the full
 *   transfer: read, transform, apply, any gating. When the link has no
 *   repeatable behavior, activate is []{}. It is always callable.
 *
 * Who calls activate and when is entirely the caller's concern. A Context
 * callback, a version-gating wrapper, a Lila script, another Link's
 * activate body - all are valid drivers.
 */
struct Link {
    std::function<void()> establish;
    std::function<void()> tap { [] { } };

    /**
     * @brief Construct and immediately run establish.
     * @param est  Coupling formation function. Run once here.
     * @param t    Repeatable tap body. Defaults to no-op.
     */
    Link(std::function<void()> est, std::function<void()> t = [] { })
        : establish(std::move(est))
        , tap(std::move(t))
    {
        if (establish)
            establish();
    }

    Link(Link&&) noexcept = default;
    Link& operator=(Link&&) noexcept = default;
    Link(const Link&) = delete;
    Link& operator=(const Link&) = delete;
};

} // namespace MayaFlux::Portal::Forma
