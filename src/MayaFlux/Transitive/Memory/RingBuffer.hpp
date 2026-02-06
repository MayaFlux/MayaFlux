#pragma once

namespace MayaFlux::Memory {

/**
 * @concept TriviallyCopyable
 * @brief Constraint for types that can be safely copied bitwise
 *
 * Required for lock-free ring buffers to ensure atomic operations
 * don't invoke copy constructors during concurrent access.
 */
template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

/**
 * @struct FixedStorage
 * @brief Compile-time fixed-capacity storage using std::array
 *
 * Provides zero-overhead compile-time buffer allocation with capacity
 * known at compile time. Required for lock-free contexts and real-time
 * audio processing where dynamic allocation is forbidden.
 *
 * Capacity must be a power of 2 for efficient modulo operations using
 * bitwise AND instead of division.
 *
 * @tparam T Element type
 * @tparam Capacity Buffer size (must be power of 2)
 *
 * **Memory Layout:**
 * - Stack-allocated std::array<T, Capacity>
 * - Zero initialization overhead
 * - Cache-friendly contiguous memory
 *
 * **Use Cases:**
 * - Lock-free queues for input events
 * - Real-time logging buffers
 * - Fixed-size delay lines
 * - MIDI message queues
 *
 * @code{.cpp}
 * // 4096-element lock-free queue for input events
 * using InputQueue = RingBuffer<InputValue,
 *                               FixedStorage<InputValue, 4096>,
 *                               LockFreePolicy,
 *                               QueueAccess>;
 * @endcode
 */
template <typename T, size_t Capacity>
struct FixedStorage {
    static_assert((Capacity & (Capacity - 1)) == 0,
        "FixedStorage capacity must be power of 2 for efficient modulo. "
        "Use 64, 128, 256, 512, 1024, 2048, 4096, 8192, etc.");

    using storage_type = std::array<T, Capacity>;
    static constexpr size_t capacity_value = Capacity;
    static constexpr bool is_resizable = false;

    storage_type buffer {};

    [[nodiscard]] constexpr size_t capacity() const noexcept { return Capacity; }
};

/**
 * @struct DynamicStorage
 * @brief Runtime-resizable storage using std::vector
 *
 * Provides dynamic capacity management for non-realtime contexts where
 * buffer size cannot be determined at compile time. Supports resize()
 * operations but may trigger heap allocations.
 *
 * @tparam T Element type
 *
 * **Memory Layout:**
 * - Heap-allocated std::vector<T>
 * - Resizable via resize() method
 * - May reallocate on capacity change
 *
 * **Use Cases:**
 * - Variable-length delay lines in non-realtime contexts
 * - Recording buffers with unknown duration
 * - Feedback processors with modulated delay times
 * - Non-audio processing chains
 */
template <typename T>
struct DynamicStorage {
    using storage_type = std::vector<T>;
    static constexpr bool is_resizable = true;

    storage_type buffer;

    explicit DynamicStorage(size_t initial_capacity = 64)
        : buffer(initial_capacity)
    {
    }

    [[nodiscard]] size_t capacity() const noexcept { return buffer.size(); }

    void resize(size_t new_capacity) { buffer.resize(new_capacity); }
};

/**
 * @struct LockFreePolicy
 * @brief Lock-free SPSC (Single Producer Single Consumer) concurrency
 *
 * Implements wait-free push and lock-free pop using C++20 atomic operations
 * with careful memory ordering. Designed for real-time producer threads
 * writing to non-realtime consumer threads.
 *
 * **Thread Safety:**
 * - ONE writer thread (producer)
 * - ONE reader thread (consumer)
 * - NO mutex locks (real-time safe)
 * - Cache-line aligned atomics prevent false sharing
 *
 * **Memory Ordering:**
 * - relaxed: Initial atomic read (no synchronization needed)
 * - acquire: Read from other thread (synchronizes-with release)
 * - release: Write for other thread (synchronizes-with acquire)
 *
 * **SPSC Safety with Non-Trivial Types:**
 * Unlike MPMC (multi-producer multi-consumer), SPSC queues are safe for
 * non-trivially-copyable types because:
 * - Each slot written by ONLY ONE thread (producer)
 * - Each slot read by ONLY ONE thread (consumer)
 * - No concurrent access to same slot (guaranteed by SPSC protocol)
 * - Assignment `buffer[i] = value` is sequentially consistent per slot
 *
 * This means InputValue with std::string/std::vector is SAFE here.
 * The atomic indices synchronize which slots are readable/writable,
 * but the actual data copy happens in exclusive ownership.
 *
 * **Requirements:**
 * - Storage MUST be FixedStorage (compile-time capacity)
 * - Capacity MUST be power of 2
 * - SPSC only (single producer, single consumer)
 *
 * **Performance:**
 * - Wait-free push: O(1) without blocking
 * - Lock-free pop: O(1) without mutex
 * - ~10-20 CPU cycles per operation
 *
 * @code{.cpp}
 * // Works with complex types (SPSC safe)
 * LockFreeQueue<InputValue, 4096> input_queue;  // Contains strings/vectors
 *
 * // Producer thread
 * void enqueue_event(const InputValue& event) {
 *     input_queue.push(event);  // Safe: exclusive write to slot
 * }
 *
 * // Consumer thread
 * void process_events() {
 *     while (auto event = input_queue.pop()) {
 *         handle(*event);  // Safe: exclusive read from slot
 *     }
 * }
 * @endcode
 */
struct LockFreePolicy {
    static constexpr bool is_thread_safe = true;
    static constexpr bool requires_trivial_copyable = false;
    static constexpr bool requires_fixed_storage = true;

