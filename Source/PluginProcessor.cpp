#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
juce::String utf(const char* s) { return juce::String::fromUTF8(s); }

template <typename Enum, typename NameFn>
juce::StringArray names(NameFn fn) {
    juce::StringArray result;
    for (int i = 0; i < (int) Enum::count; ++i)
        result.add(utf(fn((Enum) i)));
    return result;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordCodeProProcessor::layout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterInt>("key", "Tom", 0, 11, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Escala", names<ccp::ScaleType>(ccp::scaleName), 1));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("style", "Estilo", names<ccp::Style>(ccp::styleName), (int) ccp::Style::standard));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("mood", "Vibe", names<ccp::Mood>(ccp::moodName), (int) ccp::Mood::standard));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("complexity", "Complexidade", names<ccp::Complexity>(ccp::complexityName), (int) ccp::Complexity::automatic));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("tension", "Tensão", names<ccp::Tension>(ccp::tensionName), (int) ccp::Tension::standard));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("timbre", "Timbre", names<ccp::Timbre>(ccp::timbreName), (int) ccp::Timbre::piano));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("voicing", "Voicing", names<ccp::Voicing>(ccp::voicingName), (int) ccp::Voicing::standard));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("rhythm", "Ritmo MIDI", juce::StringArray { "RETO", "HUMANIZE", "STRUM", "ARPEJO" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>("length", "Comprimento", juce::StringArray { "1 TEMPO", "2 TEMPOS", "4 TEMPOS" }, 1));
    p.push_back(std::make_unique<juce::AudioParameterInt>("velocity", "Volume", 0, 100, 100));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("previewGain", utf("Volume de prévia"), 0.0f, 1.0f, 0.68f));
    p.push_back(std::make_unique<juce::AudioParameterBool>("bpmSync", "Sincronizar BPM", true));
    p.push_back(std::make_unique<juce::AudioParameterInt>("manualBpm", "BPM manual", 40, 240, 120));
    return { p.begin(), p.end() };
}

