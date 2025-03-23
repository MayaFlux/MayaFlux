#pragma once

#include "config.h"

namespace MayaFlux {

namespace Core {

    class Device {
    public:
        Device(RtAudio* context);
        ~Device() = default;

        inline unsigned int get_default_output_device() const
        {
            return m_default_out_device;
        }

        inline unsigned int get_default_input_device() const
        {
            return m_default_in_device;
        }

    private:
        unsigned int m_Num_in_devices, m_Num_out_devices;
        std::vector<RtAudio::DeviceInfo> m_Input_devices;
        std::vector<RtAudio::DeviceInfo> m_Output_devices;
        unsigned int m_default_out_device;
        unsigned int m_default_in_device;
    };
}

}
