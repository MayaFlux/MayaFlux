#pragma once

#ifdef PIPEWIRE_MIDI_BACKEND
namespace MayaFlux::Core {
using MIDIBackend = PipeWireMIDIBackend;
}
#else
#include "RtMidiBackend.hpp"
namespace MayaFlux::Core {
using MIDIBackend = RtMidiBackend;
}
#endif