ChordCodeProProcessor::ChordCodeProProcessor()
    : AudioProcessor(BusesProperties().withOutput(utf("Saída"), juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ChordCodeProState", layout()), generator() {
    slots[0] = generator.generate(settingsFromParams());
    slots[1] = slots[0];
    persistProgression();
}

ChordCodeProProcessor::~ChordCodeProProcessor() {
    command.store(-1);
    synth.allNotesOff();
}

ccp::GenerationSettings ChordCodeProProcessor::settingsFromParams() const {
    ccp::GenerationSettings s;
    s.key = (int) *apvts.getRawParameterValue("key");
    s.scale = (ccp::ScaleType) (int) *apvts.getRawParameterValue("scale");
    s.style = (ccp::Style) (int) *apvts.getRawParameterValue("style");
    s.mood = (ccp::Mood) (int) *apvts.getRawParameterValue("mood");
    s.complexity = (ccp::Complexity) (int) *apvts.getRawParameterValue("complexity");
    s.tension = (ccp::Tension) (int) *apvts.getRawParameterValue("tension");
    s.voicing = (ccp::Voicing) (int) *apvts.getRawParameterValue("voicing");
    return s;
}

std::array<bool, ccp::maxChords> ChordCodeProProcessor::lockSnapshot() const {
    std::array<bool, ccp::maxChords> result {};
    for (int i = 0; i < ccp::maxChords; ++i)
        result[(size_t) i] = locks[(size_t) i].load();
    return result;
}

ccp::MidiOptions ChordCodeProProcessor::midiOptions() const {
    ccp::MidiOptions options;
    options.bpm = juce::jlimit(40, 240, (int) std::lround(hostBpm()));
    static constexpr double beats[] { 1.0, 2.0, 4.0 };
    const int lengthChoice = juce::jlimit(0, 2, (int) *apvts.getRawParameterValue("length"));
    options.beatsPerChord = beats[lengthChoice];
    const int volume = juce::jlimit(0, 100, (int) *apvts.getRawParameterValue("velocity"));
    options.velocity = juce::jlimit(1, 127, juce::roundToInt((float) volume * 1.27f));
    options.rhythm = (ccp::MidiRhythm) (int) *apvts.getRawParameterValue("rhythm");
    options.style = (ccp::Style) (int) *apvts.getRawParameterValue("style");
    return options;
}

void ChordCodeProProcessor::prepareToPlay(double sampleRate, int maximumBlockSize) {
    sr = sampleRate > 1.0 ? sampleRate : 44100.0;
    synth.prepare(sr, maximumBlockSize);
    outputGain = *apvts.getRawParameterValue("previewGain") * *apvts.getRawParameterValue("velocity") / 100.0f;
    stopPreview();
    setPreviewTimbreIfNeeded();
}

void ChordCodeProProcessor::releaseResources() {
    command.store(-1);
    synth.allNotesOff(false);
    playing = -1;
    releaseSamples = 0;
    active.store(-1);
}

bool ChordCodeProProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

ccp::Progression ChordCodeProProcessor::getProgression() const {
    return slots[(size_t) activeSlot.load(std::memory_order_acquire)];
}

void ChordCodeProProcessor::publish(const ccp::Progression& p, bool addHistory) {
    stopPreview();
    if (addHistory) {
        const auto old = getProgression();
        if (old.identity != 0 && old.identity != p.identity) {
            HistoryEntry entry;
            entry.progression = old;
            entry.rhythm = (int) *apvts.getRawParameterValue("rhythm");
            entry.length = (int) *apvts.getRawParameterValue("length");
            entry.velocity = (int) *apvts.getRawParameterValue("velocity");
            history.insert(history.begin(), entry);
            if (history.size() > 10)
                history.resize(10);
        }
    }
    const int next = 1 - activeSlot.load(std::memory_order_relaxed);
    slots[(size_t) next] = p;
    activeSlot.store(next, std::memory_order_release);
    persistProgression();
}

void ChordCodeProProcessor::generate() {
    const auto current = getProgression();
    publish(generator.generate(settingsFromParams(), &current, lockSnapshot(), current.chordCount));
}

void ChordCodeProProcessor::variation() {
    publish(generator.variation(getProgression(), lockSnapshot()));
}

void ChordCodeProProcessor::continueToEight() {
    setChordCount(8);
}

void ChordCodeProProcessor::reduceToFour() {
    setChordCount(4);
}

void ChordCodeProProcessor::setChordCount(int requested) {
    const int count = requested <= 2 ? 2 : (requested >= 8 ? 8 : 4);
    auto p = getProgression();
    if (p.chordCount != count) {
        if (count < p.chordCount) {
            p.chordCount = count;
            p.identity += 1;
            for (int i = count; i < ccp::maxChords; ++i) locks[(size_t) i].store(false);
            publish(p);
        } else if (count == 8) {
            publish(generator.continueToEight(p));
        } else {
            std::array<bool, ccp::maxChords> keep {};
            for (int i = 0; i < p.chordCount; ++i) keep[(size_t) i] = true;
            publish(generator.generate(settingsFromParams(), &p, keep, count));
        }
    }
}

void ChordCodeProProcessor::regenerateChord(int i) {
    const auto p = getProgression();
    if (i >= 0 && i < p.chordCount && !isLocked(i))
        publish(generator.regenerateChord(p, i));
}

std::vector<ccp::Chord> ChordCodeProProcessor::chordChoices(int position) const {
    const auto progression = getProgression();
    if (position < 0 || position >= progression.chordCount)
        return {};

    const int groupStart = (position / 4) * 4;
    std::array<bool, 7> usedDegrees {};
    for (int i = groupStart; i < std::min(groupStart + 4, progression.chordCount); ++i)
        if (i != position)
            usedDegrees[(size_t) progression.chords[(size_t) i].degree] = true;

    const ccp::Chord* previous = position > 0 ? &progression.chords[(size_t) (position - 1)] : nullptr;
    std::vector<ccp::Chord> choices;
    for (int degree = 0; degree < 7; ++degree) {
        if (usedDegrees[(size_t) degree])
            continue;
        for (int kind = 0; kind < (int) ccp::ChordKind::count; ++kind) {
            auto candidate = ccp::HarmonyEngine::buildChord(
                progression.settings, degree, (ccp::ChordKind) kind, previous, position);
            if (candidate.noteCount >= 3 && candidate.noteCount <= 5 && !candidate.name.empty())
                choices.push_back(std::move(candidate));
        }
    }
    return choices;
}

std::vector<ccp::Chord> ChordCodeProProcessor::allChordChoices(int position) const {
    const auto progression = getProgression();
    if (position < 0 || position >= progression.chordCount)
        return {};
    const ccp::Chord* previous = position > 0 ? &progression.chords[(size_t) (position - 1)] : nullptr;
    std::vector<ccp::Chord> choices;
    for (int root = 0; root < 12; ++root)
        for (int quality = 0; quality < ccp::chromaticQualityCount; ++quality)
            choices.push_back(ccp::HarmonyEngine::buildChromaticChord(progression.settings, root, quality, previous, position));
    return choices;
}

void ChordCodeProProcessor::chooseChord(int position, const ccp::Chord& selected) {
    const auto current = getProgression();
    if (position < 0 || position >= current.chordCount)
        return;

    auto updated = current;
    const ccp::Chord* previous = position > 0 ? &updated.chords[(size_t) (position - 1)] : nullptr;
    updated.chords[(size_t) position] = ccp::HarmonyEngine::rebuild(updated.settings, selected, previous, position);
    for (int i = position + 1; i < updated.chordCount; ++i)
        updated.chords[(size_t) i] = ccp::HarmonyEngine::rebuild(
            updated.settings, updated.chords[(size_t) i], &updated.chords[(size_t) (i - 1)], i);
    ++updated.identity;
    publish(updated);
}

void ChordCodeProProcessor::auditionChord(int position, const ccp::Chord& chord) {
    if (position < 0 || position >= ccp::maxChords)
        return;
    previewDegree.store(chord.degree, std::memory_order_relaxed);
    previewKind.store((int) chord.kind, std::memory_order_relaxed);
    previewRoot.store(chord.root, std::memory_order_relaxed);
    previewQuality.store(chord.quality, std::memory_order_relaxed);
    command.store(20 + position, std::memory_order_release);
}

void ChordCodeProProcessor::setKey(int k) {
    publish(ccp::HarmonyEngine::transpose(getProgression(), juce::jlimit(0, 11, k)));
}

void ChordCodeProProcessor::rebuildForSettings() {
    const auto current = getProgression();
    publish(generator.generate(settingsFromParams(), &current, lockSnapshot(), current.chordCount));
}

void ChordCodeProProcessor::setChoice(const char* id, int value) {
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
}

void ChordCodeProProcessor::randomize() {
    auto& r = juce::Random::getSystemRandom();
    setChoice("key", r.nextInt(12));
    setChoice("scale", r.nextInt((int) ccp::ScaleType::count));
    setChoice("style", r.nextInt((int) ccp::Style::count));
    setChoice("mood", r.nextInt((int) ccp::Mood::count));
    setChoice("complexity", r.nextInt((int) ccp::Complexity::count));
    setChoice("tension", r.nextInt((int) ccp::Tension::count));
    generate(); // Timbre, voicing, rhythm and length intentionally remain unchanged.
}

void ChordCodeProProcessor::toggleLock(int i) {
    if (i >= 0 && i < ccp::maxChords)
        locks[(size_t) i].store(!locks[(size_t) i].load());
    persistProgression();
}

bool ChordCodeProProcessor::isLocked(int i) const {
    return i >= 0 && i < ccp::maxChords && locks[(size_t) i].load();
}

bool ChordCodeProProcessor::restoreHistory(int index) {
    if (index < 0 || index >= (int) history.size())
        return false;
    auto selected = history[(size_t) index];
    history.erase(history.begin() + index);
    const auto& s = selected.progression.settings;
    setChoice("key", s.key);
    setChoice("scale", (int) s.scale);
    setChoice("style", (int) s.style);
    setChoice("mood", (int) s.mood);
    setChoice("complexity", (int) s.complexity);
    setChoice("tension", (int) s.tension);
    setChoice("voicing", (int) s.voicing);
    setChoice("rhythm", selected.rhythm);
    setChoice("length", selected.length);
    setChoice("velocity", selected.velocity);
    publish(selected.progression);
    return true;
}

int ChordCodeProProcessor::historySize() const { return (int) history.size(); }

juce::String ChordCodeProProcessor::historyLabel(int index) const {
    if (index < 0 || index >= (int) history.size())
        return {};
    const auto& p = history[(size_t) index].progression;
    juce::String text;
    for (int i = 0; i < p.chordCount; ++i) {
        if (i > 0) text << "  ·  ";
        text << utf(p.chords[(size_t) i].name.c_str());
    }
    return text;
}

double ChordCodeProProcessor::hostBpm() const {
    const bool sync = *apvts.getRawParameterValue("bpmSync") > 0.5f;
    if (sync)
        if (auto* p = getPlayHead())
            if (auto pos = p->getPosition())
                if (auto bpm = pos->getBpm())
                    if (*bpm >= 20.0 && *bpm <= 400.0)
                        return *bpm;
    return (double) *apvts.getRawParameterValue("manualBpm");
}

void ChordCodeProProcessor::scheduleChord(juce::MidiBuffer& m, const ccp::Chord& c, int chordIndex,
                                          int64_t chordStart, int64_t chordDuration,
                                          int64_t blockStart, int64_t blockEnd, ccp::MidiRhythm rhythm,
                                          double sampleRate) {
    const int volume = juce::jlimit(0, 100, (int) *apvts.getRawParameterValue("velocity"));
    const int baseVelocity = juce::jlimit(1, 127, juce::roundToInt((float) volume * 1.27f));
    for (int i = 0; i < c.noteCount; ++i) {
        const int note = juce::jlimit(0, 127, c.notes[(size_t) i]);
        int64_t offset = 0;
        int velocity = baseVelocity;
        if (rhythm == ccp::MidiRhythm::humanize) {
            offset = (int64_t) std::llround(sampleRate * (0.012 + 0.003 * (double) ((chordIndex * 17 + i * 23) % 11)));
            velocity = juce::jlimit(1, 127, velocity + ((chordIndex * 11 + i * 7) % 13) - 6);
        } else if (rhythm == ccp::MidiRhythm::strum) {
            offset = (int64_t) std::llround(sampleRate * 0.027 * (double) i);
        } else if (rhythm == ccp::MidiRhythm::arpeggio) {
            offset = i * juce::jmax<int64_t>(1, chordDuration / juce::jmax(1, c.noteCount));
        }
        offset = juce::jmin(offset, juce::jmax<int64_t>(0, chordDuration - 2));
        const int64_t noteOn = chordStart + offset;
        int64_t noteOff = chordStart + chordDuration - 1;
        if (rhythm == ccp::MidiRhythm::arpeggio) {
            const int64_t step = juce::jmax<int64_t>(1, chordDuration / juce::jmax(1, c.noteCount));
            noteOff = juce::jmin(noteOff, noteOn + juce::jmax<int64_t>(1, step * 4 / 5));
        }
        if (noteOn >= blockStart && noteOn < blockEnd)
            m.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) velocity), (int) (noteOn - blockStart));
        if (noteOff >= blockStart && noteOff < blockEnd)
            m.addEvent(juce::MidiMessage::noteOff(1, note), (int) (noteOff - blockStart));
    }
}