    template <typename T, typename Storage>
    struct State {
        static_assert(!Storage::is_resizable,
            "LockFreePolicy requires FixedStorage<T, N>. "
            "For dynamic capacity, use SingleThreadedPolicy with DynamicStorage.");

        alignas(64) std::atomic<size_t> write_index { 0 };
        alignas(64) std::atomic<size_t> read_index { 0 };

        static constexpr size_t increment(size_t index, size_t capacity) noexcept
        {
            return (index + 1) & (capacity - 1);
        }
    };
};

/**
 * @struct SingleThreadedPolicy
 * @brief Non-atomic single-threaded operation
 *
 * Provides minimal overhead when all access is from a single thread or
 * when synchronization is handled externally (e.g., mutex-protected).
 *
 * **Thread Safety:**
 * - NO thread synchronization
 * - Use when externally synchronized OR single-threaded
 *
 * **Benefits:**
 * - No atomic overhead (~2-5 CPU cycles faster per operation)
 * - Works with any element type (no TriviallyCopyable requirement)
 * - Compatible with both FixedStorage and DynamicStorage
 *
 * **When to Use:**
 * - DSP delay lines in audio callback (single thread)
 * - Polynomial node history buffers (single thread)
 * - Non-realtime data structures
 * - Already protected by external mutex
 *
 * @code{.cpp}
 * // Single-threaded delay line for audio DSP
 * using AudioDelay = RingBuffer<double,
 *                               DynamicStorage<double>,
 *                               SingleThreadedPolicy,
 *                               HomogeneousAccess>;
 *
 * void audio_process_callback() {
 *     AudioDelay delay(1000);  // No thread sync overhead
 *     delay.push(input_sample);
 *     auto delayed = delay.peek_oldest();
 * }
 * @endcode
 */
struct SingleThreadedPolicy {
    static constexpr bool is_thread_safe = false;
    static constexpr bool requires_trivial_copyable = false;
    static constexpr bool requires_fixed_storage = false;

    template <typename T, typename Storage>
    struct State {
        size_t write_index { 0 };
        size_t read_index { 0 };

