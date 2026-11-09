#include "TensorDLPack.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include "dlpack.h"

namespace MayaFlux::Kakshya {

static_assert(static_cast<int>(TensorDTypeKind::INT) == kDLInt);
static_assert(static_cast<int>(TensorDTypeKind::UINT) == kDLUInt);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT) == kDLFloat);
static_assert(static_cast<int>(TensorDTypeKind::OPAQUE_HANDLE) == kDLOpaqueHandle);
static_assert(static_cast<int>(TensorDTypeKind::BFLOAT) == kDLBfloat);
static_assert(static_cast<int>(TensorDTypeKind::COMPLEX) == kDLComplex);
static_assert(static_cast<int>(TensorDTypeKind::BOOL) == kDLBool);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E3M4) == kDLFloat8_e3m4);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E4M3) == kDLFloat8_e4m3);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E4M3B11FNUZ) == kDLFloat8_e4m3b11fnuz);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E4M3FN) == kDLFloat8_e4m3fn);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E4M3FNUZ) == kDLFloat8_e4m3fnuz);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E5M2) == kDLFloat8_e5m2);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E5M2FNUZ) == kDLFloat8_e5m2fnuz);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT8_E8M0FNU) == kDLFloat8_e8m0fnu);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT6_E2M3FN) == kDLFloat6_e2m3fn);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT6_E3M2FN) == kDLFloat6_e3m2fn);
static_assert(static_cast<int>(TensorDTypeKind::FLOAT4_E2M1FN) == kDLFloat4_e2m1fn);
static_assert(static_cast<int>(TensorDTypeKind::BCOMPLEX) == kDLBcomplex);
static_assert(static_cast<int>(TensorDeviceType::CPU) == kDLCPU);
static_assert(static_cast<int>(TensorDeviceType::CUDA) == kDLCUDA);
static_assert(static_cast<int>(TensorDeviceType::CUDA_HOST) == kDLCUDAHost);
static_assert(static_cast<int>(TensorDeviceType::OPENCL) == kDLOpenCL);
static_assert(static_cast<int>(TensorDeviceType::VULKAN) == kDLVulkan);
static_assert(static_cast<int>(TensorDeviceType::METAL) == kDLMetal);
static_assert(static_cast<int>(TensorDeviceType::VPI) == kDLVPI);
static_assert(static_cast<int>(TensorDeviceType::ROCM) == kDLROCM);
static_assert(static_cast<int>(TensorDeviceType::ROCM_HOST) == kDLROCMHost);
static_assert(static_cast<int>(TensorDeviceType::EXT_DEV) == kDLExtDev);
static_assert(static_cast<int>(TensorDeviceType::CUDA_MANAGED) == kDLCUDAManaged);
static_assert(static_cast<int>(TensorDeviceType::ONEAPI) == kDLOneAPI);
static_assert(static_cast<int>(TensorDeviceType::WEBGPU) == kDLWebGPU);
static_assert(static_cast<int>(TensorDeviceType::HEXAGON) == kDLHexagon);
static_assert(static_cast<int>(TensorDeviceType::MAIA) == kDLMAIA);
static_assert(static_cast<int>(TensorDeviceType::TRN) == kDLTrn);
static_assert(static_cast<int>(TensorDeviceType::TPU) == kDLTPU);
static_assert(static_cast<int>(TensorDeviceType::TPU_HOST) == kDLTPUHost);
static_assert(static_cast<int>(TensorDeviceType::ASCEND) == kDLAscend);

namespace {

    struct ExportContext {
        std::shared_ptr<const TensorData> owner;
        std::vector<int64_t> shape;
        std::vector<int64_t> strides;
        DLManagedTensorVersioned managed {};
    };

    void release_export(DLManagedTensorVersioned* self)
    {
        delete static_cast<ExportContext*>(self->manager_ctx);
    }

