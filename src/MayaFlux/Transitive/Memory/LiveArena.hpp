#pragma once

namespace MayaFlux {

namespace internal {

    /**
     * @brief Maximum number of objects that can be registered in the live arena.
     */
    inline constexpr std::size_t LIVE_ARENA_MAX_ENTRIES = 256;

    /**
     * @brief Maximum length of a live arena key, including null terminator.
     */
    inline constexpr std::size_t LIVE_ARENA_KEY_MAX = 64;

    /**
     * @brief Default capacity of the live arena in bytes.
     */
    inline constexpr std::size_t LIVE_ARENA_CAPACITY = static_cast<const std::size_t>(16 * 1024 * 1024);

    // =========================================================================

    /**
     * @brief Single directory entry mapping a string key to a location within the arena.
     *
     * offset == UINT32_MAX indicates an externally owned object whose pointer
     * is stored in g_live_arena_shared_ptrs at the same index.
     */
    struct LiveArenaEntry {
        char key[LIVE_ARENA_KEY_MAX];
        uint32_t offset;
        uint32_t size;
        bool occupied;
    };

    /**
     * @brief POD header block placed at the start of the arena storage.
     *
     * The directory array lives here. All offsets in LiveArenaEntry are
     * relative to the first byte of object storage that follows this header.
     */
    struct LiveArenaHeader {
        uint32_t entry_count;
        LiveArenaEntry entries[LIVE_ARENA_MAX_ENTRIES];
    };

    // =========================================================================

    /**
     * @brief Raw backing storage for the live arena with external linkage.
     *
     * Exported so the JIT can locate it via DynamicLibrarySearchGenerator.
     * The first sizeof(LiveArenaHeader) bytes are the directory.
     * Object storage begins immediately after.
     */
    alignas(std::max_align_t) extern MAYAFLUX_API unsigned char g_live_arena_storage[LIVE_ARENA_CAPACITY];

    /**
     * @brief Byte offset of the bump pointer from the start of the object region.
     */
    extern MAYAFLUX_API uint32_t g_live_arena_bump;

    /**
     * @brief Shared ownership table for objects registered via expose_live.
     *
     * Stores shared_ptr<void> so live_cast can reconstruct a typed shared_ptr<T>
     * via static_pointer_cast. Null for bump-allocated entries.
     */
    extern MAYAFLUX_API std::shared_ptr<void> g_live_arena_shared_ptrs[LIVE_ARENA_MAX_ENTRIES];

    // =========================================================================

    /**
     * @brief Returns a pointer to the header block at the start of arena storage.
     */
    inline LiveArenaHeader* live_arena_header() noexcept
    {
        return reinterpret_cast<LiveArenaHeader*>(g_live_arena_storage);
    }

    /**
     * @brief Returns a pointer to the object region (immediately after the header).
     */
    inline unsigned char* live_arena_object_region() noexcept
    {
        return g_live_arena_storage + sizeof(LiveArenaHeader);
    }

    /**
     * @brief Aligns @p value up to the next multiple of @p align.
     */
    inline uint32_t align_up(uint32_t value, uint32_t align) noexcept
    {
        return (value + align - 1U) & ~(align - 1U);
    }

    /**
     * @brief Locates an existing directory entry by key.
     * @return Pointer to the matching entry, or nullptr if not found.
     */
    inline LiveArenaEntry* live_arena_find(const char* key) noexcept
    {
        auto* hdr = live_arena_header();
        for (uint32_t i = 0; i < hdr->entry_count; ++i) {
            if (hdr->entries[i].occupied
                && std::strncmp(hdr->entries[i].key, key, LIVE_ARENA_KEY_MAX) == 0) {
                return &hdr->entries[i];
            }
        }
        return nullptr;
    }

    /**
     * @brief Allocates @p size bytes from the arena with @p alignment.
     *
     * Writes a directory entry under @p key. Returns nullptr if the arena
     * is full, the key is already registered, or capacity is exceeded.
     */
    inline void* live_arena_alloc(const char* key, uint32_t size, uint32_t alignment) noexcept
    {
        auto* hdr = live_arena_header();

        if (hdr->entry_count >= LIVE_ARENA_MAX_ENTRIES) {
            return nullptr;
        }

        if (live_arena_find(key)) {
            return nullptr;
        }

        const auto object_region_capacity = static_cast<uint32_t>(LIVE_ARENA_CAPACITY - sizeof(LiveArenaHeader));

        const uint32_t aligned_bump = align_up(g_live_arena_bump, alignment);

        if (aligned_bump + size > object_region_capacity) {
            return nullptr;
        }

        const uint32_t idx = hdr->entry_count;
        auto& entry = hdr->entries[idx];
        std::strncpy(entry.key, key, LIVE_ARENA_KEY_MAX - 1);
        entry.key[LIVE_ARENA_KEY_MAX - 1] = '\0';
        entry.offset = aligned_bump;
        entry.size = size;
        entry.occupied = true;

        g_live_arena_bump = aligned_bump + size;
        ++hdr->entry_count;

        return live_arena_object_region() + aligned_bump;
    }