        static constexpr size_t increment(size_t index, size_t capacity) noexcept
        {
            return (index + 1) % capacity;
        }
    };
};

/**
 * @struct QueueAccess
 * @brief FIFO queue semantics (oldest data first)
 *
 * Traditional queue behavior: enqueue at back, dequeue from front.
 * Data ordering: [0] = oldest inserted, [N-1] = newest inserted.
 *
 * **Operations:**
 * - push(): Add to back of queue
 * - pop(): Remove from front of queue
 * - linearized_view(): [oldest → newest]
 *
 * **Use Cases:**
 * - Event queues (input, MIDI, OSC)
 * - Message passing between threads
 * - Task queues
 * - Logging buffers
 *
 * @code{.cpp}
 * LockFreeQueue<InputEvent, 2048> event_queue;
 *
 * // Producer: add events as they arrive
 * event_queue.push(event);
 *
 * // Consumer: process in arrival order
 * while (auto event = event_queue.pop()) {
 *     process(*event);  // Oldest event first
 * }
 * @endcode
 */
struct QueueAccess {
    static constexpr bool push_front = false;
    static constexpr bool pop_front = true;
    static constexpr const char* name = "Queue (FIFO)";
};

/**
 * @struct HistoryBufferAccess
 * @brief History buffer semantics (newest sample first)
 *
 * Maintains temporal ordering where index 0 represents the most recent
 * sample and higher indices represent progressively older samples.
 * This is the natural indexing for difference equations and recursive
 * relations: y[n], y[n-1], y[n-2], ...
 *
 * **Operations:**
 * - push(): Add to front (becomes index 0)
 * - operator[]: Direct indexing where [0] = newest, [k] = k samples ago
 * - linearized_view(): Returns mutable span [newest → oldest]
 *
 * **Mathematical Context:**
 * - Difference equations: y[n] = a·y[n-1] + b·y[n-2]
 * - FIR filters: y[n] = Σ h[k]·x[n-k]
 * - IIR filters: y[n] = Σ b[k]·x[n-k] - Σ a[k]·y[n-k]
 * - Recurrence relations: any recursive numerical method
 *
 * @code{.cpp}
 * HistoryBuffer<double> history(100);
 *
 * // Difference equation: y[n] = 0.5·y[n-1] + x[n]
 * history.push(input_sample);       // New sample at [0]
 * double current = history[0];      // Current input
 * double previous = history[1];     // One sample ago
 * double output = 0.5 * previous + current;
 *
 * // Access via linearized view for convolution
 * auto view = history.linearized_view();  // [newest → oldest]
 * for (size_t k = 0; k < view.size(); ++k) {
 *     output += view[k] * coefficients[k];
 * }
 * @endcode
 */
struct HistoryBufferAccess {
    static constexpr bool push_front = true;
    static constexpr bool pop_front = false;
    static constexpr const char* name = "HistoryBuffer (newest-first)";
};

/**
 * @brief History buffer for difference equations and recursive relations
 *
 * Specialized ring buffer for maintaining temporal history in mathematical
 * computations. Pre-initialized to full capacity with zeros to match
 * standard mathematical notation where y[n-k] is defined for all k < N
 * from the start (initial conditions = 0).
 *
 * **Key Differences from Generic RingBuffer:**
 * - Pre-filled to capacity on construction (all zeros)
 * - Always returns full-size mutable spans (size = capacity)
 * - Direct mutable element access via operator[]
 * - Matches mathematical notation: y[0] = newest, y[k] = k steps back
 *
 * @tparam T Element type
 *
 * @code{.cpp}
 * // IIR filter: y[n] = 0.8·y[n-1] - 0.5·y[n-2] + x[n]
 * HistoryBuffer<double> y_history(2);  // Pre-filled with [0.0, 0.0]
 *
 * for (auto x : input_signal) {
 *     y_history.push(x);  // Current input becomes y[0]
 *
 *     double y_n = y_history[0];     // y[n] (just pushed)
 *     double y_n1 = y_history[1];    // y[n-1]
 *     double y_n2 = y_history[2];    // y[n-2]
 *
 *     double output = 0.8*y_n1 - 0.5*y_n2 + y_n;
 *     y_history.overwrite_newest(output);  // Replace y[n] with computed output
 * }
 * @endcode
 */
template <typename T>
class HistoryBuffer {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;

    /**
     * @brief Construct history buffer with specified capacity
     * @param capacity Maximum number of historical samples to maintain
     *
     * **Critical:** Buffer is immediately filled to capacity with default
     * values (zero for numeric types). This matches mathematical convention
     * where initial conditions are typically zero.
     */
    explicit HistoryBuffer(size_t capacity)
        : m_capacity(capacity)
        , m_data(capacity, T {})
        , m_linear_view(capacity)
        , m_count(capacity)
    {
    }

    /**
     * @brief Push new value to front of history
     * @param value New sample value
     *
     * Inserts value at index 0, shifting all previous values back.
     * Oldest value (at index capacity-1) is discarded.
     */
    void push(const T& value)
    {
        if (m_capacity == 0)
            return;

        m_head = (m_head == 0) ? m_capacity - 1 : m_head - 1;
        m_data[m_head] = value;

        if (m_count < m_capacity) {
            ++m_count;
        }
    }

    /**
     * @brief Access element by temporal offset
     * @param index Temporal offset (0 = newest, k = k samples ago)
     * @return Mutable reference to element
     *
     * Provides direct access to history: [0] = most recent, [1] = one step back, etc.
     * Matches mathematical notation y[n-k] where index k represents temporal offset.
     */
    reference operator[](size_t index)
    {
        return m_data[(m_head + index) % m_capacity];
    }

    const_reference operator[](size_t index) const
    {
        return m_data[(m_head + index) % m_capacity];
    }

