#pragma once

#ifdef PIPEWIRE_BACKEND
#include "PipewireMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = PipewireMidiBackend;
}
#else
#include "RtMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = RtMidiBackend;
}
#endif
