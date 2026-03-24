#include "MessageUtils.hpp"

#include "MayaFlux/Core/Input/OscParser.hpp"

namespace MayaFlux::Portal::Network {

std::optional<Core::InputValue::OSCMessage>
as_osc(const Core::NetworkMessage& msg)
{
    auto result = Core::OscParser::parse(msg.data.data(), msg.data.size());
    if (!result || result->type != Core::InputValue::Type::OSC) {
        return std::nullopt;
    }
    return result->as_osc();
}

std::vector<uint8_t>
serialize_osc(const std::string& address,
    const std::vector<Core::InputValue::OSCArg>& args)
{
    return Core::OscParser::serialize(address, args);
}

} // namespace MayaFlux::Portal::Network