    /**
     * @brief Get newest element (same as [0])
     * @return Reference to most recent sample
     */
    reference newest()
    {
        return m_data[m_head];
    }

    const_reference newest() const
    {
        return m_data[m_head];
    }

    /**
     * @brief Get oldest element (same as [capacity-1])
     * @return Reference to oldest sample in buffer
     */
    reference oldest()
    {
        size_t oldest_idx = (m_head + m_count - 1) % m_capacity;
        return m_data[oldest_idx];
    }

    const_reference oldest() const
    {
        size_t oldest_idx = (m_head + m_count - 1) % m_capacity;
        return m_data[oldest_idx];
    }

    /**
     * @brief Overwrite the newest element without advancing position
     * @param value New value for current sample
     *
     * Critical for recursive algorithms where you push input, compute output,
     * then replace the pushed value with the computed result.
     */
    void overwrite_newest(const T& value)
    {
        m_data[m_head] = value;
    }

    /**
     * @brief Get mutable linearized view of entire history
     * @return Mutable span ordered [newest → oldest], size = capacity
     *
     * **Always returns full-size span** (size = capacity), even if fewer
     * elements have been pushed. This matches mathematical convention where
     * y[n-k] is defined for all k < N with initial conditions = 0.
     */
    std::span<T> linearized_view()
    {
        for (size_t i = 0; i < m_count; ++i) {
            m_linear_view[i] = m_data[(m_head + i) % m_capacity];
        }
        return { m_linear_view.data(), m_count };
    }

    /**
     * @brief Get const linearized view
     */
    std::span<const T> linearized_view() const
    {
        for (size_t i = 0; i < m_count; ++i) {
            m_linear_view[i] = m_data[(m_head + i) % m_capacity];
        }
        return { m_linear_view.data(), m_count };
    }

    /**
     * @brief Update element at specific index
     * @param index Temporal offset (0 = newest, k = k samples ago)
     * @param value New value
     */
    void update(size_t index, const T& value)
    {
        if (index >= m_count) {
            return;
        }
        m_data[(m_head + index) % m_capacity] = value;
    }

    /**
     * @brief Reset buffer to initial state (all zeros)
     *
     * Fills buffer with default values and resets to full capacity.
     * Matches behavior of mathematical systems with zero initial conditions.
     */
    void reset()
    {
        std::ranges::fill(m_data, T {});
        m_head = 0;
        m_count = m_capacity;
    }

    /**
     * @brief Set initial conditions
     * @param values Initial values (ordered newest to oldest)
     *
     * Sets the first min(values.size(), capacity) elements to the given values,
     * fills remainder with zeros. Sets count to full capacity.
     */
    void set_initial_conditions(const std::vector<T>& values)
    {
        std::ranges::fill(m_data, T {});

        size_t count = std::min(values.size(), m_capacity);
        for (size_t i = 0; i < count; ++i) {
            m_data[i] = values[i];
        }

        m_head = 0;
        m_count = m_capacity;
    }

    /**
     * @brief Resize buffer capacity
     * @param new_capacity New maximum number of samples
     *
     * Preserves existing data in temporal order. If growing, new slots
     * filled with zeros. If shrinking, oldest data discarded.
     * Count always equals capacity after resize.
     */
    void resize(size_t new_capacity)
    {
        if (new_capacity == m_capacity)
            return;

        std::vector<T> current_data;
        current_data.reserve(m_count);
        for (size_t i = 0; i < m_count; ++i) {
            current_data.push_back(m_data[(m_head + i) % m_capacity]);
        }

        m_capacity = new_capacity;
        m_data.resize(new_capacity, T {});
        m_linear_view.resize(new_capacity);

        size_t to_copy = std::min(current_data.size(), new_capacity);
        for (size_t i = 0; i < to_copy; ++i) {
            m_data[i] = current_data[i];
        }

        m_head = 0;
        m_count = m_capacity;
    }

    /**
     * @brief Get buffer capacity
     * @return Maximum number of samples buffer can hold
     */
    [[nodiscard]] size_t capacity() const { return m_capacity; }

    /**
     * @brief Get current count (always equals capacity for HistoryBuffer)
     * @return Number of valid elements (= capacity)
     */
    [[nodiscard]] size_t size() const { return m_count; }

    /**
     * @brief Check if buffer is empty (always false for HistoryBuffer)
     */
    [[nodiscard]] bool empty() const { return false; }