    bool known_device(int32_t type)
    {
        switch (type) {
        case kDLCPU:
        case kDLCUDA:
        case kDLCUDAHost:
        case kDLOpenCL:
        case kDLVulkan:
        case kDLMetal:
        case kDLVPI:
        case kDLROCM:
        case kDLROCMHost:
        case kDLExtDev:
        case kDLCUDAManaged:
        case kDLOneAPI:
        case kDLWebGPU:
        case kDLHexagon:
        case kDLMAIA:
        case kDLTrn:
        case kDLTPU:
        case kDLTPUHost:
        case kDLAscend:
            return true;
        default:
            return false;
        }
    }

    std::string spec_from(const DLTensor& tensor, TensorSpec& spec)
    {
        if (tensor.ndim < 0)
            return "negative rank";
        if (tensor.ndim > 0 && !tensor.shape)
            return "missing shape";
        if (tensor.dtype.code > kDLBcomplex || tensor.dtype.bits == 0 || tensor.dtype.lanes == 0)
            return "unsupported element type";
        if (!known_device(static_cast<int32_t>(tensor.device.device_type)))
            return "unknown device type";

        spec.dtype = { .kind = static_cast<TensorDTypeKind>(tensor.dtype.code),
            .bits = tensor.dtype.bits,
            .lanes = tensor.dtype.lanes };
        spec.device = { .type = static_cast<TensorDeviceType>(tensor.device.device_type),
            .id = tensor.device.device_id };

        for (int32_t i = 0; i < tensor.ndim; ++i) {
            if (tensor.shape[i] < 0)
                return "negative extent";
            spec.shape.push_back(static_cast<uint64_t>(tensor.shape[i]));
        }
        if (tensor.strides) {
            spec.strides.assign(tensor.strides, tensor.strides + tensor.ndim);
        }
        spec.byte_offset = tensor.byte_offset;
        return {};
    }

}

DLManagedTensorVersioned* export_dlpack(std::shared_ptr<const TensorData> tensor)
{
    if (!tensor || !tensor->is_valid() || !tensor->spec().is_valid()) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "export_dlpack: tensor is null or invalid");
        return nullptr;
    }

    auto context = std::make_unique<ExportContext>();
    const TensorSpec& spec = tensor->spec();
    const bool empty = spec.element_count().value_or(0) == 0;

    for (const auto extent : spec.shape)
        context->shape.push_back(static_cast<int64_t>(extent));
    context->strides = spec.resolved_strides();
    context->owner = std::move(tensor);

    auto& managed = context->managed;
    managed.version = { .major = DLPACK_MAJOR_VERSION, .minor = DLPACK_MINOR_VERSION };
    managed.flags = DLPACK_FLAG_BITMASK_READ_ONLY;
    managed.deleter = release_export;
    managed.dl_tensor.data = empty ? nullptr
                                   : const_cast<std::byte*>(context->owner->storage().data());
    managed.dl_tensor.device = { .device_type = static_cast<DLDeviceType>(spec.device.type),
        .device_id = spec.device.id };
    managed.dl_tensor.ndim = static_cast<int32_t>(spec.shape.size());
    managed.dl_tensor.dtype = { .code = static_cast<uint8_t>(spec.dtype.kind),
        .bits = spec.dtype.bits,
        .lanes = spec.dtype.lanes };
    managed.dl_tensor.shape = context->shape.data();
    managed.dl_tensor.strides = context->strides.data();
    managed.dl_tensor.byte_offset = empty ? 0 : spec.byte_offset;

    auto* raw = context.release();
    raw->managed.manager_ctx = raw;
    return &raw->managed;
}

TensorImport::TensorImport(TensorSpec spec, const std::byte* data, size_t size, bool read_only,
    std::function<void()> release) noexcept
    : m_spec(std::move(spec))
    , m_data(data)
    , m_size(size)
    , m_read_only(read_only)
    , m_release(std::move(release))
{
}

