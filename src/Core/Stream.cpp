#include "Core/Stream.hpp"

using namespace MayaFlux::Core;

Stream::Stream(unsigned int out_device, GlobalStreamInfo stream_info)
{
    m_Stream_info = stream_info;

    m_Parameters.deviceId = out_device;
    m_Parameters.nChannels = m_Stream_info.num_channels;
}