    /**
     * @brief Save current state for later restoration
     * @return Vector containing current data in temporal order
     */
    [[nodiscard]] std::vector<T> save_state() const
    {
        std::vector<T> state;
        state.reserve(m_count);
        for (size_t i = 0; i < m_count; ++i) {
            state.push_back(m_data[(m_head + i) % m_capacity]);
        }
        return state;
    }

    /**
     * @brief Restore previously saved state
     * @param state State vector from save_state()
     */
    void restore_state(const std::vector<T>& state)
    {
        std::ranges::fill(m_data, T {});

        size_t count = std::min(state.size(), m_capacity);
        for (size_t i = 0; i < count; ++i) {
            m_data[i] = state[i];
        }

        m_head = 0;
        m_count = m_capacity;
    }

private:
    size_t m_capacity;
    std::vector<T> m_data;
    mutable std::vector<T> m_linear_view;
    size_t m_head {};
    size_t m_count;
};

/**
 * @class RingBuffer
 * @brief Policy-driven unified circular buffer implementation
 *
 * Provides a single, flexible ring buffer that adapts to different use cases
 * through compile-time policy selection. Policies control storage allocation,
 * thread safety, and access patterns independently.
 *
 * **Design Philosophy:**
 * - Zero runtime cost: All policy dispatch via templates (no virtual calls)
 * - Type-safe: Policy constraints enforced at compile time
 * - Composable: Policies combine orthogonally
 * - Minimal: No features you don't need based on policy selection
 *
 * **Policy Dimensions:**
 * 1. **Storage**: FixedStorage (compile-time) vs DynamicStorage (runtime)
 * 2. **Concurrency**: LockFreePolicy (SPSC) vs SingleThreadedPolicy (no sync)
 * 3. **Access**: QueueAccess (FIFO) vs HistoryBufferAccess (newest-first)
 *
 * **Common Configurations:**
 *
 * @code{.cpp}
 * // Lock-free input event queue (realtime → worker thread)
 * using InputQueue = RingBuffer<InputValue,
 *                               FixedStorage<InputValue, 4096>,
 *                               LockFreePolicy,
 *                               QueueAccess>;
 *
 * // Single-threaded audio delay line (DSP processing)
 * using AudioDelay = RingBuffer<double,
 *                               DynamicStorage<double>,
 *                               SingleThreadedPolicy,
 *                               HistoryBufferAccess>;
 *
 * // Lock-free logging buffer (audio thread → disk writer)
 * using LogBuffer = RingBuffer<RealtimeEntry,
 *                              FixedStorage<RealtimeEntry, 8192>,
 *                              LockFreePolicy,
 *                              QueueAccess>;
 *
 * // Polynomial node history buffer (single-threaded DSP)
 * using NodeHistory = RingBuffer<double,
 *                                FixedStorage<double, 64>,
 *                                SingleThreadedPolicy,
 *                                HistoryBufferAccess>;
 * @endcode
 *
 * **Migration from Legacy Implementations:**
 *
 * @tparam T Element type
 * @tparam StoragePolicy FixedStorage<T,N> or DynamicStorage<T>
 * @tparam ConcurrencyPolicy LockFreePolicy or SingleThreadedPolicy
 * @tparam AccessPattern QueueAccess or HistoryBufferAccess
 */
template <
    typename T,
    typename StoragePolicy,
    typename ConcurrencyPolicy = SingleThreadedPolicy,
    typename AccessPattern = QueueAccess>
class RingBuffer {

    static_assert(!ConcurrencyPolicy::requires_fixed_storage || !StoragePolicy::is_resizable,
        "Selected ConcurrencyPolicy requires FixedStorage<T, N>. "
        "Either: (1) Use SingleThreadedPolicy, or (2) Use FixedStorage.");

    using State = typename ConcurrencyPolicy::template State<T, StoragePolicy>;

public:
    using value_type = T;
    using storage_type = StoragePolicy;
    using reference = T&;
    using const_reference = const T&;

    static constexpr bool is_lock_free = ConcurrencyPolicy::is_thread_safe;
    static constexpr bool is_resizable = StoragePolicy::is_resizable;
    static constexpr bool is_delay_line = AccessPattern::push_front;

    /**
     * @brief Construct ring buffer with runtime capacity (DynamicStorage only)
     * @param initial_capacity Initial buffer size in elements
     */
    explicit RingBuffer(size_t initial_capacity = 64)
        requires(is_resizable)
        : m_storage(initial_capacity)
        , m_linearized(initial_capacity)
    {
    }

