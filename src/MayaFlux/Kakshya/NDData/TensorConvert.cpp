#include "TensorConvert.hpp"

#include "SmallFloat.hpp"

#include <bit>

namespace MayaFlux::Kakshya {

namespace {

    enum class Family : uint8_t {
        SIGNED,
        UNSIGNED,
        BOOL,
        FLOAT,
        COMPLEX
    };

    struct Layout {
        Family family {};
        uint8_t bits {};
        bool brain {};
    };

    struct Scalar {
        double real {};
        uint64_t magnitude {};
        bool negative {};
        bool exact {};
    };

    using Decoder = Scalar (*)(const std::byte*);
    using Encoder = void (*)(std::byte*, const Scalar&);

    bool one_of(uint8_t bits, std::initializer_list<uint8_t> widths)
    {
        return std::ranges::find(widths, bits) != widths.end();
    }

    std::optional<Layout> layout_of(TensorDType dtype, std::string& why)
    {
        if (dtype.lanes != 1) {
            why = dtype.name() + " holds several lanes per element";
            return std::nullopt;
        }

        switch (dtype.kind) {
        case TensorDTypeKind::INT:
            if (one_of(dtype.bits, { 8, 16, 32, 64 }))
                return Layout { .family = Family::SIGNED, .bits = dtype.bits, .brain = false };
            break;
        case TensorDTypeKind::UINT:
            if (one_of(dtype.bits, { 8, 16, 32, 64 }))
                return Layout { .family = Family::UNSIGNED, .bits = dtype.bits, .brain = false };
            break;
        case TensorDTypeKind::BOOL:
            if (dtype.bits == 8)
                return Layout { .family = Family::BOOL, .bits = 8, .brain = false };
            break;
        case TensorDTypeKind::FLOAT:
            if (one_of(dtype.bits, { 16, 32, 64 }))
                return Layout { .family = Family::FLOAT, .bits = dtype.bits, .brain = false };
            break;
        case TensorDTypeKind::BFLOAT:
            if (dtype.bits == 16)
                return Layout { .family = Family::FLOAT, .bits = 16, .brain = true };
            break;
        case TensorDTypeKind::COMPLEX:
            if (one_of(dtype.bits, { 64, 128 }))
                return Layout { .family = Family::COMPLEX, .bits = dtype.bits, .brain = false };
            break;
        default:
            break;
        }

        why = dtype.name() + " is not supported by conversion";
        return std::nullopt;
    }

    bool holds_quantization(Layout layout)
    {
        return (layout.family == Family::SIGNED || layout.family == Family::UNSIGNED) && layout.bits <= 32;
    }

    bool same_quantization(const std::optional<TensorQuantization>& a, const std::optional<TensorQuantization>& b)
    {
        if (!a || !b)
            return !a && !b;
        return a->scale == b->scale && a->zero_point == b->zero_point && a->axis == b->axis;
    }

    double wide_real(const Scalar& value)
    {
        constexpr uint64_t exact_limit = uint64_t { 1 } << 53;
        if (!value.exact || value.magnitude <= exact_limit)
            return value.real;

        const int shift = std::bit_width(value.magnitude) - 53;
        uint64_t mantissa = value.magnitude >> shift;
        if ((value.magnitude & ((uint64_t { 1 } << shift) - 1U)) != 0U)
            mantissa |= 1U;
        const double magnitude = std::ldexp(static_cast<double>(mantissa), shift);
        return value.negative ? -magnitude : magnitude;
    }

    template <typename T>
    T read(const std::byte* source)
    {
        T value;
        std::memcpy(&value, source, sizeof(T));
        return value;
    }

    template <typename T>
    void write(std::byte* destination, T value)
    {
        std::memcpy(destination, &value, sizeof(T));
    }

    template <typename T>
    Scalar decode_signed(const std::byte* source)
    {
        const auto value = read<T>(source);
        Scalar result;
        result.exact = true;
        result.real = static_cast<double>(value);
        result.negative = value < 0;
        result.magnitude = value < 0
            ? static_cast<uint64_t>(-(static_cast<int64_t>(value) + 1)) + 1U
            : static_cast<uint64_t>(value);
        return result;
    }

    template <typename T>
    Scalar decode_unsigned(const std::byte* source)
    {
        const auto value = read<T>(source);
        Scalar result;
        result.exact = true;
        result.real = static_cast<double>(value);
        result.magnitude = value;
        return result;
    }

    Scalar decode_bool(const std::byte* source)
    {
        Scalar result;
        result.exact = true;
        result.magnitude = *source != std::byte { 0 } ? 1U : 0U;
        result.real = static_cast<double>(result.magnitude);
        return result;
    }

    Scalar decode_f32(const std::byte* source)
    {
        return Scalar { .real = static_cast<double>(read<float>(source)) };
    }

    Scalar decode_f64(const std::byte* source)
    {
        return Scalar { .real = read<double>(source) };
    }

    Scalar decode_f16(const std::byte* source)
    {
        return Scalar { .real = decode_small_float(read<uint16_t>(source), 5, 10) };
    }

