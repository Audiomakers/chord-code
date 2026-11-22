#pragma once
#include "Music.h"
#include <filesystem>
#include <vector>
namespace ccp {
enum class MidiRhythm { straight, humanize, strum, arpeggio, count };
// Alternation of chord lengths, in pairs: LONGO-CURTO plays chord 1 for 1.5x and chord 2 for 0.5x the
// chosen duration (the loop keeps its total length), CURTO-LONGO the reverse.
enum class Groove { even, longShort, shortLong, count };
struct MidiOptions {int bpm{120};double beatsPerChord{1.0};int velocity{127};MidiRhythm rhythm{MidiRhythm::straight};Style style{Style::standard};Groove groove{Groove::even};};
// Where a chord starts and how long it lasts, in beats, with the groove applied.
double chordStartBeats(const MidiOptions&,int index);
double chordLengthBeats(const MidiOptions&,int index);
class MidiFile {public:static constexpr int ppq=960;static std::vector<uint8_t>bytes(const Progression&,const MidiOptions&);static std::vector<uint8_t>basslineBytes(const Progression&,const MidiOptions&);static bool write(const Progression&,const std::filesystem::path&,const MidiOptions&);static bool writeBassline(const Progression&,const std::filesystem::path&,const MidiOptions&);};
}
