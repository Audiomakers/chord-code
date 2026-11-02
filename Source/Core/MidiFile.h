#pragma once
#include "Music.h"
#include <string>
#include <vector>
namespace ccp {
enum class MidiRhythm { straight, humanize, strum, arpeggio, count };
struct MidiOptions {int bpm{120};double beatsPerChord{1.0};int velocity{127};MidiRhythm rhythm{MidiRhythm::straight};Style style{Style::standard};};
class MidiFile {public:static constexpr int ppq=960;static std::vector<uint8_t>bytes(const Progression&,const MidiOptions&);static std::vector<uint8_t>basslineBytes(const Progression&,const MidiOptions&);static bool write(const Progression&,const std::string&,const MidiOptions&);static bool writeBassline(const Progression&,const std::string&,const MidiOptions&);};
}
