#pragma once

/**
 * @file KernelSpec.hpp
 * @brief Compile-time separable filter kernel definition and named kernel bank.
 *
 * KernelSpec<N> is a constexpr wrapper around std::array<float, N> with
 * chainable compile-time transforms. All methods return a new KernelSpec
 * leaving the original unchanged.
 *
 * Named constants at the bottom of this file cover the standard operators.
 * Custom kernels can be constructed inline at any call site:
 *
 *   constexpr auto k = KernelSpec<5>{ 1.F, 4.F, 6.F, 4.F, 1.F }.normalize();
 *   filter_separable(src, tmp, dst, w, h, k, k);
 *
 * KernelSpec<N> converts implicitly to std::span<const float> so it can be
 * passed directly to any filter function without an explicit cast.
 */

namespace MayaFlux::Kinesis::Vision {

// ============================================================================
// KernelSpec<N>
// ============================================================================

/**
 * @brief Compile-time 1D separable filter kernel.
 *
 * @tparam N Kernel length. Must be odd and >= 1.
 */
template <size_t N>
struct KernelSpec {
    static_assert(N % 2 == 1, "KernelSpec length must be odd");
    static_assert(N >= 1, "KernelSpec length must be >= 1");

    static constexpr size_t length = N;
    static constexpr size_t half = N / 2;

    std::array<float, N> taps {};

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    constexpr KernelSpec() = default;

    template <typename... Ts>
        requires(sizeof...(Ts) == N) && (std::is_convertible_v<Ts, float> && ...)
    constexpr explicit KernelSpec(Ts... values) noexcept
        : taps { static_cast<float>(values)... }
    {
    }

    // -------------------------------------------------------------------------
    // Transforms — each returns a new KernelSpec, original unchanged
    // -------------------------------------------------------------------------

    /**
     * @brief Divide all taps by their sum.
     *
     * Produces a unit-sum kernel suitable for smoothing passes.
     * No-op if sum is zero.
     */
    [[nodiscard]] constexpr KernelSpec normalize() const noexcept
    {
        float sum = 0.0F;
        for (float v : taps)
            sum += v;
        if (sum == 0.0F)
            return *this;
        KernelSpec out;
        for (size_t i = 0; i < N; ++i)
            out.taps[i] = taps[i] / sum;
        return out;
    }

    /**
     * @brief Negate all taps.
     *
     * Derives the transpose derivative kernel from its pair:
     *   sobel_ky = sobel_kx.negate()
     * Not needed for Sobel/Scharr since kx == ky, but useful for
     * asymmetric operators.
     */
    [[nodiscard]] constexpr KernelSpec negate() const noexcept
    {
        KernelSpec out;
        for (size_t i = 0; i < N; ++i)
            out.taps[i] = -taps[i];
        return out;
    }

    /**
     * @brief Reverse tap order.
     *
     * Reflects the kernel about its centre tap. For symmetric kernels
     * this is a no-op. Useful when a vertical kernel is the mirror of
     * its horizontal counterpart.
     */
    [[nodiscard]] constexpr KernelSpec reflect() const noexcept
    {
        KernelSpec out;
        for (size_t i = 0; i < N; ++i)
            out.taps[i] = taps[N - 1 - i];
        return out;
    }

    /**
     * @brief Multiply all taps by a scalar.
     */
    [[nodiscard]] constexpr KernelSpec scale(float s) const noexcept
    {
        KernelSpec out;
        for (size_t i = 0; i < N; ++i)
            out.taps[i] = taps[i] * s;
        return out;
    }

    /**
     * @brief Add a scalar to all taps.
     *
     * Useful for shifting a zero-mean derivative kernel to a non-negative
     * range for visualisation.
     */
    [[nodiscard]] constexpr KernelSpec shift(float s) const noexcept
    {
        KernelSpec out;
        for (size_t i = 0; i < N; ++i)
            out.taps[i] = taps[i] + s;
        return out;
    }

    /**
     * @brief Convolve this kernel with another of the same length.
     *
     * Produces a kernel of length 2*N-1 representing the sequential
     * application of both. Use to construct larger kernels from smaller
     * primitives, e.g. binomial5 = binomial3.convolve(binomial3).
     *
     * @tparam M Length of the other kernel. Result length is N+M-1.
     */
    template <size_t M>
    [[nodiscard]] constexpr KernelSpec<N + M - 1> convolve(
        const KernelSpec<M>& other) const noexcept
    {
        KernelSpec<N + M - 1> out;
        for (size_t i = 0; i < N; ++i)
            for (size_t j = 0; j < M; ++j)
                out.taps[i + j] += taps[i] * other.taps[j];
        return out;
    }

    // -------------------------------------------------------------------------
    // Span interop
    // -------------------------------------------------------------------------