void ChordCodeProProcessor::setPreviewTimbreIfNeeded() {
    const int t = (int) *apvts.getRawParameterValue("timbre");
    if (t != currentTimbre) {
        synth.setTimbre((ccp::Timbre) t);
        currentTimbre = t;
    }
}

void ChordCodeProProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& incoming) {
    b.clear();
    setPreviewTimbreIfNeeded();
    juce::MidiBuffer events;
    events.addEvents(incoming, 0, b.getNumSamples(), 0);
    const int cmd = command.exchange(0);
    auto p = getProgression();

    if (cmd == -1) {
        synth.allNotesOff();
        playing = -1;
        releaseSamples = (int64_t) std::llround(sr * 0.72);
        auditionMode = false;
        customAudition = false;
    } else if (cmd >= 20) {
        synth.allNotesOff();
        releaseSamples = 0;
        playing = juce::jlimit(0, juce::jmax(0, p.chordCount - 1), cmd - 20);
        const auto* previous = playing > 0 ? &p.chords[(size_t) (playing - 1)] : nullptr;
        ccp::Chord requested;
        requested.degree = previewDegree.load(std::memory_order_relaxed);
        requested.kind = (ccp::ChordKind) previewKind.load(std::memory_order_relaxed);
        requested.root = previewRoot.load(std::memory_order_relaxed);
        requested.quality = previewQuality.load(std::memory_order_relaxed);
        customPreviewChord = ccp::HarmonyEngine::rebuild(p.settings, requested, previous, playing);
        customAudition = true;
        playheadSamples = 0;
        active.store(playing);
        auditionMode = true;
    } else if (cmd >= 10) {
        synth.allNotesOff();
        releaseSamples = 0;
        playing = juce::jlimit(0, juce::jmax(0, p.chordCount - 1), cmd - 10);
        customAudition = false;
        playheadSamples = 0;
        active.store(playing);
        auditionMode = true;
    } else if (cmd == 1) {
        synth.allNotesOff();
        releaseSamples = 0;
        playing = 0;
        customAudition = false;
        playheadSamples = 0;
        active.store(0);
        auditionMode = false;
    }

    if (playing >= 0 && playing < p.chordCount) {
        const auto options = midiOptions();
        const int64_t chordSamples = juce::jmax<int64_t>(1, (int64_t) std::llround(sr * 60.0 / options.bpm * options.beatsPerChord));
        const int64_t auditionSamples = juce::jmax<int64_t>(1, (int64_t) std::llround(sr * juce::jmax(0.45, 60.0 / options.bpm)));
        const int64_t blockEnd = playheadSamples + b.getNumSamples();
        if (auditionMode) {
            const auto& auditionedChord = customAudition ? customPreviewChord : p.chords[(size_t) playing];
            scheduleChord(events, auditionedChord, playing, 0, auditionSamples,
                          playheadSamples, blockEnd, options.rhythm, sr);
            if (blockEnd >= auditionSamples) {
                playing = -1;
                auditionMode = false;
            }
        } else {
            const int64_t total = chordSamples * p.chordCount;
            const int first = juce::jlimit(0, p.chordCount - 1, (int) (playheadSamples / chordSamples));
            const int last = juce::jlimit(0, p.chordCount - 1, (int) ((juce::jmin(blockEnd, total) - 1) / chordSamples));
            for (int i = first; i <= last; ++i) {
                scheduleChord(events, p.chords[(size_t) i], i, (int64_t) i * chordSamples, chordSamples,
                              playheadSamples, blockEnd, options.rhythm, sr);
            }
            if (playheadSamples < total) active.store(last);
            if (blockEnd >= total) { playing = -1; }
        }
        playheadSamples = blockEnd;
    }

    synth.render(b, events);
    const float volume = juce::jlimit(0.0f, 1.0f, *apvts.getRawParameterValue("velocity") / 100.0f);
    const float targetGain = *apvts.getRawParameterValue("previewGain") * volume;
    b.applyGainRamp(0, b.getNumSamples(), outputGain, targetGain);
    outputGain = targetGain;
    if (playing < 0) {
        releaseSamples = juce::jmax<int64_t>(0, releaseSamples - b.getNumSamples());
        if (releaseSamples == 0) active.store(-1);
    }
}

