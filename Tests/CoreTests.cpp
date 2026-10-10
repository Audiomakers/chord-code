#include "Core/MidiFile.h"
#include "Core/Music.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <set>

using namespace ccp;

static uint32_t be32(const std::vector<uint8_t>& b, size_t p) {
    return ((uint32_t) b[p] << 24) | ((uint32_t) b[p + 1] << 16) | ((uint32_t) b[p + 2] << 8) | b[p + 3];
}

static uint64_t signature(const Progression& p) {
    uint64_t result = (uint64_t) p.chordCount;
    for (int i = 0; i < p.chordCount; ++i)
        result = result * 131 + (uint64_t) p.chords[(size_t) i].degree * 17 + (uint64_t) p.chords[(size_t) i].kind;
    return result;
}

static bool same(const Progression& a, const Progression& b) {
    if (a.chordCount != b.chordCount) return false;
    for (int i = 0; i < a.chordCount; ++i) {
        const auto& x = a.chords[(size_t) i];
        const auto& y = b.chords[(size_t) i];
        if (x.degree != y.degree || x.kind != y.kind || x.noteCount != y.noteCount || x.name != y.name) return false;
        for (int n = 0; n < x.noteCount; ++n) if (x.notes[(size_t) n] != y.notes[(size_t) n]) return false;
    }
    return true;
}

static int countStatus(const std::vector<uint8_t>& bytes, uint8_t status) {
    int result = 0;
    for (size_t i = 22; i + 2 < bytes.size(); ++i) if (bytes[i] == status) ++result;
    return result;
}

int main() {
    for (int key = 0; key < 12; ++key) {
        for (int si = 0; si < (int) ScaleType::count; ++si) {
            auto notes = HarmonyEngine::scaleNotes(key, (ScaleType) si);
            assert(std::set<int>(notes.begin(), notes.end()).size() == 7);
            for (int degree = 0; degree < 7; ++degree) {
                for (int kind = 0; kind < (int) ChordKind::count; ++kind) {
                    GenerationSettings settings;
                    settings.key = key;
                    settings.scale = (ScaleType) si;
                    settings.style = Style::jazz;
                    settings.mood = Mood::romantic;
                    settings.complexity = Complexity::rich;
                    settings.voicing = (Voicing) (kind % (int) Voicing::count);
                    auto chord = HarmonyEngine::buildChord(settings, degree, (ChordKind) kind, nullptr, degree);
                    assert(chord.noteCount >= 3 && chord.noteCount <= 5);
                    assert(!chord.name.empty());
                    for (int i = 0; i < chord.noteCount; ++i) {
                        assert(chord.notes[(size_t) i] >= 0 && chord.notes[(size_t) i] <= 127);
                        if (i) assert(chord.notes[(size_t) i] > chord.notes[(size_t) (i - 1)]);
                    }
                }
            }
        }
    }

    GenerationSettings base;
    base.key = 9;
    base.scale = ScaleType::naturalMinor;
    base.style = Style::trap;
    base.mood = Mood::dark;
    base.complexity = Complexity::automatic;
    ProgressionGenerator a(42), b(42);
    for (int i = 0; i < 32; ++i) {
        auto p = a.generate(base), q = b.generate(base);
        assert(same(p, q));
        assert(HarmonyEngine::valid(p));
        std::set<int> degrees;
        for (int c = 0; c < 4; ++c) degrees.insert(p.chords[(size_t) c].degree);
        assert(degrees.size() == 4);
    }

    ProgressionGenerator generator(7);
    std::set<uint64_t> recent;
    for (int i = 0; i < 40; ++i) {
        auto p = generator.generate(base);
        assert(HarmonyEngine::valid(p));
        assert(recent.insert(signature(p)).second);
    }

    auto p = generator.generate(base);
    std::array<bool, maxChords> locked {};
    locked[0] = true;
    locked[2] = true;
    auto lockedVariation = generator.variation(p, locked);
    assert(lockedVariation.chords[0].notes == p.chords[0].notes);
    assert(lockedVariation.chords[2].notes == p.chords[2].notes);
    assert(!same(lockedVariation, p));

    auto extended = generator.continueToEight(p);
    assert(extended.chordCount == 8 && HarmonyEngine::valid(extended));
    for (int i = 0; i < 4; ++i) assert(extended.chords[(size_t) i].notes == p.chords[(size_t) i].notes);
    for (int group = 0; group < 2; ++group) {
        std::set<int> degrees;
        for (int i = 0; i < 4; ++i) degrees.insert(extended.chords[(size_t) (group * 4 + i)].degree);
        assert(degrees.size() == 4);
    }
    auto halfBar = generator.generate(base, &p, {}, 2);
    assert(halfBar.chordCount == 2 && HarmonyEngine::valid(halfBar));

    std::set<uint64_t> chordVariations;
    auto cursor = p;
    for (int i = 0; i < 12; ++i) {
        auto next = generator.regenerateChord(cursor, 1);
        uint64_t choice = (uint64_t) next.chords[1].degree * 100 + (uint64_t) next.chords[1].kind;
        assert(chordVariations.insert(choice).second);
        cursor = next;
    }

    MidiOptions options;
    options.bpm = 128;
    options.beatsPerChord = 4.0;
    options.velocity = 100;
    auto midi = MidiFile::bytes(extended, options);
    assert(midi.size() > 40);
    assert(std::string(midi.begin(), midi.begin() + 4) == "MThd");
    assert(be32(midi, 4) == 6);
    assert((midi[8] << 8 | midi[9]) == 0); // MIDI format 0: one track, avoiding duplicate FL pattern lanes.
    assert((midi[10] << 8 | midi[11]) == 1);
    assert((midi[12] << 8 | midi[13]) == MidiFile::ppq);
    int expected = 0;
    for (int i = 0; i < extended.chordCount; ++i) expected += extended.chords[(size_t) i].noteCount;
    assert(countStatus(midi, 0x90) == expected);
    assert(countStatus(midi, 0x80) == expected);

    auto bass = MidiFile::basslineBytes(extended, options);
    assert(countStatus(bass, 0x90) == extended.chordCount);
    options.rhythm = MidiRhythm::humanize;
    auto humanized = MidiFile::bytes(extended, options);
    options.rhythm = MidiRhythm::strum;
    auto strummed = MidiFile::bytes(extended, options);
    options.rhythm = MidiRhythm::arpeggio;
    options.style = Style::house;
    auto arpeggio = MidiFile::bytes(extended, options);
    assert(countStatus(arpeggio, 0x90) == expected);
    assert(midi != humanized && humanized != strummed && strummed != arpeggio);

    std::array<int, 7> pop {}, jazz {};
    ProgressionGenerator popGenerator(11), jazzGenerator(11);
    GenerationSettings ps, js;
    ps.style = Style::pop; ps.mood = Mood::fun; ps.complexity = Complexity::simple;
    js.style = Style::jazz; js.mood = Mood::romantic; js.complexity = Complexity::rich;
    for (int i = 0; i < 120; ++i) {
        auto x = popGenerator.generate(ps), y = jazzGenerator.generate(js);
        for (int c = 0; c < 4; ++c) { ++pop[(size_t) x.chords[(size_t) c].degree]; ++jazz[(size_t) y.chords[(size_t) c].degree]; }
    }
    assert(pop != jazz);
    std::cout << "Chord Code core: all tests passed\n";
}