    [[nodiscard]] constexpr std::span<const float> span() const noexcept
    {
        return { taps.data(), N };
    }

    constexpr operator std::span<const float>() const noexcept
    {
        return span();
    }

    [[nodiscard]] constexpr const float* data() const noexcept { return taps.data(); }
    [[nodiscard]] constexpr size_t size() const noexcept { return N; }
};

// ============================================================================
// Named kernels
// ============================================================================

namespace Kernels {

    // ----------------------------------------------------------------------------
    // Sobel
    //
    // 3x3. Applied separably:
    //   dx = filter_separable(gray, sobel_kx, sobel_smooth)
    //   dy = filter_separable(gray, sobel_smooth, sobel_ky)
    //
    // kx/ky: finite difference [-1, 0, 1]
    // smooth: binomial averaging [1, 2, 1], normalised to unit sum
    // ----------------------------------------------------------------------------

    inline constexpr auto sobel_kx = KernelSpec<3> { -1.0F, 0.0F, 1.0F };
    inline constexpr auto sobel_ky = KernelSpec<3> { -1.0F, 0.0F, 1.0F };
    inline constexpr auto sobel_smooth = KernelSpec<3> { 1.0F, 2.0F, 1.0F }.normalize();

    // ----------------------------------------------------------------------------
    // Scharr
    //
    // 3x3. Superior rotational symmetry vs Sobel. Same smooth kernel.
    //   dx = filter_separable(gray, scharr_kx, sobel_smooth)
    //   dy = filter_separable(gray, sobel_smooth, scharr_ky)
    // ----------------------------------------------------------------------------

    inline constexpr auto scharr_kx = KernelSpec<3> { -3.0F, 0.0F, 3.0F };
    inline constexpr auto scharr_ky = KernelSpec<3> { -3.0F, 0.0F, 3.0F };

    // ----------------------------------------------------------------------------
    // Prewitt
    //
    // 3x3. Uniform smoothing axis. Lower noise suppression than Sobel.
    //   dx = filter_separable(gray, prewitt_kx, prewitt_smooth)
    //   dy = filter_separable(gray, prewitt_smooth, prewitt_ky)
    // ----------------------------------------------------------------------------

    inline constexpr auto prewitt_kx = KernelSpec<3> { -1.0F, 0.0F, 1.0F };
    inline constexpr auto prewitt_ky = KernelSpec<3> { -1.0F, 0.0F, 1.0F };
    inline constexpr auto prewitt_smooth = KernelSpec<3> { 1.0F, 1.0F, 1.0F }.normalize();

    // ----------------------------------------------------------------------------
    // Binomial smoothing
    //
    // Pascal's triangle coefficients, normalised. Fast approximation to
    // Gaussian with integer arithmetic heritage. Use for repeated smoothing
    // passes or as building blocks via convolve().
    //
    //   binomial3 = [1, 2, 1] / 4        sigma ~= 0.5
    //   binomial5 = [1, 4, 6, 4, 1] / 16 sigma ~= 1.0
    //   binomial7 = [1,6,15,20,15,6,1]/64 sigma ~= 1.5
    // ----------------------------------------------------------------------------

    inline constexpr auto binomial3 = KernelSpec<3> { 1.0F, 2.0F, 1.0F }.normalize();
    inline constexpr auto binomial5 = KernelSpec<5> { 1.0F, 4.0F, 6.0F, 4.0F, 1.0F }.normalize();
    inline constexpr auto binomial7 = KernelSpec<7> { 1.0F, 6.0F, 15.0F, 20.0F, 15.0F, 6.0F, 1.0F }.normalize();

    // ----------------------------------------------------------------------------
    // Box filters
    //
    // Uniform averaging. Fast but introduces ringing on sharp edges.
    // Prefer binomial for image processing; use box for debug/preview.
    // ----------------------------------------------------------------------------

    inline constexpr auto box3 = KernelSpec<3> { 1.0F, 1.0F, 1.0F }.normalize();
    inline constexpr auto box5 = KernelSpec<5> { 1.0F, 1.0F, 1.0F, 1.0F, 1.0F }.normalize();

    // ----------------------------------------------------------------------------
    // Difference of Gaussians approximation
    //
    // Two fixed-sigma Gaussian approximations via binomial convolution.
    // Subtract coarse from fine (pointwise) after two separate blur passes
    // to approximate a Laplacian of Gaussian without runtime kernel construction.
    //
    //   log_fine   ~= sigma 1.0  (binomial5)
    //   log_coarse ~= sigma 2.0  (binomial5 convolved with itself -> binomial9)
    // ----------------------------------------------------------------------------

    inline constexpr auto log_fine = binomial5;
    inline constexpr auto log_coarse = binomial5.convolve(binomial5).normalize();

} // namespace Kernels

} // namespace MayaFlux::Kinesis::Vision