    /**
     * @brief Registers an externally owned shared_ptr under @p key.
     *
     * Stores shared_ptr<void> in g_live_arena_shared_ptrs at the entry index.
     * offset is set to UINT32_MAX to distinguish from bump-allocated entries.
     *
     * @return true on success, false if the key is already registered or
     *         the directory is full.
     */
    inline bool live_arena_expose(const char* key, std::shared_ptr<void> shared) noexcept
    {
        auto* hdr = live_arena_header();

        if (hdr->entry_count >= LIVE_ARENA_MAX_ENTRIES) {
            return false;
        }

        if (live_arena_find(key)) {
            return false;
        }

        const uint32_t idx = hdr->entry_count;
        auto& entry = hdr->entries[idx];
        std::strncpy(entry.key, key, LIVE_ARENA_KEY_MAX - 1);
        entry.key[LIVE_ARENA_KEY_MAX - 1] = '\0';
        entry.offset = UINT32_MAX;
        entry.size = UINT32_MAX;
        entry.occupied = true;

        g_live_arena_shared_ptrs[idx] = std::move(shared);

        ++hdr->entry_count;
        return true;
    }

    /**
     * @brief Extracts the unqualified type name from a compiler-generated function signature.
     *
     * Parses __PRETTY_FUNCTION__ to extract the final component of a fully qualified
     * type name. Used by MF_LIVE_EXPOSE_AUTO to derive stable per-type key prefixes
     * without requiring the caller to supply a string literal.
     *
     * @tparam T Type to name.
     * @return string_view into static storage valid for the process lifetime.
     */
    template <typename T>
    constexpr std::string_view live_type_name() noexcept
    {
        std::string_view sv = __PRETTY_FUNCTION__;
        auto eq = sv.find("T = ");
        auto end = sv.find(';', eq);
        if (end == std::string_view::npos) {
            end = sv.rfind(']');
        }
        if (eq == std::string_view::npos || end == std::string_view::npos) {
            return sv;
        }
        sv = sv.substr(eq + 4, end - eq - 4);
        auto colon = sv.rfind(':');
        if (colon == std::string_view::npos) {
            return sv;
        }
        return sv.substr(colon + 1);
    }

