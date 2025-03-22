#include "Device.hpp"

using namespace MayaFlux::Core;

Device::Device(RtAudio* Context)
{
    m_Num_devices = Context->getDeviceCount();

    if (m_Num_devices == 0) {
        throw std::runtime_error("No audio devices found");
    }

    for (unsigned int i = 0; i < m_Num_devices; i++) {
        try {
            RtAudio::DeviceInfo info = Context->getDeviceInfo(i);

            if (info.outputChannels > 0 || info.inputChannels > 0) {
                m_Devices.push_back(info);
            }
        } catch (RtAudioErrorType& e) {
            std::cerr << "Erroor probing device: " << i << ": " << e << "\n";
        }
    }

    std::cout << "Found " << m_Devices.size() << " devices\n";

    m_default_out_device = Context->getDefaultOutputDevice();
    m_default_in_device = Context->getDefaultInputDevice();
}