    Scalar decode_bf16(const std::byte* source)
    {
        return Scalar { .real = decode_small_float(read<uint16_t>(source), 8, 7) };
    }

    template <typename T>
    T integer_from_real(double value)
    {
        if (std::isnan(value))
            return T {};
        const double rounded = std::nearbyint(value);
        if (rounded <= static_cast<double>(std::numeric_limits<T>::min()))
            return std::numeric_limits<T>::min();
        if (rounded >= static_cast<double>(std::numeric_limits<T>::max()))
            return std::numeric_limits<T>::max();
        return static_cast<T>(rounded);
    }

    template <typename T>
    void encode_signed(std::byte* destination, const Scalar& value)
    {
        T result {};
        if (!value.exact) {
            result = integer_from_real<T>(value.real);
        } else if (value.negative) {
            const uint64_t limit = static_cast<uint64_t>(std::numeric_limits<T>::max()) + 1U;
            result = value.magnitude >= limit
                ? std::numeric_limits<T>::min()
                : static_cast<T>(-static_cast<int64_t>(value.magnitude));
        } else {
            result = value.magnitude > static_cast<uint64_t>(std::numeric_limits<T>::max())
                ? std::numeric_limits<T>::max()
                : static_cast<T>(value.magnitude);
        }
        write(destination, result);
    }

    template <typename T>
    void encode_unsigned(std::byte* destination, const Scalar& value)
    {
        T result {};
        if (!value.exact) {
            result = integer_from_real<T>(value.real);
        } else if (value.negative) {
            result = 0;
        } else {
            result = value.magnitude > static_cast<uint64_t>(std::numeric_limits<T>::max())
                ? std::numeric_limits<T>::max()
                : static_cast<T>(value.magnitude);
        }
        write(destination, result);
    }

    void encode_bool(std::byte* destination, const Scalar& value)
    {
        const bool truth = value.exact ? value.magnitude != 0U : value.real != 0.0;
        *destination = truth ? std::byte { 1 } : std::byte { 0 };
    }

    void encode_f32(std::byte* destination, const Scalar& value)
    {
        write(destination, static_cast<float>(wide_real(value)));
    }

    void encode_f64(std::byte* destination, const Scalar& value)
    {
        write(destination, value.real);
    }

    void encode_f16(std::byte* destination, const Scalar& value)
    {
        write(destination, static_cast<uint16_t>(encode_small_float(wide_real(value), 5, 10)));
    }

    void encode_bf16(std::byte* destination, const Scalar& value)
    {
        write(destination, static_cast<uint16_t>(encode_small_float(wide_real(value), 8, 7)));
    }

    Decoder decoder_for(Layout layout)
    {
        switch (layout.family) {
        case Family::SIGNED:
            switch (layout.bits) {
            case 8:
                return decode_signed<int8_t>;
            case 16:
                return decode_signed<int16_t>;
            case 32:
                return decode_signed<int32_t>;
            default:
                return decode_signed<int64_t>;
            }
        case Family::UNSIGNED:
            switch (layout.bits) {
            case 8:
                return decode_unsigned<uint8_t>;
            case 16:
                return decode_unsigned<uint16_t>;
            case 32:
                return decode_unsigned<uint32_t>;
            default:
                return decode_unsigned<uint64_t>;
            }
        case Family::BOOL:
            return decode_bool;
        default:
            switch (layout.bits) {
            case 16:
                return layout.brain ? decode_bf16 : decode_f16;
            case 32:
                return decode_f32;
            default:
                return decode_f64;
            }
        }
    }

    Encoder encoder_for(Layout layout)
    {
        switch (layout.family) {
        case Family::SIGNED:
            switch (layout.bits) {
            case 8:
                return encode_signed<int8_t>;
            case 16:
                return encode_signed<int16_t>;
            case 32:
                return encode_signed<int32_t>;
            default:
                return encode_signed<int64_t>;
            }
        case Family::UNSIGNED:
            switch (layout.bits) {
            case 8:
                return encode_unsigned<uint8_t>;
            case 16:
                return encode_unsigned<uint16_t>;
            case 32:
                return encode_unsigned<uint32_t>;
            default:
                return encode_unsigned<uint64_t>;
            }
        case Family::BOOL:
            return encode_bool;
        default:
            switch (layout.bits) {
            case 16:
                return layout.brain ? encode_bf16 : encode_f16;
            case 32:
                return encode_f32;
            default:
                return encode_f64;
            }
        }
    }

    template <typename Visit>
    void walk(const TensorAccess& source, Visit visit)
    {
        const auto& spec = source.spec();
        const uint64_t count = source.element_count();
        const auto element = static_cast<int64_t>(spec.dtype.element_bytes());
        const std::vector<int64_t> strides = spec.resolved_strides();
        const std::byte* base = source.element_data();
        std::vector<uint64_t> index(spec.shape.size(), 0);

        for (uint64_t n = 0; n < count; ++n) {
            int64_t offset = 0;
            for (size_t axis = 0; axis < index.size(); ++axis)
                offset += strides[axis] * static_cast<int64_t>(index[axis]);
            visit(n, base + offset * element, index);
            for (size_t axis = index.size(); axis-- > 0;) {
                if (++index[axis] < spec.shape[axis])
                    break;
                index[axis] = 0;
            }
        }
    }

