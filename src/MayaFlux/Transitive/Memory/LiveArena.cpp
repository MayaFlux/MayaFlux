#include "MayaFlux/Transitive/Memory/LiveArena.hpp"

namespace MayaFlux::internal {

alignas(std::max_align_t) unsigned char g_live_arena_storage[LIVE_ARENA_CAPACITY] {};
uint32_t g_live_arena_bump {};
std::shared_ptr<void> g_live_arena_shared_ptrs[LIVE_ARENA_MAX_ENTRIES] {};

} // namespace MayaFlux::internal
