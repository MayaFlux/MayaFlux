#include "MayaFlux/Transitive/Memory/LiveArena.hpp"

namespace MayaFlux {

namespace internal {

    alignas(std::max_align_t) unsigned char g_live_arena_storage[LIVE_ARENA_CAPACITY] {};
    uint32_t g_live_arena_bump {};
    std::shared_ptr<void> g_live_arena_shared_ptrs[LIVE_ARENA_MAX_ENTRIES] {};

} // namespace internal

MAYAFLUX_API void* live_cast_impl(const char* key) noexcept
{
    auto* hdr = internal::live_arena_header();
    for (uint32_t i = 0; i < hdr->entry_count; ++i) {
        auto& entry = hdr->entries[i];
        if (!entry.occupied) {
            continue;
        }
        if (std::strncmp(entry.key, key, internal::LIVE_ARENA_KEY_MAX) != 0) {
            continue;
        }
        if (entry.offset == UINT32_MAX) {
            return internal::g_live_arena_shared_ptrs[i].get();
        }
        return internal::live_arena_object_region() + entry.offset;
    }
    return nullptr;
}

MAYAFLUX_API void live_arena_dump() noexcept
{
    auto* hdr = internal::live_arena_header();
    for (uint32_t i = 0; i < hdr->entry_count; ++i) {
        auto& entry = hdr->entries[i];
        fprintf(stderr, "[LiveArena] %u: key=\"%s\" occupied=%d offset=%u\n",
            i, entry.key, (int)entry.occupied, entry.offset);
    }
    fprintf(stderr, "[LiveArena] entry_count=%u bump=%u\n",
        hdr->entry_count, internal::g_live_arena_bump);
}

} // namespace MayaFlux