    /**
     * @brief Formats a live arena key as "TypeName_N" into @p buf.
     *
     * @param buf   Destination buffer of at least LIVE_ARENA_KEY_MAX bytes.
     * @param name  Unqualified type name, typically from live_type_name<T>().
     * @param count Per-type monotonic counter value.
     */
    inline void live_format_key(char* buf, std::string_view name, uint32_t count) noexcept
    {
        std::snprintf(buf, LIVE_ARENA_KEY_MAX, "%.*s_%u",
            static_cast<int>(name.size()), name.data(), count);
    }

} // namespace MayaFlux::internal

/**
 * @brief Exposes @p ptr to the live arena under a user-supplied key when MAYAFLUX_LIVE is defined.
 *
 * Expands to a no-op when MAYAFLUX_LIVE is not defined.
 */
#ifdef MAYAFLUX_LIVE
#define MF_LIVE_EXPOSE(key, ptr) ::MayaFlux::expose_live((key), (ptr))
#else
#define MF_LIVE_EXPOSE(key, ptr) ((void)0)
#endif

/**
 * @brief Auto-expose variant that deduces the key prefix from the shared_ptr element type.
 *
 * Generates keys of the form "TypeName_N" where N is a per-type monotonic counter.
 * The static counter is local to each template instantiation so each type maintains
 * its own independent sequence: Sine_0, Sine_1, Phasor_0, Phasor_1, etc.
 *
 * Use when no explicit name is available at the call site (e.g. read_* methods).
 *
 * @param ptr shared_ptr whose element_type drives both the key prefix and the counter.
 */
#ifdef MAYAFLUX_LIVE
#define MF_LIVE_EXPOSE_AUTO(ptr)                                \
    do {                                                        \
        using _MfT = typename decltype(ptr)::element_type;      \
        static std::atomic<uint32_t> s_live_counter { 0 };      \
        char _mf_key[::MayaFlux::internal::LIVE_ARENA_KEY_MAX]; \
        ::MayaFlux::internal::live_format_key(                  \
            _mf_key,                                            \
            ::MayaFlux::internal::live_type_name<_MfT>(),       \
            s_live_counter.fetch_add(1));                       \
        ::MayaFlux::expose_live(_mf_key, (ptr));                \
    } while (0)
#else
#define MF_LIVE_EXPOSE_AUTO(ptr) ((void)0)
#endif

/**
 * @brief Auto-expose variant with an explicit name prefix and per-name counter.
 *
 * Generates keys of the form "name_N" where N is a monotonic counter local to
 * the call site's template instantiation. Use inside Creator macro expansions
 * where the method name is available via the stringified macro parameter.
 *
 * @param name String literal key prefix, typically #method_name.
 * @param ptr  shared_ptr to expose.
 */
#ifdef MAYAFLUX_LIVE
#define MF_LIVE_EXPOSE_NAMED(name, ptr)                         \
    do {                                                        \
        static std::atomic<uint32_t> s_live_counter { 0 };      \
        char _mf_key[::MayaFlux::internal::LIVE_ARENA_KEY_MAX]; \
        ::MayaFlux::internal::live_format_key(                  \
            _mf_key,                                            \
            std::string_view { (name) },                        \
            s_live_counter.fetch_add(1));                       \
        ::MayaFlux::expose_live(_mf_key, (ptr));                \
    } while (0)
#else
#define MF_LIVE_EXPOSE_NAMED(name, ptr) ((void)0)
#endif
// =============================================================================

/**
 * @brief Constructs a T in-place inside the live arena and registers it under @p key.
 *
 * The returned shared_ptr is stable for the process lifetime and participates
 * in normal shared_ptr ref-counting. The arena owns the underlying storage;
 * the shared_ptr uses a no-op deleter so the arena remains the sole allocator.
 *
 * Reachable from JIT'd code immediately via live_cast<T>(key).
 *
 * @tparam T    Type to construct.
 * @param  key  Null-terminated string used to locate the object from the JIT.
 *              Must be unique within the arena.
 * @param  args Arguments forwarded to T's constructor.
 * @return shared_ptr to the constructed object, or nullptr if the arena is
 *         full or @p key is already registered.
 */
template <typename T, typename... Args>
std::shared_ptr<T> make_live(const char* key, Args&&... args)
{
    void* mem = internal::live_arena_alloc(
        key,
        static_cast<uint32_t>(sizeof(T)),
        static_cast<uint32_t>(alignof(T)));

    if (!mem) {
        return nullptr;
    }

    T* obj = ::new (mem) T(std::forward<Args>(args)...);
    return std::shared_ptr<T>(obj, [](T*) { });
}

/**
 * @brief Exposes an existing shared_ptr-managed object to the live arena under @p key.
 *
 * The shared_ptr ref-count is incremented; the object remains alive at least
 * as long as the arena entry exists. Reachable from JIT'd code via live_cast<T>(key).
 *
 * @tparam T   Type of the object.
 * @param  key Null-terminated string used to locate the object from the JIT.
 * @param  obj shared_ptr to the existing object.
 * @return true on success, false if @p key is already registered or the
 *         directory is full.
 */
template <typename T>
bool expose_live(const char* key, const std::shared_ptr<T>& obj) noexcept
{
    return internal::live_arena_expose(key, std::static_pointer_cast<void>(obj));
}

/**
 * @brief Exposes an existing raw pointer to the live arena under @p key.
 *
 * Lifetime management remains entirely the caller's responsibility.
 * The arena stores a no-op shared_ptr so live_cast returns a valid
 * shared_ptr without affecting the object's actual lifetime.
 *
 * @tparam T   Type of the object.
 * @param  key Null-terminated string used to locate the object from the JIT.
 * @param  ptr Raw pointer to the existing object.
 * @return true on success, false if @p key is already registered or the
 *         directory is full.
 */
template <typename T>
bool expose_live(const char* key, T* ptr) noexcept
{
    return internal::live_arena_expose(key, std::shared_ptr<void>(ptr, [](void*) { }));
}

/**
 * @brief Retrieves a live-arena object by key as a shared_ptr<T>.
 *
 * Callable from both host code and JIT'd code. For shared_ptr-exposed objects
 * the returned shared_ptr shares ownership with the original. For bump-allocated
 * and raw-pointer objects the returned shared_ptr uses a no-op deleter.
 *
 * @tparam T   Expected type of the registered object.
 * @param  key Null-terminated string used at make_live or expose_live time.
 * @return shared_ptr<T>, or nullptr if the key is not found.
 */
template <typename T>
std::shared_ptr<T> live_cast(const char* key) noexcept
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
            return std::static_pointer_cast<T>(internal::g_live_arena_shared_ptrs[i]);
        }
        T* ptr = reinterpret_cast<T*>(internal::live_arena_object_region() + entry.offset);
        return std::shared_ptr<T>(ptr, [](T*) { });
    }
    return nullptr;
}

} // namespace MayaFlux
