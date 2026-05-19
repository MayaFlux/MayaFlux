#pragma once

#ifdef MAYAFLUX_PLATFORM_LINUX
#include "PipewireMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = PipewireMidiBackend;
}
#elif defined(MAYAFLUX_PLATFORM_WINDOWS)
#include "WinMmMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = WinMmMidiBackend;
}
#else
#include "RtMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = RtMidiBackend;
}
#endif