bool ChordCodeProProcessor::renderWav(juce::MemoryBlock& destination) {
    const auto p = getProgression();
    if (p.chordCount <= 0) return false;
    const auto options = midiOptions();
    constexpr double rate = 44100.0;
    constexpr int blockSize = 512;
    const int64_t chordSamples = juce::jmax<int64_t>(1, (int64_t) std::llround(rate * 60.0 / options.bpm * options.beatsPerChord));
    const int64_t loop = chordSamples * p.chordCount;

    PreviewSynth offline;
    offline.prepare(rate, blockSize);
    offline.setTimbre((ccp::Timbre) (int) *apvts.getRawParameterValue("timbre"));
    juce::AudioBuffer<float> result(2, (int) loop), chunk(2, blockSize);
    result.clear();
    for (int64_t position = 0; position < loop * 2; position += blockSize) {
        const int count = (int) juce::jmin<int64_t>(blockSize, loop * 2 - position);
        juce::MidiBuffer events;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < p.chordCount; ++i)
                scheduleChord(events, p.chords[(size_t) i], i, pass * loop + i * chordSamples, chordSamples,
                              position, position + count, options.rhythm, rate);
        chunk.setSize(2, count, false, false, true);
        chunk.clear();
        offline.render(chunk, events);
        // Keep the second pass only.
        const int64_t from = juce::jmax(position, loop);
        if (position + count > loop)
            for (int c = 0; c < 2; ++c)
                result.copyFrom(c, (int) (from - loop), chunk, c, (int) (from - position), (int) (position + count - from));
    }

    // Same level for every timbre and volume setting: peak at -1 dBFS.
    const float peak = result.getMagnitude(0, result.getNumSamples());
    if (!std::isfinite(peak) || peak < 1.0e-5f) return false;
    result.applyGain(0.891f / peak);

    destination.reset();
    auto stream = std::make_unique<juce::MemoryOutputStream>(destination, false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release();   // owned by the writer now
    const bool written = writer->writeFromAudioSampleBuffer(result, 0, result.getNumSamples());
    writer.reset();     // flushes the header into destination
    return written && destination.getSize() > 44;
}

