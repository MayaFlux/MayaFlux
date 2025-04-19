#include "Device.hpp"

using namespace MayaFlux::Core;

Device::Device(RtAudio* Context)
{

    if (Context->getDeviceCount() == 0) {
        throw std::runtime_error("No audio devices found");
    }

    for (auto id : Context->getDeviceIds()) {

        try {
            RtAudio::DeviceInfo info = Context->getDeviceInfo(id);

            if (info.outputChannels > 0) {
                m_Output_devices.push_back(info);
            }

            if (info.inputChannels > 0) {
                m_Input_devices.push_back(info);
            }
        } catch (RtAudioErrorType& e) {
            std::cerr << "Error probing device: " << id << ": " << e << "\n";
        }
    }

    m_Num_in_devices = m_Input_devices.size();
    m_Num_out_devices = m_Output_devices.size();

    m_default_out_device = Context->getDefaultOutputDevice();
    m_default_in_device = Context->getDefaultInputDevice();
}
