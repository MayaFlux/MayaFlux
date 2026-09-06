#pragma once

namespace MayaFlux::Kinesis {

/**
 * @struct FieldSource
 * @brief Parsed representation of a stringified field lambda.
 *
 * Produced by MF_FIELD, which stringifies the lambda text at the call site.
 * Shares an input shape with Portal::Graphics::KernelSource but not a consumer,
 * and differs in one respect: the parameter list is retained verbatim rather
 * than stripped to identifiers, because a field is emitted as a standalone GLSL
 * function above main() where the signature needs types. KernelSource strips
 * them because a main-injected body receives its operands as file-scope
 * declarations.
 *
 * Nothing here knows about ShaderSpec. The four members map onto the four
 * arguments of ShaderSpec::Assemble::function(), and that call happens at the
 * site assembling a spec, which is above both Kinesis and Portal.
 *
 * @code
 * auto spec = ShaderSpec::Assemble{}
 *     .ssbo("pos", BindingDirection::InOut, Kakshya::GpuDataFormat::VEC4_F32)
 *     .function(swirl.source.return_type, swirl.source.name,
 *               swirl.source.params, swirl.source.body)
 *     .kernel(MF_KERNEL(...))
 *     .build();
 * @endcode
 */
struct FieldSource {
    std::string name;
    std::string return_type;
    std::string params;
    std::string body;

    /**
     * @brief Extract return type, parameter list and body from lambda text.
     *
     * Requires a trailing return type: the return type is read from between the
     * arrow and the opening brace. A lambda without one yields an empty
     * return_type, which valid() reports as a parse failure.
     *
     * @param fn_name Name the emitted GLSL function will carry.
     * @param text    Raw text produced by MF_FIELD.
     * @return Populated FieldSource.
     */
    [[nodiscard]] static FieldSource parse(std::string fn_name, std::string_view text)
    {
        FieldSource fs;
        fs.name = std::move(fn_name);

        const auto bracket = text.find('[');
        const auto paren_open = text.find('(', bracket);
        if (paren_open == std::string_view::npos)
            return fs;

        std::size_t depth = 1;
        std::size_t paren_close = paren_open + 1;
        while (paren_close < text.size() && depth > 0) {
            if (text[paren_close] == '(') {
                ++depth;
            } else if (text[paren_close] == ')') {
                --depth;
            }
            ++paren_close;
        }
        --paren_close;

        fs.params = std::string(text.substr(paren_open + 1, paren_close - paren_open - 1));

        const auto arrow = text.find("->", paren_close);
        const auto brace_open = text.find('{', paren_close);
        if (brace_open == std::string_view::npos)
            return fs;

        if (arrow != std::string_view::npos && arrow < brace_open) {
            auto rt = text.substr(arrow + 2, brace_open - arrow - 2);
            const auto b = rt.find_first_not_of(" \t\n\r");
            const auto e = rt.find_last_not_of(" \t\n\r");
            if (b != std::string_view::npos)
                fs.return_type = std::string(rt.substr(b, e - b + 1));
        }

        depth = 1;
        std::size_t brace_close = brace_open + 1;
        while (brace_close < text.size() && depth > 0) {
            if (text[brace_close] == '{') {
                ++depth;
            } else if (text[brace_close] == '}') {
                --depth;
            }
            ++brace_close;
        }
        --brace_close;

        fs.body = std::string(text.substr(brace_open + 1, brace_close - brace_open - 1));
        return fs;
    }

    /**
     * @brief Whether parsing produced a usable function definition.
     */
    [[nodiscard]] bool valid() const noexcept
    {
        return !name.empty() && !return_type.empty() && !body.empty();
    }
};

} // namespace MayaFlux::Kinesis