    /**
     * @brief Default construct ring buffer (FixedStorage only)
     * Capacity determined by template parameter.
     */
    RingBuffer()
        requires(!is_resizable)
    = default;

    /**
     * @brief Push element into buffer
     * @param value Element to insert
     * @return true if successful, false if buffer full
     *
     * **Behavior by AccessPattern:**
     * - QueueAccess: Adds to back of queue (oldest at front)
     * - HistoryBufferAccess: Adds to front (becomes newest element)
     *
     * **Thread Safety:**
     * - LockFreePolicy: Wait-free for single producer
     * - SingleThreadedPolicy: Not thread-safe
     *
     * @code{.cpp}
     * RingBuffer<double, ...> buffer;
     * if (!buffer.push(42.0)) {
     *     // Buffer full, oldest data still present
     * }
     * @endcode
     */
    [[nodiscard]] bool push(const T& value) noexcept(is_lock_free)
    {
        if constexpr (is_lock_free) {
            return push_lockfree(value);
        } else {
            return push_singlethread(value);
        }
    }

    /**
     * @brief Pop element from buffer
     * @return Element if available, nullopt if empty
     *
     * **Behavior by AccessPattern:**
     * - QueueAccess: Removes from front (oldest element)
     * - HistoryBufferAccess: Not typically used (use peek_oldest instead)
     *
     * **Thread Safety:**
     * - LockFreePolicy: Lock-free for single consumer
     * - SingleThreadedPolicy: Not thread-safe
     *
     * @code{.cpp}
     * while (auto value = buffer.pop()) {
     *     process(*value);  // Process oldest data first
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<T> pop() noexcept(is_lock_free)
    {
        if constexpr (is_lock_free) {
            return pop_lockfree();
        } else {
            return pop_singlethread();
        }
    }

    /**
     * @brief Peek at newest element without removing
     * @return Reference to newest element
     *
     * Only available for SingleThreadedPolicy (HistoryBufferAccess).
     * Returns element at index 0 in delay line ordering.
     */
    [[nodiscard]] const_reference peek_newest() const
        requires(!is_lock_free && is_delay_line)
    {
        return m_storage.buffer[m_state.write_index];
    }

    /**
     * @brief Peek at oldest element without removing
     * @return Reference to oldest element
     *
     * Only available for SingleThreadedPolicy (HistoryBufferAccess).
     * Returns element at highest valid index in delay line.
     */
    [[nodiscard]] const_reference peek_oldest() const
        requires(!is_lock_free && is_delay_line)
    {
        const size_t count = size();
        if (count == 0) {
            return m_storage.buffer[m_state.write_index];
        }

        const size_t cap = m_storage.capacity();
        size_t oldest_idx = (m_state.write_index + cap - count + 1) % cap;
        return m_storage.buffer[oldest_idx];
    }

    /**
     * @brief Access element by index (delay line style)
     * @param index Distance from newest element (0 = newest)
     * @return Reference to element
     *
     * Only available for SingleThreadedPolicy (HistoryBufferAccess).
     * Provides array-like access where [0] is newest sample.
     */
    [[nodiscard]] const_reference operator[](size_t index) const
        requires(!is_lock_free && is_delay_line)
    {
        const size_t cap = m_storage.capacity();
        size_t actual_idx = (m_state.write_index + cap - index) % cap;
        return m_storage.buffer[actual_idx];
    }

    /**
     * @brief Overwrite newest element without advancing write position
     * @param value New value for newest element
     *
     * Only available for SingleThreadedPolicy (HistoryBufferAccess).
     * Useful for in-place modification of current sample.
     *
     * @code{.cpp}
     * delay.push(input);
     * delay.overwrite_newest(input * 0.5);  // Modify current sample
     * @endcode
     */
    void overwrite_newest(const T& value)
        requires(!is_lock_free && is_delay_line)
    {
        m_storage.buffer[m_state.write_index] = value;
    }

    /**
     * @brief Get linearized view of buffer contents
     * @return Span ordered by AccessPattern
     *
     * Only available for SingleThreadedPolicy.
     * Returns contiguous view of buffer data in logical order:
     * - QueueAccess: [oldest → newest]
     * - HistoryBufferAccess: [newest → oldest]
     *
     * **Not Real-time Safe**: Copies data to linear buffer.
     * Use sparingly in audio processing paths.
     */
    [[nodiscard]] std::span<T> linearized_view() const
        requires(!is_lock_free)
    {
        const size_t cap = m_storage.capacity();
        const size_t count = size();

        if constexpr (AccessPattern::push_front) {
            for (size_t i = 0; i < count; ++i) {
                size_t idx = (m_state.write_index + cap - i) % cap;
                m_linearized[i] = m_storage.buffer[idx];
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                size_t idx = (m_state.read_index + i) % cap;
                m_linearized[i] = m_storage.buffer[idx];
            }
        }

        return { m_linearized.data(), count };
    }