void ChordCodeProProcessor::playSequence() { command.store(1); }
void ChordCodeProProcessor::stopPreview() { command.store(-1); }
void ChordCodeProProcessor::audition(int i) { command.store(10 + juce::jlimit(0, ccp::maxChords - 1, i)); }

void ChordCodeProProcessor::persistProgression() {
    const auto p = getProgression();
    juce::String s;
    s << "3|" << p.chordCount << "," << p.settings.key << "," << (int) p.settings.scale << "," << (int) p.settings.style
      << "," << (int) p.settings.mood << "," << (int) p.settings.complexity << "," << (int) p.settings.tension
      << "," << (int) p.settings.voicing << "," << (juce::int64) p.identity << "|";
    for (int i = 0; i < p.chordCount; ++i) {
        const auto& c = p.chords[(size_t) i];
        s << c.degree << "," << c.root << "," << (int) c.kind << "," << c.noteCount << "," << c.velocity << ","
          << juce::String(c.name) << ",";
        for (int n = 0; n < c.noteCount; ++n)
            s << c.notes[(size_t) n] << (n + 1 < c.noteCount ? "." : "");
        s << "," << c.quality << ";";
    }
    apvts.state.setProperty("progressionV2", s, nullptr);
    juce::String lockText;
    for (int i = 0; i < ccp::maxChords; ++i) lockText << (isLocked(i) ? "1" : "0");
    apvts.state.setProperty("locksV2", lockText, nullptr);
}