TensorImport::TensorImport(TensorImport&& other) noexcept
    : m_spec(std::move(other.m_spec))
    , m_data(other.m_data)
    , m_size(other.m_size)
    , m_read_only(other.m_read_only)
    , m_release(std::move(other.m_release))
{
    other.m_release = nullptr;
}

TensorImport& TensorImport::operator=(TensorImport&& other) noexcept
{
    if (this != &other) {
        if (m_release)
            m_release();
        m_spec = std::move(other.m_spec);
        m_data = other.m_data;
        m_size = other.m_size;
        m_read_only = other.m_read_only;
        m_release = std::move(other.m_release);
        other.m_release = nullptr;
    }
    return *this;
}

TensorImport::~TensorImport()
{
    if (m_release)
        m_release();
}

namespace {

    std::optional<TensorImport> reject(const std::function<void()>& release, const std::string& reason)
    {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::Runtime,
            "TensorImport: {}", reason);
        release();
        return std::nullopt;
    }

}

std::optional<TensorImport> TensorImport::take(DLManagedTensorVersioned* managed)
{
    if (!managed)
        return std::nullopt;

    const std::function<void()> release = [managed] {
        if (managed->deleter)
            managed->deleter(managed);
    };

    if (managed->version.major != DLPACK_MAJOR_VERSION)
        return reject(release, "unsupported DLPack major version " + std::to_string(managed->version.major));
    if ((managed->flags & DLPACK_FLAG_BITMASK_IS_SUBBYTE_TYPE_PADDED) != 0)
        return reject(release, "padded sub-byte element storage is not supported");

    TensorSpec spec;
    if (const auto why = spec_from(managed->dl_tensor, spec); !why.empty())
        return reject(release, why);

    const bool empty = spec.element_count().value_or(0) == 0;
    if (empty)
        spec.byte_offset = 0;
    const auto range = spec.byte_range();
    if (!range)
        return reject(release, spec.diagnose());

    const auto* data = static_cast<const std::byte*>(managed->dl_tensor.data);
    const size_t size = empty ? 0 : static_cast<size_t>(range->second);
    if (!empty && !data)
        return reject(release, "null data pointer for a non-empty tensor");

    const bool read_only = (managed->flags & DLPACK_FLAG_BITMASK_READ_ONLY) != 0;
    TensorImport imported(std::move(spec), data, size, read_only, release);
    if (!imported.access()) {
        imported.m_release = nullptr;
        return reject(release, "tensor layout is not valid host memory");
    }
    return imported;
}

std::optional<TensorImport> TensorImport::take(DLManagedTensor* managed)
{
    if (!managed)
        return std::nullopt;

    const std::function<void()> release = [managed] {
        if (managed->deleter)
            managed->deleter(managed);
    };

    TensorSpec spec;
    if (const auto why = spec_from(managed->dl_tensor, spec); !why.empty())
        return reject(release, why);

    const bool empty = spec.element_count().value_or(0) == 0;
    if (empty)
        spec.byte_offset = 0;
    const auto range = spec.byte_range();
    if (!range)
        return reject(release, spec.diagnose());

    const auto* data = static_cast<const std::byte*>(managed->dl_tensor.data);
    const size_t size = empty ? 0 : static_cast<size_t>(range->second);
    if (!empty && !data)
        return reject(release, "null data pointer for a non-empty tensor");

    TensorImport imported(std::move(spec), data, size, false, release);
    if (!imported.access()) {
        imported.m_release = nullptr;
        return reject(release, "tensor layout is not valid host memory");
    }
    return imported;
}

std::optional<TensorAccess> TensorImport::access() const
{
    return as_tensor_access({ m_data, m_size }, m_spec, m_read_only);
}

std::optional<TensorData> TensorImport::copy() const
{
    const auto view = access();
    return view ? TensorData::copy_of(*view) : std::nullopt;
}

}