    double zero_point_at(const TensorQuantization& quantization, size_t slice)
    {
        return quantization.zero_point.empty() ? 0.0 : static_cast<double>(quantization.zero_point[slice]);
    }

    void convert_real(const TensorAccess& source, Layout from, Layout to, const std::optional<TensorQuantization>& requested, std::byte* out)
    {
        const Decoder decode = decoder_for(from);
        const Encoder encode = encoder_for(to);
        const size_t out_bytes = to.family == Family::BOOL ? 1U : to.bits / 8U;
        const TensorQuantization* in_q = source.spec().quantization ? &*source.spec().quantization : nullptr;
        const TensorQuantization* out_q = requested ? &*requested : nullptr;

        walk(source, [&](uint64_t n, const std::byte* element, const std::vector<uint64_t>& index) {
            Scalar value = decode(element);
            if (in_q != nullptr) {
                const size_t slice = in_q->axis ? index[*in_q->axis] : 0U;
                value.real = in_q->scale[slice] * (value.real - zero_point_at(*in_q, slice));
                value.exact = false;
            }
            if (out_q != nullptr) {
                const size_t slice = out_q->axis ? index[*out_q->axis] : 0U;
                const double zero = zero_point_at(*out_q, slice);
                const double scaled = value.real / out_q->scale[slice];
                value.real = std::isnan(scaled) ? zero : std::nearbyint(scaled) + zero;
                value.exact = false;
            }
            encode(out + n * out_bytes, value);
        });
    }

    void convert_complex(const TensorAccess& source, Layout from, Layout to, std::byte* out)
    {
        const size_t out_bytes = to.bits / 8U;
        walk(source, [&](uint64_t n, const std::byte* element, const std::vector<uint64_t>&) {
            const std::complex<double> value = from.bits == 64
                ? std::complex<double>(read<std::complex<float>>(element))
                : read<std::complex<double>>(element);
            std::byte* destination = out + n * out_bytes;
            if (to.bits == 64) {
                write(destination, std::complex<float>(static_cast<float>(value.real()), static_cast<float>(value.imag())));
            } else {
                write(destination, value);
            }
        });
    }

}

std::string diagnose_conversion(
    const TensorSpec& source, TensorDType target, const std::optional<TensorQuantization>& quantization)
{
    if (auto problem = source.diagnose(); !problem.empty())
        return problem;

    const std::string label = source.name.empty() ? std::string("tensor") : "tensor '" + source.name + "'";
    if (!source.device.is_host_accessible())
        return label + ": memory is not readable by the host";

    std::string why;
    const auto from = layout_of(source.dtype, why);
    if (!from)
        return label + ": source " + why;
    const auto to = layout_of(target, why);
    if (!to)
        return label + ": target " + why;

    if ((from->family == Family::COMPLEX) != (to->family == Family::COMPLEX))
        return label + ": complex and real element types do not convert to each other";

    if (source.quantization && !holds_quantization(*from))
        return label + ": quantization on " + source.dtype.name() + " storage is not supported";

    if (quantization) {
        if (!holds_quantization(*to))
            return label + ": " + target.name() + " cannot hold quantization, use an integer type of at most 32 bits";

        TensorSpec probe = source;
        probe.dtype = target;
        probe.strides.clear();
        probe.byte_offset = 0;
        probe.quantization = quantization;
        if (auto problem = probe.diagnose(); !problem.empty())
            return problem;

        const bool usable = std::ranges::all_of(quantization->scale,
            [](double scale) { return std::isfinite(scale) && scale != 0.0; });
        if (!usable)
            return label + ": quantization scales must be finite and non-zero";
    }

    return {};
}

std::expected<TensorData, std::string> convert_tensor(
    const TensorAccess& source, TensorDType target, const std::optional<TensorQuantization>& quantization)
{
    const TensorSpec& from_spec = source.spec();
    if (auto problem = diagnose_conversion(from_spec, target, quantization); !problem.empty())
        return std::unexpected(std::move(problem));

    if (from_spec.dtype == target && same_quantization(from_spec.quantization, quantization)) {
        auto copy = TensorData::copy_of(source);
        if (!copy)
            return std::unexpected("tensor '" + from_spec.name + "': the copy could not be made");
        return std::move(*copy);
    }

    TensorSpec spec = from_spec;
    spec.dtype = target;
    spec.strides.clear();
    spec.byte_offset = 0;
    spec.device = {};
    spec.quantization = quantization;

    auto result = TensorData::allocate(std::move(spec));
    if (!result)
        return std::unexpected("tensor '" + from_spec.name + "': the result could not be allocated");
    if (source.element_count() == 0)
        return std::move(*result);

    std::string unused;
    const Layout from = *layout_of(from_spec.dtype, unused);
    const Layout to = *layout_of(target, unused);
    std::byte* out = result->storage().data();

    if (from.family == Family::COMPLEX) {
        convert_complex(source, from, to, out);
    } else {
        convert_real(source, from, to, quantization, out);
    }
    return std::move(*result);
}

}