    /**
     * @brief Get mutable linearized view for modification
     * @return Mutable span ordered by AccessPattern
     *
     * Same as linearized_view() but returns mutable references.
     * Changes to span elements affect underlying buffer.
     *
     * @code{.cpp}
     * auto history = delay.linearized_view_mut();
     * for (auto& sample : history) {
     *     sample *= 0.9;  // Apply gain reduction
     * }
     * @endcode
     */
    [[nodiscard]] std::span<T> linearized_view_mut()
        requires(!is_lock_free)
    {
        const size_t cap = m_storage.capacity();
        const size_t count = size();

        if constexpr (AccessPattern::push_front) {
            for (size_t i = 0; i < count; ++i) {
                size_t idx = (m_state.write_index + cap - i) % cap;
                m_linearized[i] = m_storage.buffer[idx];
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                size_t idx = (m_state.read_index + i) % cap;
                m_linearized[i] = m_storage.buffer[idx];
            }
        }

        return { m_linearized.data(), count };
    }

    /**
     * @brief Thread-safe snapshot of buffer contents
     * @return Vector copy ordered by AccessPattern
     *
     * Safe for lock-free contexts but allocates memory.
     * For LockFreePolicy, provides consistent view despite concurrent access.
     *
     * **Not Real-time Safe**: Allocates std::vector.
     *
     * @code{.cpp}
     * // Lock-free queue
     * LockFreeQueue<Event, 1024> queue;
     * auto events = queue.snapshot();  // Safe despite concurrent push
     *
     * for (const auto& event : events) {
     *     write_to_disk(event);  // Can block safely
     * }
     * @endcode
     */
    [[nodiscard]] std::vector<T> snapshot() const
    {
        std::vector<T> result;

        if constexpr (is_lock_free) {
            const size_t cap = m_storage.capacity();
            auto read = m_state.read_index.load(std::memory_order_acquire);
            auto write = m_state.write_index.load(std::memory_order_acquire);

            result.reserve(cap);
            while (read != write) {
                result.push_back(m_storage.buffer[read]);
                read = State::increment(read, cap);
            }
        } else {
            auto view = linearized_view();
            result.assign(view.begin(), view.end());
        }

        return result;
    }

    /**
     * @brief Check if buffer is empty
     * @return true if no elements present
     */
    [[nodiscard]] bool empty() const noexcept
    {
        if constexpr (is_lock_free) {
            return m_state.read_index.load(std::memory_order_acquire) == m_state.write_index.load(std::memory_order_acquire);
        } else {
            return m_state.read_index == m_state.write_index;
        }
    }

    /**
     * @brief Get approximate element count
     * @return Number of elements in buffer
     *
     * For LockFreePolicy, value may be stale due to concurrent modification.
     * Use for debugging/monitoring only, not for critical logic.
     */
    [[nodiscard]] size_t size() const noexcept
    {
        const size_t cap = m_storage.capacity();

        if constexpr (is_lock_free) {
            auto write = m_state.write_index.load(std::memory_order_acquire);
            auto read = m_state.read_index.load(std::memory_order_acquire);
            return (write >= read) ? (write - read) : (cap - read + write);
        } else {
            return (m_state.write_index >= m_state.read_index)
                ? (m_state.write_index - m_state.read_index)
                : (cap - m_state.read_index + m_state.write_index);
        }
    }

    /**
     * @brief Get buffer capacity
     * @return Maximum number of elements buffer can hold
     */
    [[nodiscard]] size_t capacity() const noexcept
    {
        return m_storage.capacity();
    }

    /**
     * @brief Resize buffer capacity (DynamicStorage only)
     * @param new_capacity New maximum element count
     *
     * Preserves existing data ordered by AccessPattern.
     * **Not Real-time Safe**: May trigger heap allocation.
     */
    void resize(size_t new_capacity)
        requires(is_resizable)
    {
        if (new_capacity == m_storage.capacity())
            return;

        auto current_data = snapshot();

        m_storage.resize(new_capacity);
        m_linearized.resize(new_capacity);

        m_state.write_index = 0;
        m_state.read_index = 0;

        size_t to_copy = std::min(current_data.size(), new_capacity);
        for (size_t i = 0; i < to_copy; ++i) {
            m_storage.buffer[i] = current_data[i];
        }

        if constexpr (AccessPattern::push_front) {
            m_state.write_index = (to_copy > 0) ? to_copy - 1 : 0;
        } else {
            m_state.write_index = to_copy;
        }
    }

