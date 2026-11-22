#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace ccp {
constexpr int maxChords=8;
constexpr int maxNotes=10;   // bass + upper voicing; MUITO RICO uses up to 9
enum class ScaleType { major, naturalMinor, harmonicMinor, melodicMinor, dorian, phrygian, lydian, mixolydian, phrygianDominant, count };
enum class Style { standard, pop, trap, hipHop, rnb, edm, house, loFi, phonk, indie, funkBrasileiro, latin, cinematic, jazz, count };
enum class Mood { standard, melancholic, emotional, atmospheric, fun, dark, romantic, nostalgic, hopeful, mysterious, tense, epic, aggressive, count };
// FULL (MUITO RICO) adds low octave/fifth and high doublings; sessions saved before it had automatic at 3.
enum class Complexity { simple, colorful, rich, full, automatic, count };
enum class Tension { standard, soft, medium, high, count };
enum class Voicing { standard, closed, open, inverted, count };
// Order is the menu order (Ed's picks, 22/11). Sessions saved before had the old 7 timbres (see setStateInformation).
enum class Timbre { piano, pianoCinema, pianoLofi, rhodes, rhodesSuitcase, wurlitzer, fmPiano, vibraphone, houseOrgan, organ,
 strings, orchestra, brass, darkPad, junoPad, atmosPad, synthwave, supersaw, sawStab, count };
enum class ChordKind { triad, seventh, ninth, add9, sus2, sus4, dominant7, sixth, minorMajor7, sixthNinth, eleventh, count };

struct Chord { int degree{};int root{};ChordKind kind{ChordKind::triad};std::array<int,maxNotes> notes{};int noteCount{3};int velocity{88};std::string name;int quality{-1}; };
// Chromatic chords (any root, outside the scale) carry a quality index; diatonic chords keep quality -1.
constexpr int chromaticQualityCount=15;
const char* chromaticQualitySuffix(int quality);
struct GenerationSettings {int key{};ScaleType scale{ScaleType::naturalMinor};Style style{Style::standard};Mood mood{Mood::standard};Complexity complexity{Complexity::automatic};Tension tension{Tension::standard};Voicing voicing{Voicing::standard};};
struct Progression {GenerationSettings settings{};std::array<Chord,maxChords> chords{};int chordCount{4};uint64_t identity{};};

class HarmonyEngine {
public:
 static std::vector<int> scaleIntervals(ScaleType);static std::array<int,7> scaleNotes(int,ScaleType);
 static Chord buildChord(const GenerationSettings&,int,ChordKind,const Chord* previous=nullptr,int position=0);
 static Chord buildChromaticChord(const GenerationSettings&,int root,int quality,const Chord* previous=nullptr,int position=0);
 // Rebuilds a chord with new voice leading, keeping whether it is diatonic or chromatic.
 static Chord rebuild(const GenerationSettings&,const Chord&,const Chord* previous=nullptr,int position=0);
 // Resolved complexity (AUTOMÁTICO picks by style and mood).
 static Complexity resolve(const GenerationSettings&);
 static Progression transpose(const Progression&,int);static bool valid(const Progression&);static bool nameMatches(const Chord&);static int movement(const Chord&,const Chord&);
};
class ProgressionGenerator {
public:
 explicit ProgressionGenerator(uint32_t seed=std::random_device{}());
 Progression generate(const GenerationSettings&,const Progression* current=nullptr,const std::array<bool,maxChords>& locked={},int chordCount=4);
 Progression variation(const Progression&,const std::array<bool,maxChords>& locked={});
 Progression continueToEight(const Progression&);
 Progression regenerateChord(const Progression&,int);
private:
 ChordKind chooseKind(const GenerationSettings&,int,int);uint64_t nextIdentity();uint64_t signature(const Progression&)const;bool recentlyUsed(uint64_t)const;void remember(const Progression&);
 std::mt19937 rng;uint64_t counter{1},variationSerial{};std::array<int,maxChords>lastDegrees{-1,-1,-1,-1,-1,-1,-1,-1};std::deque<uint64_t>recentProgressions;std::array<std::deque<uint64_t>,maxChords>recentChordChoices;std::array<size_t,maxChords>chordCursor{};
};
const char* scaleName(ScaleType);const char* styleName(Style);const char* moodName(Mood);const char* complexityName(Complexity);const char* tensionName(Tension);const char* voicingName(Voicing);const char* timbreName(Timbre);
}
