#include "InputBackend.hpp"

#include <utf8proc.h>

namespace MayaFlux::Core {

std::string wide_to_utf8(const wchar_t* ws)
{
    if (!ws)
        return {};

    std::string out;
    for (const wchar_t* p = ws; *p != L'\0'; ++p) {
        auto cp = static_cast<utf8proc_int32_t>(static_cast<uint32_t>(*p));

        if constexpr (sizeof(wchar_t) == 2) {
            if (cp >= 0xD800 && cp <= 0xDBFF && p[1] >= 0xDC00 && p[1] <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10)
                    + (static_cast<utf8proc_int32_t>(*++p) - 0xDC00);
            }
        }

        std::array<utf8proc_uint8_t, 4> bytes {};
        const utf8proc_ssize_t length = utf8proc_encode_char(cp, bytes.data());
        if (length <= 0) {
            out.push_back('?');
            continue;
        }
        out.append(reinterpret_cast<const char*>(bytes.data()),
            static_cast<size_t>(length));
    }
    return out;
}

} // namespace MayaFlux::Core