    /**
     * @brief Clear buffer contents and reset indices
     *
     * **Real-time Safe**: No allocations, just resets atomic indices.
     */
    void reset() noexcept
    {
        if constexpr (is_lock_free) {
            m_state.write_index.store(0, std::memory_order_release);
            m_state.read_index.store(0, std::memory_order_release);
        } else {
            m_state.write_index = 0;
            m_state.read_index = 0;
        }
    }

private:
    [[nodiscard]] bool push_lockfree(const T& value) noexcept
    {
        const size_t cap = m_storage.capacity();
        auto write = m_state.write_index.load(std::memory_order_relaxed);
        auto next_write = State::increment(write, cap);

        if (next_write == m_state.read_index.load(std::memory_order_acquire)) {
            return false;
        }

        m_storage.buffer[write] = value;
        m_state.write_index.store(next_write, std::memory_order_release);

        return true;
    }

    [[nodiscard]] std::optional<T> pop_lockfree() noexcept
    {
        auto read = m_state.read_index.load(std::memory_order_relaxed);

        if (read == m_state.write_index.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T value = m_storage.buffer[read];
        m_state.read_index.store(
            State::increment(read, m_storage.capacity()),
            std::memory_order_release);

        return value;
    }

    [[nodiscard]] bool push_singlethread(const T& value) noexcept
    {
        const size_t cap = m_storage.capacity();
        auto next_write = State::increment(m_state.write_index, cap);

        if (next_write == m_state.read_index) {
            return false;
        }

        if constexpr (AccessPattern::push_front) {
            m_state.write_index = (m_state.write_index == 0)
                ? cap - 1
                : m_state.write_index - 1;
            m_storage.buffer[m_state.write_index] = value;
        } else {
            m_storage.buffer[m_state.write_index] = value;
            m_state.write_index = next_write;
        }

        return true;
    }

    [[nodiscard]] std::optional<T> pop_singlethread() noexcept
    {
        if (m_state.read_index == m_state.write_index) {
            return std::nullopt;
        }

        T value = m_storage.buffer[m_state.read_index];
        m_state.read_index = State::increment(m_state.read_index, m_storage.capacity());

        return value;
    }

    StoragePolicy m_storage;
    State m_state;
    mutable std::vector<T> m_linearized;
};

/**
 * @brief Type alias: Lock-free SPSC queue with fixed capacity
 * @tparam T Element type (must be TriviallyCopyable)
 * @tparam Capacity Buffer size (must be power of 2)
 *
 * Common configuration for real-time producer → non-realtime consumer.
 *
 * **Use Cases:**
 * - Input event queues (MIDI, OSC, HID)
 * - Real-time logging buffers
 * - Audio thread → worker thread communication
 *
 * **Example:**
 * @code{.cpp}
 * // Replace: Memory::LockFreeRingBuffer<InputValue, 4096>
 * LockFreeQueue<InputValue, 4096> input_queue;
 *
 * // Audio thread (realtime)
 * input_queue.push(event);  // Wait-free
 *
 * // Worker thread (non-realtime)
 * while (auto event = input_queue.pop()) {
 *     process_event(*event);
 * }
 * @endcode
 */
template <typename T, size_t Capacity>
using LockFreeQueue = RingBuffer<T,
    FixedStorage<T, Capacity>,
    LockFreePolicy,
    QueueAccess>;

/**
 * @brief Type alias: Single-threaded FIFO queue with fixed capacity
 * @tparam T Element type
 * @tparam Capacity Buffer size (must be power of 2)
 *
 * For non-concurrent queue usage with compile-time capacity.
 */
template <typename T, size_t Capacity>
using FixedQueue = RingBuffer<T,
    FixedStorage<T, Capacity>,
    SingleThreadedPolicy,
    QueueAccess>;

/**
 * @brief Type alias: Resizable FIFO queue (non-concurrent)
 * @tparam T Element type
 *
 * For non-concurrent queue usage with runtime capacity.
 */
template <typename T>
using DynamicQueue = RingBuffer<T,
    DynamicStorage<T>,
    SingleThreadedPolicy,
    QueueAccess>;

} // namespace MayaFlux::Memory