bool ChordCodeProProcessor::restoreProgression() {
    auto text = apvts.state.getProperty("progressionV2").toString();
    if (text.isEmpty()) {
        const auto legacy = apvts.state.getProperty("progressionV1").toString();
        const auto oldSections = juce::StringArray::fromTokens(legacy, "|", "");
        if (oldSections.size() != 3 || oldSections[0] != "1") return false;
        const auto oldMeta = juce::StringArray::fromTokens(oldSections[1], ",", "");
        const auto oldChords = juce::StringArray::fromTokens(oldSections[2], ";", "");
        if (oldMeta.size() < 6 || oldChords.size() != 4) return false;
        ccp::Progression migrated;
        migrated.chordCount = 4;
        migrated.settings.key = oldMeta[0].getIntValue();
        migrated.settings.scale = (ccp::ScaleType) oldMeta[1].getIntValue();
        { const int old = oldMeta[2].getIntValue(); migrated.settings.style = (ccp::Style) (old == 13 ? 0 : old + 1); }
        { const int old = oldMeta[3].getIntValue(); migrated.settings.mood = (ccp::Mood) (old == 12 ? 0 : old + 1); }
        migrated.settings.complexity = (ccp::Complexity) oldMeta[4].getIntValue();
        migrated.identity = (uint64_t) oldMeta[5].getLargeIntValue();
        for (int i = 0; i < 4; ++i) {
            const auto fields = juce::StringArray::fromTokens(oldChords[i], ",", "");
            if (fields.size() < 7) return false;
            auto& c = migrated.chords[(size_t) i];
            c.degree = fields[0].getIntValue(); c.root = fields[1].getIntValue(); c.kind = (ccp::ChordKind) fields[2].getIntValue();
            c.noteCount = juce::jlimit(1, 5, fields[3].getIntValue()); c.velocity = fields[4].getIntValue(); c.name = fields[5].toStdString();
            const auto notes = juce::StringArray::fromTokens(fields[6], ".", "");
            if (notes.size() != c.noteCount) return false;
            for (int n = 0; n < c.noteCount; ++n) c.notes[(size_t) n] = notes[n].getIntValue();
        }
        if (!ccp::HarmonyEngine::valid(migrated)) return false;
        slots[0] = slots[1] = migrated;
        activeSlot.store(0);
        persistProgression();
        return true;
    }
    const auto sections = juce::StringArray::fromTokens(text, "|", "");
    if (sections.size() != 3 || (sections[0] != "2" && sections[0] != "3"))
        return false;
    const auto meta = juce::StringArray::fromTokens(sections[1], ",", "");
    if (meta.size() < 9)
        return false;
    ccp::Progression p;
    p.chordCount = juce::jlimit(2, ccp::maxChords, meta[0].getIntValue());
    p.settings.key = meta[1].getIntValue();
    p.settings.scale = (ccp::ScaleType) meta[2].getIntValue();
    if (sections[0] == "2") {
        const int oldStyle = meta[3].getIntValue(), oldMood = meta[4].getIntValue();
        p.settings.style = (ccp::Style) (oldStyle == 13 ? 0 : oldStyle + 1);
        p.settings.mood = (ccp::Mood) (oldMood == 12 ? 0 : oldMood + 1);
    } else {
        p.settings.style = (ccp::Style) meta[3].getIntValue();
        p.settings.mood = (ccp::Mood) meta[4].getIntValue();
    }
    p.settings.complexity = (ccp::Complexity) meta[5].getIntValue();
    p.settings.tension = (ccp::Tension) meta[6].getIntValue();
    p.settings.voicing = (ccp::Voicing) meta[7].getIntValue();
    p.identity = (uint64_t) meta[8].getLargeIntValue();
    const auto chordTokens = juce::StringArray::fromTokens(sections[2], ";", "");
    if (chordTokens.size() != p.chordCount)
        return false;
    for (int i = 0; i < p.chordCount; ++i) {
        const auto fields = juce::StringArray::fromTokens(chordTokens[i], ",", "");
        if (fields.size() < 7) return false;
        auto& c = p.chords[(size_t) i];
        c.degree = fields[0].getIntValue();
        c.root = fields[1].getIntValue();
        c.kind = (ccp::ChordKind) fields[2].getIntValue();
        c.noteCount = juce::jlimit(1, 5, fields[3].getIntValue());
        c.velocity = juce::jlimit(1, 127, fields[4].getIntValue());
        c.name = fields[5].toStdString();
        const auto notes = juce::StringArray::fromTokens(fields[6], ".", "");
        if (notes.size() != c.noteCount) return false;
        for (int n = 0; n < c.noteCount; ++n) c.notes[(size_t) n] = notes[n].getIntValue();
        c.quality = fields.size() >= 8 ? juce::jlimit(-1, ccp::chromaticQualityCount - 1, fields[7].getIntValue()) : -1;
    }
    if (!ccp::HarmonyEngine::valid(p))
        return false;
    slots[0] = slots[1] = p;
    activeSlot.store(0);
    setChoice("style", (int) p.settings.style);
    setChoice("mood", (int) p.settings.mood);
    const auto lockText = apvts.state.getProperty("locksV2").toString();
    for (int i = 0; i < ccp::maxChords; ++i)
        locks[(size_t) i].store(i < lockText.length() && lockText[i] == '1');
    return true;
}

void ChordCodeProProcessor::getStateInformation(juce::MemoryBlock& d) {
    persistProgression();
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, d);
}

void ChordCodeProProcessor::setStateInformation(const void* d, int n) {
    if (auto xml = getXmlFromBinary(d, n))
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            if (!restoreProgression()) {
                slots[0] = generator.generate(settingsFromParams());
                slots[1] = slots[0];
            }
        }
}

juce::AudioProcessorEditor* ChordCodeProProcessor::createEditor() { return new ChordCodeProEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordCodeProProcessor; }
