#pragma once

#include "ControlValueStore.hpp"

#include "MayaFlux/Transitive/Reflect/TextCodec.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace MayaFlux::Reflect {

/**
 * @brief A numeric control slot from text: one number, `true` or `false`, a vector of two to four numbers, or a box of six.
 *
 * The count of numbers picks the shape: 1 a number, 2 to 4 a float vector, 6 a
 * box of min then max. Any other count is Invalid. The slot takes the value as
 * a new pending one, so the next due() is true. A slot cannot know an enum's
 * type from text, so enumerators are written as their number.
 */
template <NumericControlSlot T>
struct TextCodec<T> {
    static constexpr size_t extent = 0;

    static bool decode(std::string_view source, T& value, TextError& error)
    {
        if (source == "true" || source == "false") {
            value.assign(T(source == "true"));
            return true;
        }

        const auto numbers = parse_numbers<double>(source);
        if (!numbers) {
            error = TextError::Invalid;
            return false;
        }

        const auto& n = *numbers;
        const auto f = [&](size_t index) { return static_cast<float>(n[index]); };
        switch (n.size()) {
        case 1:
            value.assign(T(n[0]));
            return true;
        case 2:
            value.assign(T(glm::vec2(f(0), f(1))));
            return true;
        case 3:
            value.assign(T(glm::vec3(f(0), f(1), f(2))));
            return true;
        case 4:
            value.assign(T(glm::vec4(f(0), f(1), f(2), f(3))));
            return true;
        case 6: {
            struct Box {
                glm::vec3 min;
                glm::vec3 max;
            };
            value.assign(T(Box { { f(0), f(1), f(2) }, { f(3), f(4), f(5) } }));
            return true;
        }
        default:
            error = TextError::Invalid;
            return false;
        }
    }
};

}
