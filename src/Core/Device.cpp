#include "Core/Device.hpp"

using namespace MayaFlux::Core;

Device::Device(RtAudio* Context)
{
    m_Num_devices = Context->getDeviceCount();

    for (unsigned int i = 0; i < m_Num_devices; i++) {
        RtAudio::DeviceInfo info = Context->getDeviceInfo(i);
        m_Devices.push_back(info);
    }

    m_default_out_device = Context->getDefaultOutputDevice();
    m_default_in_device = Context->getDefaultInputDevice();
}
