#include "PluginEditor.h"
#include "ChordCodeTheme.h"
#include "Core/MidiFile.h"
#include <cmath>

using namespace ChordCodeTheme;

namespace {
// The 100% window is 1120 px wide; the layout is drawn in design units and scaled.
constexpr float defaultWindowScale = 1120.0f / (float) designWidth;
constexpr float minWindowScale = 0.6f, maxWindowScale = 2.0f;
// Popups were designed for the original 960 px wide window.
constexpr float panelScaleAt100 = 1120.0f / 960.0f;
constexpr const char* windowScaleKey = "windowScale";
constexpr int cardPageWidth = 1324;
juce::String utf(const char* text) { return juce::String::fromUTF8(text); }

template <typename Enum, typename Fn>
void addNames(juce::ComboBox& box, Fn fn) {
    for (int i = 0; i < (int) Enum::count; ++i)
        box.addItem(AMUI::displayCase(utf(fn((Enum) i))), i + 1);   // values in normal case (Audio Makers UI)
}

void label(juce::Graphics& g, const char* text, int x, int y, int width) {
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(16.0f));
    g.drawText(utf(text), x, y, width, 20, juce::Justification::centredLeft);
}

juce::String romanNumeral(const ccp::Chord& chord, int key) {
    static const char* numerals[] { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* chromatic[] { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    auto numeral = chord.quality >= 0 ? juce::String(chromatic[(size_t) (((chord.root - key) % 12 + 12) % 12)])
                                      : juce::String(numerals[(size_t) juce::jlimit(0, 6, chord.degree)]);
    auto suffix = utf(chord.name.c_str()).substring(1);
    if (suffix.startsWithChar('#') || suffix.startsWithChar('b')) suffix = suffix.substring(1);
    if (suffix.startsWith("dim") || suffix.startsWith(utf("°")) || suffix.startsWith("m7b5"))
        return numeral.toLowerCase() + utf("°");
    if (suffix.startsWithChar('m') && !suffix.startsWith("maj"))
        return numeral.toLowerCase();
    return numeral;
}

juce::String chordNotes(const ccp::Chord& chord) {
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::StringArray notes;
    for (int i = 0; i < chord.noteCount; ++i) {
        const auto name = juce::String(names[(size_t) ((chord.notes[(size_t) i] % 12 + 12) % 12)]);
        if (!notes.contains(name)) notes.add(name);
    }
    return notes.joinIntoString(utf("  ·  "));
}

// ---- MIDI export helpers -------------------------------------------------------------
// Never hand a UTF-8 std::string to std::filesystem on Windows: it is decoded with the ANSI
// code page, so any accented user folder (C:\Users\João\...) turns into a path that doesn't
// exist and the write silently fails. juce::File uses the wide (UTF-16) Win32 API.
bool isPlainAscii(const juce::String& text) {
    for (auto c : text) if (c > 127) return false;
    return true;
}

bool writeMidiBytes(const juce::File& file, const std::vector<uint8_t>& bytes, juce::String& error) {
    const auto dir = file.getParentDirectory();
    if (const auto made = dir.createDirectory(); made.failed()) {
        error = made.getErrorMessage();
        return false;
    }
    if (!file.replaceWithData(bytes.data(), bytes.size())) {
        // replaceWithData writes a temp file and renames it; some antivirus / OneDrive setups
        // block the rename. Try a plain direct write before giving up.
        juce::FileOutputStream out(file);
        if (!out.openedOk()) {
            error = out.getStatus().failed() ? out.getStatus().getErrorMessage() : juce::String("sem permissão de escrita");
            return false;
        }
        out.setPosition(0);
        out.truncate();
        out.write(bytes.data(), bytes.size());
        out.flush();
        if (out.getStatus().failed()) { error = out.getStatus().getErrorMessage(); return false; }
    }
    if (!file.existsAsFile() || file.getSize() != (juce::int64) bytes.size()) {
        error = utf("o arquivo não ficou no disco (antivírus ou Acesso Controlado a Pastas?)");
        return false;
    }
    return true;
}

// Where the drag file is written, best first. Plain-ASCII folders go first because a few
// hosts mishandle dropped paths with accents; protected folders (Documents, Music) are last
// because Windows "Controlled folder access" blocks DAWs that aren't on its allow list.
juce::Array<juce::File> dragFolders() {
    const auto sub = [](const juce::File& base) { return base.getChildFile("Audio Makers").getChildFile("Chord Code"); };
    juce::Array<juce::File> candidates;
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::tempDirectory)));
   #if JUCE_WINDOWS
    if (const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {}); local.isNotEmpty())
        candidates.add(sub(juce::File(local)).getChildFile("MIDI"));
    if (const auto pub = juce::SystemStats::getEnvironmentVariable("PUBLIC", {}); pub.isNotEmpty())
        candidates.add(sub(juce::File(pub)).getChildFile("MIDI"));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)).getChildFile("MIDI"));
   #endif
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)).getChildFile("MIDI"));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userMusicDirectory)));

    juce::Array<juce::File> ascii, other;
    for (const auto& dir : candidates) {
        if (dir.getFullPathName().length() < 4 || ascii.contains(dir) || other.contains(dir)) continue;
        (isPlainAscii(dir.getFullPathName()) ? ascii : other).add(dir);
    }
    ascii.addArray(other);
    return ascii;
}

juce::String chordFileStem(const ccp::Progression& progression) {
    juce::StringArray names;
    for (int i = 0; i < progression.chordCount; ++i) {
        auto name = utf(progression.chords[(size_t) i].name.c_str());
        for (auto invalid : juce::String("/\\:*?\"<>| ")) name = name.replaceCharacter(invalid, '_');
        juce::String safe;
        for (auto c : name) safe << (c > 31 && c < 127 ? juce::String::charToString(c) : juce::String("_"));
        names.add(safe);
    }
    auto stem = names.joinIntoString("-").substring(0, 100).trimCharactersAtEnd("._ ");
    return stem.isEmpty() ? juce::String("Chord Code") : stem;
}

// Right-click chord picker. "Campo harmônico" groups the scale's chords by degree;
// "Todos os acordes" offers every root with every chord quality. Hover auditions.
class ChordPickerPanel final : public juce::PopupMenu::CustomComponent {
public:
    ChordPickerPanel(int slotIndex, int keyRoot, std::array<int, 7> scale, ccp::Chord currentChord,
                     std::vector<ccp::Chord> diatonicOptions, std::vector<ccp::Chord> allOptions,
                     std::shared_ptr<bool> showAll, std::function<void(const ccp::Chord&)> onHover,
                     std::shared_ptr<int> chosenIndex)
        : juce::PopupMenu::CustomComponent(false), slot(slotIndex), key(keyRoot), scaleNotes(scale),
          current(std::move(currentChord)), diatonic(std::move(diatonicOptions)), all(std::move(allOptions)),
          allMode(std::move(showAll)), hoverAction(std::move(onHover)), chosen(std::move(chosenIndex)) {
        for (size_t i = 0; i < diatonic.size(); ++i) {
            if (rows.empty() || rows.back().degree != diatonic[i].degree) rows.push_back({ diatonic[i].degree, {}, {} });
            rows.back().items.push_back((int) i);
        }
        selectedRoot = ((current.root % 12) + 12) % 12;
        layout();
    }

    void getIdealSize(int& width, int& height) override { width = panelWidth; height = totalHeight; }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(surface));
        g.fillRect(bounds);

        auto header = bounds.removeFromTop((float) headerHeight).reduced(16.0f, 0.0f);
        g.setColour(juce::Colour(textSecondary));
        g.setFont(labelFont(12.0f));
        g.drawText(utf("TROCAR ACORDE ") + juce::String(slot + 1).paddedLeft('0', 2), header.removeFromTop(30.0f).withTrimmedTop(10.0f),
                   juce::Justification::centredLeft);
        g.setColour(juce::Colour(textPrimary));
        g.setFont(font(19.0f, true));
        g.drawText(utf(current.name.c_str()), header.removeFromTop(24.0f), juce::Justification::centredLeft);
        g.setColour(juce::Colour(textSecondary));
        g.setFont(font(13.0f));
        g.drawText(chordNotes(current), header.withY(header.getY() - 24.0f).withHeight(24.0f), juce::Justification::centredRight);

        // Mode toggle.
        const auto toggle = toggleArea();
        g.setColour(juce::Colour(0xff0a0c0f));
        g.fillRoundedRectangle(toggle, 10.0f);
        g.setColour(juce::Colour(border));
        g.drawRoundedRectangle(toggle.reduced(0.5f), 10.0f, 1.0f);
        for (int i = 0; i < 2; ++i) {
            const auto segment = toggleSegment(i);
            const bool active = (i == 1) == *allMode;
            if (active) { g.setGradientFill(accentGradient(segment)); g.fillRoundedRectangle(segment, 8.0f); }
            else if (segment.contains(mouse)) { g.setColour(juce::Colour(surfaceHover)); g.fillRoundedRectangle(segment, 8.0f); }
            g.setColour(active ? juce::Colour(0xff160904) : juce::Colour(textSecondary));
            g.setFont(labelFont(12.0f));
            g.drawText(i == 0 ? utf("CAMPO HARMÔNICO") : utf("TODOS OS ACORDES"), segment, juce::Justification::centred);
        }

        if (*allMode) paintAll(g); else paintDiatonic(g);

        auto footer = getLocalBounds().toFloat().removeFromBottom((float) footerHeight).reduced(16.0f, 0.0f);
        g.setColour(juce::Colour(textQuiet));
        g.setFont(font(12.5f));
        const auto hint = hovered >= 0 ? utf("Ouvindo ") + utf(option(hovered).name.c_str()) + utf("  ·  clique para usar")
                                       : utf("Passe o mouse para ouvir  ·  clique para trocar");
        g.drawText(hint, footer, juce::Justification::centredLeft);
    }

    void mouseMove(const juce::MouseEvent& e) override { mouse = e.position; setHovered(chipAt(e.position)); repaint(); }
    void mouseDrag(const juce::MouseEvent& e) override { mouse = e.position; setHovered(chipAt(e.position)); }
    void mouseExit(const juce::MouseEvent&) override { mouse = { -1.0f, -1.0f }; setHovered(-1); repaint(); }
    void mouseUp(const juce::MouseEvent& e) override {
        for (int i = 0; i < 2; ++i)
            if (toggleSegment(i).contains(e.position)) { *allMode = i == 1; hovered = -1; repaint(); return; }
        if (*allMode)
            for (int root = 0; root < 12; ++root)
                if (rootAreas[(size_t) root].contains(e.position)) {
                    selectedRoot = root;
                    hovered = -1;
                    repaint();
                    if (hoverAction) hoverAction(all[(size_t) (root * ccp::chromaticQualityCount + previewQuality())]);
                    return;
                }
        const int index = chipAt(e.position);
        if (index < 0) return;
        *chosen = index;
        triggerMenuItem();
    }

private:
    static constexpr const char* degreeFunctions[] { "TÔNICA", "SUPERTÔNICA", "MEDIANTE", "SUBDOMINANTE",
                                                     "DOMINANTE", "SUBMEDIANTE", "SENSÍVEL" };
    static constexpr const char* noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static constexpr int chipsPerLine = 6, chipWidth = 76, chipHeight = 30, chipGap = 6;
    static constexpr int labelWidth = 104, headerHeight = 66, toggleHeight = 34, footerHeight = 34;
    static constexpr int panelWidth = 16 + labelWidth + chipsPerLine * chipWidth + (chipsPerLine - 1) * chipGap + 16;
    static constexpr int contentTop = headerHeight + 12 + toggleHeight + 12;

    struct Row { int degree; std::vector<int> items; juce::Rectangle<float> labelArea; };

    juce::Rectangle<float> toggleArea() const { return { 16.0f, (float) headerHeight + 12.0f, (float) panelWidth - 32.0f, (float) toggleHeight }; }
    juce::Rectangle<float> toggleSegment(int i) const {
        auto r = toggleArea().reduced(3.0f);
        return i == 0 ? r.removeFromLeft(r.getWidth() * 0.5f) : r.withTrimmedLeft(r.getWidth() * 0.5f);
    }

    const ccp::Chord& option(int index) const {
        return index < (int) diatonic.size() ? diatonic[(size_t) index] : all[(size_t) (index - (int) diatonic.size())];
    }

    int previewQuality() const {
        if (current.quality >= 0) return current.quality;
        const auto suffix = utf(current.name.c_str()).substring(1).trimCharactersAtStart("#b");
        return suffix.startsWithChar('m') && !suffix.startsWith("maj") ? 1 : 0;
    }

    juce::Rectangle<float> chipBounds(int line, int column, int y) const {
        return { (float) (16 + labelWidth + column * (chipWidth + chipGap)), (float) (y + line * (chipHeight + chipGap)),
                 (float) chipWidth, (float) chipHeight };
    }

    void layout() {
        chipAreas.assign(diatonic.size(), {});
        int y = contentTop;
        for (auto& row : rows) {
            row.labelArea = { 16.0f, (float) y, (float) labelWidth - 8.0f, 48.0f };
            for (size_t n = 0; n < row.items.size(); ++n)
                chipAreas[(size_t) row.items[n]] = chipBounds((int) n / chipsPerLine, (int) n % chipsPerLine, y);
            const int lines = ((int) row.items.size() + chipsPerLine - 1) / chipsPerLine;
            y += juce::jmax(48, lines * (chipHeight + chipGap) - chipGap) + 14;
        }
        const int diatonicHeight = y;

        y = contentTop;
        rootLabel = { 16.0f, (float) y, (float) labelWidth - 8.0f, 48.0f };
        for (int root = 0; root < 12; ++root) rootAreas[(size_t) root] = chipBounds(root / chipsPerLine, root % chipsPerLine, y);
        y += 2 * (chipHeight + chipGap) - chipGap + 18;
        qualityLabel = { 16.0f, (float) y, (float) labelWidth - 8.0f, 48.0f };
        for (int q = 0; q < ccp::chromaticQualityCount; ++q) qualityAreas[(size_t) q] = chipBounds(q / chipsPerLine, q % chipsPerLine, y);
        y += ((ccp::chromaticQualityCount + chipsPerLine - 1) / chipsPerLine) * (chipHeight + chipGap) - chipGap + 14;

        totalHeight = juce::jmax(diatonicHeight, y) + footerHeight - 6;
    }

    void paintChip(juce::Graphics& g, juce::Rectangle<float> chip, const juce::String& text, bool isCurrent, bool isHover,
                   bool marked = false) const {
        if (isHover) {
            g.setGradientFill(accentGradient(chip));
            g.fillRoundedRectangle(chip, 8.0f);
        } else {
            g.setColour(isCurrent ? juce::Colour(orange).withAlpha(0.12f) : juce::Colour(surfaceRaised));
            g.fillRoundedRectangle(chip, 8.0f);
            g.setColour(isCurrent ? juce::Colour(orange) : juce::Colour(border));
            g.drawRoundedRectangle(chip.reduced(0.5f), 8.0f, 1.0f);
        }
        if (marked && !isHover) {
            g.setColour(juce::Colour(orangeLight).withAlpha(0.8f));
            g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre({ chip.getCentreX(), chip.getBottom() - 5.0f }));
        }
        g.setColour(isHover ? juce::Colour(0xff160904) : isCurrent ? juce::Colour(orangeLight) : juce::Colour(textPrimary));
        g.setFont(semi(14.0));
        g.drawFittedText(text, chip.toNearestInt().reduced(6, 0), juce::Justification::centred, 1, 0.75f);
    }

    void paintRowLabel(juce::Graphics& g, juce::Rectangle<float> label, const juce::String& pillText, const juce::String& caption) const {
        if (pillText.isNotEmpty()) {
            auto pill = juce::Rectangle<float>(label.getX(), label.getY() + 2.0f, 38.0f, 24.0f);
            g.setColour(juce::Colour(orange).withAlpha(0.14f));
            g.fillRoundedRectangle(pill, 12.0f);
            g.setColour(juce::Colour(orangeLight));
            g.setFont(semi(14.0));
            g.drawText(pillText, pill, juce::Justification::centred);
        }
        g.setColour(juce::Colour(textSecondary));
        g.setFont(labelFont(10.5f));
        g.drawText(caption, label.withTrimmedTop(pillText.isNotEmpty() ? 30.0f : 6.0f).withHeight(16.0f), juce::Justification::centredLeft);
    }

    void paintDiatonic(juce::Graphics& g) const {
        for (const auto& row : rows) {
            const auto& sample = diatonic[(size_t) row.items.front()];
            const bool subtonic = row.degree == 6 && ((sample.root - key) % 12 + 12) % 12 == 10;
            paintRowLabel(g, row.labelArea, romanNumeral(sample, key),
                          utf(subtonic ? "SUBTÔNICA" : degreeFunctions[(size_t) juce::jlimit(0, 6, row.degree)]));
        }
        for (size_t i = 0; i < chipAreas.size(); ++i) {
            const auto& chord = diatonic[i];
            const bool isCurrent = current.quality < 0 && chord.degree == current.degree && chord.kind == current.kind;
            paintChip(g, chipAreas[i], utf(chord.name.c_str()), isCurrent, (int) i == hovered);
        }
    }

    void paintAll(juce::Graphics& g) const {
        paintRowLabel(g, rootLabel, {}, "FUNDAMENTAL");
        g.setColour(juce::Colour(textQuiet));
        g.setFont(font(11.5f));
        g.drawFittedText(utf("• no tom"), rootLabel.withTrimmedTop(24.0f).withHeight(16.0f).toNearestInt(),
                         juce::Justification::centredLeft, 1);
        for (int root = 0; root < 12; ++root) {
            const bool inScale = std::find(scaleNotes.begin(), scaleNotes.end(), root) != scaleNotes.end();
            paintChip(g, rootAreas[(size_t) root], noteNames[root], root == selectedRoot,
                      rootAreas[(size_t) root].contains(mouse), inScale);
        }
        paintRowLabel(g, qualityLabel, {}, "ACORDE");
        for (int q = 0; q < ccp::chromaticQualityCount; ++q) {
            const int index = (int) diatonic.size() + selectedRoot * ccp::chromaticQualityCount + q;
            const auto& chord = option(index);
            const bool isCurrent = (current.root % 12 + 12) % 12 == selectedRoot
                && (current.quality >= 0 ? current.quality == q : utf(current.name.c_str()) == utf(chord.name.c_str()));
            paintChip(g, qualityAreas[(size_t) q], utf(chord.name.c_str()), isCurrent, index == hovered);
        }
    }

    int chipAt(juce::Point<float> p) const {
        if (*allMode) {
            for (int q = 0; q < ccp::chromaticQualityCount; ++q)
                if (qualityAreas[(size_t) q].contains(p)) return (int) diatonic.size() + selectedRoot * ccp::chromaticQualityCount + q;
            return -1;
        }
        for (size_t i = 0; i < chipAreas.size(); ++i)
            if (chipAreas[i].contains(p)) return (int) i;
        return -1;
    }

    void setHovered(int index) {
        if (index == hovered) return;
        hovered = index;
        repaint();
        if (index >= 0 && hoverAction) hoverAction(option(index));
    }

    int slot {}, key {}, selectedRoot {};
    std::array<int, 7> scaleNotes {};
    ccp::Chord current;
    std::vector<ccp::Chord> diatonic, all;
    std::shared_ptr<bool> allMode;
    std::vector<Row> rows;
    std::vector<juce::Rectangle<float>> chipAreas;
    std::array<juce::Rectangle<float>, 12> rootAreas {};
    std::array<juce::Rectangle<float>, ccp::chromaticQualityCount> qualityAreas {};
    juce::Rectangle<float> rootLabel, qualityLabel;
    std::function<void(const ccp::Chord&)> hoverAction;
    std::shared_ptr<int> chosen;
    juce::Point<float> mouse { -1.0f, -1.0f };
    int hovered { -1 }, totalHeight {};
};
}

juce::Rectangle<float> SegmentedControl::itemBounds(float index) const {
    const auto track = getLocalBounds().toFloat().reduced(4.0f);
    const float itemWidth = track.getWidth() / (float) juce::jmax(1, items.size());
    return { track.getX() + index * itemWidth, track.getY(), itemWidth, track.getHeight() };
}

int SegmentedControl::indexAt(juce::Point<float> p) const {
    if (items.isEmpty() || !getLocalBounds().toFloat().contains(p)) return -1;
    const auto track = getLocalBounds().toFloat().reduced(4.0f);
    return juce::jlimit(0, items.size() - 1, (int) ((p.x - track.getX()) / (track.getWidth() / (float) items.size())));
}

void SegmentedControl::setSelected(int value) {
    selected = value;
    if (indicator < 0.0f || !isShowing()) indicator = (float) value;
    else startTimerHz(60);
    repaint();
}

void SegmentedControl::timerCallback() {
    indicator += ((float) selected - indicator) * 0.28f;
    if (std::abs(indicator - (float) selected) < 0.004f) { indicator = (float) selected; stopTimer(); }
    repaint();
}

void SegmentedControl::paint(juce::Graphics& g) {
    if (items.isEmpty()) return;
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff08090b));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(juce::Colour(borderQuiet));
    g.drawRoundedRectangle(bounds, 11.0f, 1.0f);

    if (hovered >= 0 && hovered != selected) {
        g.setColour(juce::Colour(surfaceHover));
        g.fillRoundedRectangle(itemBounds((float) hovered).reduced(1.0f), 8.0f);
    }

    // Sliding pill marks the active option and glides between choices.
    const auto pill = itemBounds(juce::jmax(0.0f, indicator));
    g.setGradientFill(accentGradient(pill));
    g.fillRoundedRectangle(pill, 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.22f), 0.0f, pill.getY(),
                                           juce::Colours::white.withAlpha(0.0f), 0.0f, pill.getCentreY(), false));
    g.fillRoundedRectangle(pill.reduced(1.5f).withHeight(pill.getHeight() * 0.5f), 7.0f);

    g.setFont(labelFont(15.5f));
    for (int i = 0; i < items.size(); ++i) {
        const float closeness = juce::jlimit(0.0f, 1.0f, 1.0f - std::abs(indicator - (float) i));
        const auto idle = juce::Colour(i == hovered ? textPrimary : textSecondary);
        g.setColour(idle.interpolatedWith(juce::Colour(0xff160904), closeness));
        g.drawFittedText(items[i], itemBounds((float) i).toNearestInt().reduced(6, 0), juce::Justification::centred, 1);
    }
}

void SegmentedControl::mouseMove(const juce::MouseEvent& e) { const int h = indexAt(e.position); if (h != hovered) { hovered = h; repaint(); } }
void SegmentedControl::mouseExit(const juce::MouseEvent&) { hovered = -1; repaint(); }
void SegmentedControl::mouseUp(const juce::MouseEvent& e) {
    const int i = indexAt(e.position);
    if (i >= 0 && i != selected) { setSelected(i); if (onSelect) onSelect(i); }
}

ChordCardComponent::ChordCardComponent() = default;

juce::Rectangle<float> ChordCardComponent::body() const { return getLocalBounds().toFloat().reduced(glowMargin); }
juce::Rectangle<float> ChordCardComponent::playArea() const { auto b = body(); return { b.getRight() - 40.0f, b.getY() + 16.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> ChordCardComponent::variationArea() const { auto b = body(); return { b.getRight() - 105.0f, b.getY() + 18.0f, 22.0f, 22.0f }; }
juce::Rectangle<float> ChordCardComponent::lockArea() const { auto b = body(); return { b.getRight() - 74.0f, b.getY() + 16.0f, 24.0f, 24.0f }; }
juce::Rectangle<float> ChordCardComponent::recordArea() const { auto b = body(); return { b.getRight() - 138.0f, b.getY() + 17.0f, 24.0f, 24.0f }; }

void ChordCardComponent::advanceAnimation() {
    const float hoverTarget = available && hover ? 1.0f : 0.0f;
    const float activeTarget = available && playing ? 1.0f : 0.0f;
    const auto ease = [](float& value, float target, float speed) {
        if (std::abs(value - target) < 0.005f) { const bool changed = std::abs(value - target) > 0.0f; value = target; return changed; }
        value += (target - value) * speed;
        return true;
    };
    const bool changed = ease(hoverLevel, hoverTarget, 0.30f) | ease(activeLevel, activeTarget, 0.22f);
    if (changed) repaint();
}

void ChordCardComponent::paint(juce::Graphics& g) {
    const auto r = body();
    const bool active = available && playing;
    const float glow = juce::jmax(activeLevel, selected && available ? 0.35f : 0.0f);
    juce::Path shape;
    shape.addRoundedRectangle(r, radiusCard);

    if (glow > 0.01f)
        drawGlow(g, shape, juce::Colour(orange).withAlpha(0.55f * glow), 18);
    const auto recordRed = juce::Colour(0xffff4d4d);
    const float pulse = 0.5f + 0.5f * std::sin((float) juce::Time::getMillisecondCounter() * 0.006f);
    if (armed)
        drawGlow(g, shape, recordRed.withAlpha(0.35f + 0.3f * pulse), 16);

    const float alpha = available ? 1.0f : 0.45f;
    const auto top = juce::Colour(surfaceRaised).interpolatedWith(juce::Colour(surfaceHover), hoverLevel);
    g.setGradientFill(raisedGradient(r, top.withMultipliedAlpha(alpha), juce::Colour(surface).withMultipliedAlpha(alpha)));
    g.fillPath(shape);

    if (activeLevel > 0.01f) {
        juce::ColourGradient warm(juce::Colour(orange).withAlpha(0.26f * activeLevel), r.getCentreX(), r.getBottom(),
                                  juce::Colour(orange).withAlpha(0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.15f, true);
        g.setGradientFill(warm);
        g.fillPath(shape);
    }

    // Top edge highlight.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f + 0.05f * hoverLevel), 0.0f, r.getY(),
                                           juce::Colours::transparentWhite, 0.0f, r.getY() + 60.0f, false));
    g.strokePath(shape, juce::PathStrokeType(1.0f), juce::AffineTransform::translation(0.0f, 0.5f));

    if (glow > 0.01f) {
        auto outline = accentGradient(r);
        for (int i = 0; i < outline.getNumColours(); ++i)
            outline.setColour(i, outline.getColour(i).withAlpha(glow));
        g.setGradientFill(outline);
        g.strokePath(shape, juce::PathStrokeType(1.0f + 0.8f * glow));
    } else {
        g.setColour(juce::Colour(border).interpolatedWith(juce::Colour(borderStrong), hoverLevel).withMultipliedAlpha(available ? 1.0f : 0.5f));
        g.strokePath(shape, juce::PathStrokeType(1.0f));
    }

    if (armed) {
        g.setColour(recordRed.withAlpha(0.6f + 0.4f * pulse));
        g.strokePath(shape, juce::PathStrokeType(2.0f));
    }

    // Slot badge.
    auto badge = juce::Rectangle<float>(r.getX() + 16.0f, r.getY() + 16.0f, 36.0f, 24.0f);
    g.setColour(active ? juce::Colour(orange).withAlpha(0.18f) : juce::Colour(surfaceRaised).withMultipliedAlpha(alpha));
    g.fillRoundedRectangle(badge, 8.0f);
    g.setColour(active ? juce::Colour(orangeLight) : juce::Colour(textSecondary).withMultipliedAlpha(alpha));
    g.setFont(semi(15.0));
    g.drawText(juce::String(slot + 1).paddedLeft('0', 2), badge, juce::Justification::centred);

    if (available) {
        // Record: capture the next chord played on the MIDI keyboard.
        const auto rec = recordArea();
        if (armed) {
            g.setColour(recordRed.withAlpha(0.18f));
            g.fillRoundedRectangle(rec.expanded(4.0f), 7.0f);
        }
        const auto iconIdle = juce::Colour(textSecondary);
        AMUI::drawIcon(g, AMUI::Icon::record, rec, armed ? recordRed.withAlpha(0.6f + 0.4f * pulse) : iconIdle);
        AMUI::drawIcon(g, AMUI::Icon::variation, variationArea().withSizeKeepingCentre(22.0f, 22.0f), iconIdle);
        if (locked) {
            g.setColour(juce::Colour(orange).withAlpha(0.16f));
            g.fillRoundedRectangle(lockArea().expanded(4.0f), 8.0f);
        }
        AMUI::drawIcon(g, locked ? AMUI::Icon::lock : AMUI::Icon::unlock, lockArea().withSizeKeepingCentre(22.0f, 22.0f),
                       locked ? juce::Colour(orange) : iconIdle);
        if (activeLevel > 0.01f)
            AMUI::drawIcon(g, AMUI::Icon::play, playArea().withSizeKeepingCentre(22.0f, 22.0f), juce::Colour(textPrimary).withAlpha(activeLevel));
    }

    // Chord name, gently lifting when hovered or playing.
    const float lift = 3.0f * juce::jmax(hoverLevel, activeLevel);
    g.setColour(juce::Colour(textPrimary).withAlpha(available ? 1.0f : 0.25f));
    g.setFont(font(54.0f, true));
    g.drawFittedText(available ? utf(chord.name.c_str()) : juce::String::fromUTF8("—"),
                     (int) r.getX() + 16, (int) (r.getY() + 62.0f - lift), (int) r.getWidth() - 32, 68,
                     juce::Justification::centred, 1, 0.82f);

    if (available) {
        const auto numeral = romanNumeral(chord, keyRoot);
        g.setFont(semi(17.0));
        const float numeralWidth = (float) juce::GlyphArrangement::getStringWidthInt(semi(17.0), numeral) + 18.0f;
        const auto notes = chordNotes(chord);
        const auto notesFont = font(16.0f, false);
        const float notesWidth = (float) juce::GlyphArrangement::getStringWidthInt(notesFont, notes);
        const float rowWidth = numeralWidth + 10.0f + notesWidth;
        auto row = juce::Rectangle<float>(rowWidth, 26.0f).withCentre({ r.getCentreX(), r.getY() + 146.0f - lift * 0.5f });
        auto pill = row.removeFromLeft(numeralWidth);
        g.setColour(juce::Colour(orange).withAlpha(0.12f + 0.10f * activeLevel));
        g.fillRoundedRectangle(pill, 8.0f);
        g.setColour(juce::Colour(orangeLight));
        g.drawText(numeral, pill, juce::Justification::centred);
        row.removeFromLeft(10.0f);
        g.setColour(juce::Colour(textSecondary));
        g.setFont(notesFont);
        g.drawText(notes, row, juce::Justification::centredLeft);
    }

    if (armed) {
        g.setColour(recordRed);
        g.setFont(labelFont(13.0f));
        g.drawFittedText(utf("TOQUE UM ACORDE\nNO TECLADO MIDI"), r.withTrimmedTop(176.0f).withHeight(48.0f).toNearestInt(),
                         juce::Justification::centred, 2);
        return;
    }

    if (showPiano) { paintPiano(g, juce::Rectangle<float>(r.getX() + 22.0f, r.getY() + 174.0f, r.getWidth() - 44.0f, 52.0f)); return; }

    // Equalizer: idle bars stay calm, playback makes them dance.
    static constexpr float heights[] { 10.0f, 18.0f, 28.0f, 40.0f, 48.0f, 40.0f, 28.0f, 18.0f, 10.0f };
    constexpr int barCount = 9;
    constexpr float barWidth = 6.0f, gap = 8.0f;
    const float startX = r.getCentreX() - ((float) barCount * barWidth + (float) (barCount - 1) * gap) * 0.5f;
    const float centreY = r.getY() + 200.0f;
    const auto idle = juce::Colour(waveform).interpolatedWith(juce::Colour(0xff59606b), hoverLevel).withMultipliedAlpha(available ? 1.0f : 0.35f);
    for (int i = 0; i < barCount; ++i) {
        float height = heights[i];
        if (activeLevel > 0.01f) {
            const float pulse = 0.5f + 0.5f * std::sin(playbackPhase * 1.15f + (float) i * 0.85f)
                                     * std::cos(playbackPhase * 0.55f - (float) i * 0.4f);
            const float animated = juce::jlimit(8.0f, 56.0f, heights[i] * (0.55f + pulse * 0.85f));
            height = juce::jmap(activeLevel, height, animated);
        }
        auto bar = juce::Rectangle<float>(startX + (float) i * (barWidth + gap), centreY - height * 0.5f, barWidth, height);
        if (activeLevel > 0.01f) {
            juce::ColourGradient fill(juce::Colour(amber), 0.0f, bar.getY(), juce::Colour(orange), 0.0f, bar.getBottom(), false);
            fill.multiplyOpacity(activeLevel);
            g.setColour(idle.withMultipliedAlpha(1.0f - activeLevel));
            g.fillRoundedRectangle(bar, barWidth * 0.5f);
            g.setGradientFill(fill);
        } else {
            g.setColour(idle);
        }
        g.fillRoundedRectangle(bar, barWidth * 0.5f);
    }
}

void ChordCardComponent::mouseUp(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) return;
    if (!available) return;
    if (variationArea().contains(e.position)) { if (onRegenerate) onRegenerate(); return; }
    if (lockArea().contains(e.position)) { if (onLock) onLock(); return; }
    if (recordArea().expanded(3.0f).contains(e.position)) { if (onRecord) onRecord(); return; }
    if ((selected || playing) && playArea().contains(e.position)) { if (onPlay) onPlay(); return; }
    if (onSelect) onSelect();
    if (onPlay) onPlay();
}

void ChordCardComponent::paintPiano(juce::Graphics& g, juce::Rectangle<float> area) const {
    // At least two octaves from the C below the lowest note, enough to hold the whole chord.
    int lowest = 127, highest = 0;
    for (int i = 0; i < chord.noteCount; ++i) { lowest = juce::jmin(lowest, chord.notes[(size_t) i]); highest = juce::jmax(highest, chord.notes[(size_t) i]); }
    if (chord.noteCount == 0) { lowest = 60; highest = 67; }
    const int start = lowest - ((lowest % 12) + 12) % 12;
    const int octaves = juce::jmax(2, (highest - start) / 12 + 1);
    const int end = start + 12 * octaves - 1;
    static constexpr bool black[] { false, true, false, true, false, false, true, false, true, false, true, false };
    const auto isOn = [this](int n) { for (int i = 0; i < chord.noteCount; ++i) if (chord.notes[(size_t) i] == n) return true; return false; };
    const float whiteWidth = area.getWidth() / (float) (7 * octaves);
    const float alpha = available ? 1.0f : 0.35f;
    const auto lit = [&](int n) {
        const auto c = n == lowest ? juce::Colour(amber) : juce::Colour(orange);
        return c.interpolatedWith(juce::Colour(orangeLight), activeLevel * 0.5f).withMultipliedAlpha(alpha);
    };
    juce::Path clip; clip.addRoundedRectangle(area, 5.0f);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(clip);
    int white = 0;
    for (int n = start; n <= end; ++n) {
        if (black[n % 12]) continue;
        auto key = juce::Rectangle<float>(area.getX() + (float) white * whiteWidth, area.getY(), whiteWidth, area.getHeight()).reduced(0.6f, 0.0f);
        g.setColour(isOn(n) ? lit(n) : juce::Colour(0xffc9ced6).interpolatedWith(juce::Colour(0xffe4e7ec), hoverLevel).withMultipliedAlpha(0.85f * alpha));
        g.fillRect(key);
        ++white;
    }
    white = 0;
    for (int n = start; n <= end; ++n) {
        if (!black[n % 12]) { ++white; continue; }
        const float x = area.getX() + (float) white * whiteWidth - whiteWidth * 0.32f;
        auto key = juce::Rectangle<float>(x, area.getY(), whiteWidth * 0.64f, area.getHeight() * 0.6f);
        g.setColour(isOn(n) ? lit(n) : juce::Colour(0xff15181d).withMultipliedAlpha(alpha));
        g.fillRoundedRectangle(key.withTrimmedTop(-3.0f), 2.0f);
    }
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(area.reduced(0.5f), 5.0f, 1.0f);
}

juce::String ChordCardComponent::getTooltip() {
    if (!available) return {};
    const auto p = getMouseXYRelative().toFloat();
    if (variationArea().contains(p)) return juce::String::fromUTF8("Troca só este acorde por outro que combine com a progressão.");
    if (lockArea().contains(p)) return juce::String::fromUTF8(locked ? "Acorde travado: GERAR e VARIAÇÃO não mexem nele. Clique para destravar."
                                                                       : "Trava este acorde: GERAR e VARIAÇÃO passam a manter ele.");
    if (recordArea().expanded(3.0f).contains(p))
        return juce::String::fromUTF8(armed ? "Esperando um acorde do teclado MIDI. Clique de novo para cancelar."
                                            : "Grava um acorde do teclado MIDI aqui: clique, toque o acorde e ele entra no lugar deste, travado.");
    if (body().contains(p)) return juce::String::fromUTF8("Clique para ouvir. Botão direito mostra outros acordes para este lugar.");
    return {};
}

void ChordCardComponent::mouseDown(const juce::MouseEvent& e) {
    if (available && e.mods.isPopupMenu() && onShowChordChoices)
        onShowChordChoices(slot);
}

void ContinueCard::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.8f);
    g.setColour(juce::Colour(over || down ? surfaceRaised : surfaceGroup));
    g.fillRoundedRectangle(r, radiusCard);
    juce::Path outline, dashed;
    outline.addRoundedRectangle(r, radiusCard);
    const float pattern[] { 6.0f, 6.0f };
    juce::PathStrokeType(1.2f).createDashedStroke(dashed, outline, pattern, 2);
    g.setColour(over ? juce::Colour(orange).withAlpha(0.8f) : juce::Colour(borderStrong).withAlpha(0.8f));
    g.fillPath(dashed);

    auto circle = juce::Rectangle<float>(52.0f, 52.0f).withCentre({ r.getCentreX(), 96.0f });
    if (over) {
        g.setGradientFill(accentGradient(circle));
        g.fillEllipse(circle);
    } else {
        g.setColour(juce::Colour(surfaceRaised));
        g.fillEllipse(circle);
        g.setColour(juce::Colour(borderStrong));
        g.drawEllipse(circle, 1.0f);
    }
    const auto plusColour = over ? juce::Colour(0xff1a0a02) : juce::Colour(textPrimary);
    if (backMode) { g.setColour(plusColour); g.drawLine(circle.getCentreX() - 9.0f, circle.getCentreY(), circle.getCentreX() + 9.0f, circle.getCentreY(), 2.4f); }
    else AMUI::drawIcon(g, AMUI::Icon::plus, circle.withSizeKeepingCentre(26.0f, 26.0f), plusColour);

    g.setColour(juce::Colour(textPrimary));
    g.setFont(labelFont(16.0f));
    g.drawFittedText(backMode ? "VOLTAR A 4" : "CONTINUAR", 8, 138, getWidth() - 16, 24,
                     juce::Justification::centred, 1);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(14.0f));
    g.drawFittedText(backMode ? "Remover 4" : "+4 acordes", 8, 162, getWidth() - 16, 20,
                     juce::Justification::centred, 1);
}

void StatusPill::setLive(bool value, float phase) {
    const float next = value ? 0.5f + 0.5f * std::sin(phase) : 0.0f;
    if (value != live || (value && std::abs(next - pulse) > 0.02f)) { live = value; pulse = next; repaint(); }
}

void StatusPill::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(live ? juce::Colour(orange).withAlpha(0.45f) : juce::Colour(border));
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
    auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ r.getX() + 20.0f, r.getCentreY() });
    if (live) {
        g.setColour(juce::Colour(orange).withAlpha(0.35f * pulse));
        g.fillEllipse(dot.expanded(4.0f + 3.0f * pulse));
    }
    g.setColour(live ? juce::Colour(orange) : juce::Colour(textQuiet));
    g.fillEllipse(dot);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(buttonFont(13.0f));
    g.drawFittedText(text, r.withTrimmedLeft(34.0f).withTrimmedRight(14.0f).toNearestInt(), juce::Justification::centredLeft, 1);
}

void HeaderPill::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const bool over = isMouseOver(true);
    const float radius = radiusControl;
    g.setColour(on ? juce::Colour(orange).withAlpha(over ? 0.22f : 0.14f) : over ? juce::Colour(surfaceHover) : juce::Colour(surface));
    g.fillRoundedRectangle(r, radius);
    g.setColour(on ? juce::Colour(orange).withAlpha(0.8f) : over ? juce::Colour(orange).withAlpha(0.6f) : juce::Colour(border));
    g.drawRoundedRectangle(r, radius, 1.0f);
    const auto ink = on ? juce::Colour(orangeLight) : juce::Colour(textSecondary);
    const auto glyph = juce::Rectangle<float>(r.getX() + 14.0f, r.getCentreY() - 10.0f, 20.0f, 20.0f);
    AMUI::drawIcon(g, icon == Icon::piano ? AMUI::Icon::piano : icon == Icon::keyboard ? AMUI::Icon::midi : AMUI::Icon::drum, glyph, ink);
    g.setColour(ink);
    g.setFont(buttonFont(13.0f));
    const bool menu = icon != Icon::piano;
    g.drawFittedText(text, r.withTrimmedLeft(44.0f).withTrimmedRight(menu ? 34.0f : 12.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    if (menu) AMUI::drawIcon(g, AMUI::Icon::chevron, { r.getRight() - 30.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f }, ink);
}

ChordCodeProEditor::ChordCodeProEditor(ChordCodeProProcessor& p) : AudioProcessorEditor(&p), pro(p) {
    setLookAndFeel(&laf);
    setOpaque(true);
    addAndMakeVisible(content);
    content.setInterceptsMouseClicks(false, true);
    // Resizable from the corner, keeping the layout's proportions. New windows open at the
    // default size chosen in Configurações > Janela (saved per computer).
    constrainer.setFixedAspectRatio((double) designWidth / (double) designHeight);
    constrainer.setSizeLimits(juce::roundToInt(designWidth * defaultWindowScale * minWindowScale),
                              juce::roundToInt(designHeight * defaultWindowScale * minWindowScale),
                              juce::roundToInt(designWidth * defaultWindowScale * maxWindowScale),
                              juce::roundToInt(designHeight * defaultWindowScale * maxWindowScale));
    setConstrainer(&constrainer);
    setResizable(true, true);
    applyWindowScale((float) license->getPreference(windowScaleKey, 1.0));

    const char* notes[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) key.addItem(notes[i], i + 1);
    addNames<ccp::ScaleType>(scale, ccp::scaleName);
    addNames<ccp::Style>(style, ccp::styleName);
    addNames<ccp::Mood>(mood, ccp::moodName);
    addNames<ccp::Complexity>(complexity, ccp::complexityName);
    addNames<ccp::Tension>(tension, ccp::tensionName);
    addNames<ccp::Timbre>(timbre, ccp::timbreName);
    addNames<ccp::Voicing>(voicingParameter, ccp::voicingName);
    rhythmParameter.addItemList({ "RETO", "HUMANIZE", "STRUM", "ARPEJO" }, 1);
    length.addItemList({ "1/4", "2/4", "4/4" }, 1);
    groove.addItemList({ "Igual", "Longo-curto", "Curto-longo" }, 1);
    groove.setJustificationType(juce::Justification::centredLeft);
    groove.setRepaintsOnMouseActivity(true);
    groove.setTooltip(utf("Alterna a duração dos acordes em pares: um mais longo e outro mais curto."));
    grooveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "groove", groove);
    content.addAndMakeVisible(groove);
    groove.onChange = [this] { pro.restartIfPlaying(); updateStatus(); };

    std::array<juce::ComboBox*, 10> boxes { &key, &scale, &style, &mood, &complexity, &tension, &timbre,
                                            &voicingParameter, &rhythmParameter, &length };
    const char* ids[] { "key", "scale", "style", "mood", "complexity", "tension", "timbre", "voicing", "rhythm", "length" };
    for (size_t i = 0; i < boxes.size(); ++i) {
        boxes[i]->setJustificationType(juce::Justification::centredLeft);
        comboAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, ids[i], *boxes[i]);
        boxes[i]->setRepaintsOnMouseActivity(true);
        if (i < 7 || i == 9) content.addAndMakeVisible(*boxes[i]);
    }

    voicing.setItems({ utf("PADRÃO"), "FECHADO", "ABERTO", "INVERTER" });
    rhythm.setItems({ "RETO", "HUMANIZE", "STRUM", "ARPEJO" });
    content.addAndMakeVisible(voicing);
    content.addAndMakeVisible(rhythm);
    voicing.onSelect = [this](int i) { voicingParameter.setSelectedItemIndex(i, juce::sendNotificationSync); };
    rhythm.onSelect = [this](int i) { rhythmParameter.setSelectedItemIndex(i, juce::sendNotificationSync); };

    velocity.setSliderStyle(juce::Slider::LinearHorizontal);
    velocity.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 48);
    velocity.setRange(0, 100, 1);
    velocity.setNumDecimalPlacesToDisplay(0);
    velocity.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(surface));
    velocity.setColour(juce::Slider::textBoxTextColourId, juce::Colour(textPrimary));
    velocity.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    velocity.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    content.addAndMakeVisible(velocity);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "velocity", velocity);

    key.onChange = [this] { if (!ignore) { pro.setKey(key.getSelectedItemIndex()); refresh(); } };
    for (auto* box : { &scale, &style, &mood, &complexity, &tension, &voicingParameter })
        box->onChange = [this] { if (!ignore) { pro.rebuildForSettings(); refresh(); } };
    timbre.onChange = [this] { pro.restartIfPlaying(); updateTimbreTooltip(); };
    timbrePrev.setComponentID("prev"); timbreNext.setComponentID("next");
    for (auto* b : { &timbrePrev, &timbreNext }) {
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        b->setTooltip(utf("Timbre anterior / próximo"));
        content.addAndMakeVisible(b);
    }
    const auto stepTimbre = [this](int d) {
        const int n = timbre.getNumItems();
        if (n > 0) timbre.setSelectedItemIndex(((timbre.getSelectedItemIndex() + d) % n + n) % n, juce::sendNotificationSync);
    };
    timbrePrev.onClick = [stepTimbre] { stepTimbre(-1); };
    timbreNext.onClick = [stepTimbre] { stepTimbre(1); };

    content.addAndMakeVisible(cardsViewport);
    cardsViewport.setInterceptsMouseClicks(false, true);
    for (int i = 0; i < 8; ++i) {
        cardsViewport.addAndMakeVisible(cards[(size_t) i]);
        cards[(size_t) i].setSlot(i);
        cards[(size_t) i].onSelect = [this, i] { selectCard(i); };
        cards[(size_t) i].onPlay = [this, i] { pro.audition(i); };
        cards[(size_t) i].onRegenerate = [this, i] { pro.regenerateChord(i); refresh(); };
        cards[(size_t) i].onLock = [this, i] { pro.toggleLock(i); refresh(); };
        cards[(size_t) i].onShowChordChoices = [this](int slot) { showChordChoices(slot); };
        cards[(size_t) i].onRecord = [this, i] { pro.armCapture(i); };
    }
    content.addAndMakeVisible(continueCard);
    content.addAndMakeVisible(status);
    content.addAndMakeVisible(keyModePill);
    content.addAndMakeVisible(beatPill);
    keyModePill.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    beatPill.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    keyModePill.onClick = [this] { showKeyModeMenu(); };
    content.addAndMakeVisible(pianoPill);
    pianoPill.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    pianoPill.onClick = [this] { setShowPiano(!showPiano); };
    setShowPiano(license->getPreference("showPiano", 1.0) > 0.5);
    beatPill.onClick = [this] { showBeatMenu(); };

    for (auto* button : { (juce::Button*) &settings, (juce::Button*) &play, (juce::Button*) &variation,
                          (juce::Button*) &random, (juce::Button*) &generate, (juce::Button*) &history,
                          (juce::Button*) &drag, (juce::Button*) &dragWav, (juce::Button*) &save })
        content.addAndMakeVisible(*button);
    settings.setComponentID("settings"); settings.setTitle("Configurações");
    play.setComponentID("play"); variation.setComponentID("variation"); random.setComponentID("random");
    generate.setComponentID("generate"); history.setComponentID("history"); drag.setComponentID("drag"); save.setComponentID("save");
    dragWav.setComponentID("dragWav");
    drag.setButtonText("ARRASTAR MIDI");
    dragWav.setButtonText("ARRASTAR WAV");

    settings.onClick = [this] { showSettings(); };
    play.onClick = [this] { if (pro.isSequencePlaying() || pro.activeChord() >= 0) pro.stopPreview(); else pro.playSequence(); };
    variation.onClick = [this] { pro.variation(); refresh(); };
    random.onClick = [this] {
        ignore = true;
        pro.randomize();
        ignore = false;
        updateFromSettings();
        refresh();
    };
    generate.onClick = [this] { pro.generate(); refresh(); };
    history.onClick = [this] { showHistory(); };
    continueCard.onClick = [this] {
        if (std::abs(slidePosition - slideTarget) > 0.01f) return;
        if (pageOffset == 4) {
            pendingChordCount = 4;
            animateToPage(0);
        } else {
            pro.continueToEight();
            refresh();
            animateToPage(4);
        }
    };
    drag.dragAction = [this] { exportDrag(ExportKind::midi); };
    drag.clickAction = [this] { revealForDrag(ExportKind::midi); };
    dragWav.dragAction = [this] { exportDrag(ExportKind::wav); };
    dragWav.clickAction = [this] { revealForDrag(ExportKind::wav); };
    save.onClick = [this] { showSaveMenu(); };

    setTooltips();

    addChildComponent(overlay);
    overlay.onDismiss = [this] { content.setEnabled(true); };
    showAllChords = std::make_shared<bool>((bool) pro.apvts.state.getProperty("chordPickerAllChords", false));
    license->addChangeListener(this);

    refresh();
    startTimerHz(30);
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<ChordCodeProEditor>(this)] {
        if (safe != nullptr) safe->checkLicense();
    });

    // Silent update check each time the editor opens; failures (offline) show nothing.
    if (const auto fake = juce::SystemStats::getEnvironmentVariable("CHORDCODE_FAKE_UPDATE", {}); fake.isNotEmpty())
        setUpdateAvailable(true);
    else
        backgroundUpdateCheck.start(JucePlugin_VersionString,
            [safe = juce::Component::SafePointer<ChordCodeProEditor>(this)](UpdateChecker::Result result) {
                if (safe != nullptr && result.succeeded) safe->setUpdateAvailable(result.updateAvailable);
            });
}

void ChordCodeProEditor::updateTimbreTooltip() {
    static const char* const descriptions[] {
        "PIANO: piano acústico de verdade, gravado, com duas camadas de força (toque leve soa suave, forte soa mais brilhante). O timbre principal, combina com tudo.",
        "RHODES: piano elétrico com tremolo, quente e macio (o mesmo do Melody Code). Ótimo para R&B, lo-fi, neo soul e trap melódico.",
        "SAW: synth de onda serra com filtro que abre no ataque. Brilhante e moderno, bom para trap, EDM e pop.",
        "ORGAN: órgão com caixa giratória (estilo Hammond). Bom para gospel, soul, house e funk.",
        "PAD ATMOSFÉRICO: som longo e suave que segura o acorde, cria clima. Bom para intros, ambient e trap melódico.",
        "VIOLINO: cordas segurando os acordes. Clima de cinema, emoção e drill.",
        "FLAUTA: flauta leve e arejada. Boa para lo-fi, R&B e beats calmos."
    };
    const int i = juce::jlimit(0, (int) std::size(descriptions) - 1, timbre.getSelectedItemIndex());
    timbre.setTooltip(juce::String::fromUTF8(descriptions[i]) + "\n\n" + utf("Só muda o som do preview e do WAV. O MIDI é o mesmo."));
}

void ChordCodeProEditor::setShowPiano(bool value) {
    showPiano = value;
    for (auto& card : cards) card.setShowPiano(value);
    pianoPill.setState("PIANO", value);
    license->setPreference("showPiano", value ? 1.0 : 0.0);
}

void ChordCodeProEditor::setTooltips() {
    key.setTooltip(utf("Tom da progressão: a nota que soa como \"casa\"."));
    scale.setTooltip(utf("Escala do tom. MAIOR soa alegre e aberto, MENOR NATURAL mais triste. As outras dão cores diferentes (DÓRICA jazzy, FRÍGIA sombria, LÍDIA sonhadora)."));
    style.setTooltip(utf("Gênero: muda quais acordes e caminhos de progressão são escolhidos."));
    mood.setTooltip(utf("Clima da progressão: puxa para acordes mais tristes, alegres, sombrios, épicos..."));
    complexity.setTooltip(utf("Quantas notas cada acorde tem. SIMPLES: tríades. COLORIDO: com nona. RICO: sétimas e nonas. AUTOMÁTICO escolhe pelo estilo."));
    tension.setTooltip(utf("Quanto a progressão sai do tom: SUAVE fica no seguro, ALTA usa acordes emprestados e mais surpresa."));
    voicing.setTooltip(utf("Como as notas se espalham. FECHADO: juntas. ABERTO: espalhadas, soa maior. INVERTER: muda a nota de baixo."));
    rhythm.setTooltip(utf("Como o acorde é tocado. RETO: todas juntas. HUMANIZE: pequenas variações, como uma pessoa tocando. STRUM: uma nota depois da outra, como violão. ARPEJO: notas em sequência."));
    length.setTooltip(utf("Duração de cada acorde: 1/4, 2/4 ou um compasso inteiro (4/4)."));
    velocity.setTooltip(utf("Força das notas no MIDI e volume do preview."));
    play.setTooltip(utf("Toca a progressão inteira. Com uma batida de referência ligada, fica em loop até parar."));
    variation.setTooltip(utf("Mantém o clima e muda alguns acordes (os travados ficam)."));
    random.setTooltip(utf("Sorteia tom, escala, estilo e clima, e gera uma progressão nova."));
    generate.setTooltip(utf("Gera uma progressão nova com as configurações escolhidas. Acordes travados ficam."));
    history.setTooltip(utf("Volta para uma progressão anterior."));
    drag.setTooltip(utf("Arraste para a DAW para levar o MIDI da progressão. Clique para abrir a pasta com o arquivo."));
    dragWav.setTooltip(utf("Arraste para a DAW para levar o áudio (WAV) com o timbre escolhido. Clique para abrir a pasta com o arquivo."));
    save.setTooltip(utf("Salva a progressão como MIDI ou WAV numa pasta que você escolher."));
    keyModePill.setTooltip(utf("Como o teclado MIDI toca. NOTAS: normal. ACORDES DO TOM: cada tecla vira um acorde do tom. TECLAS BRANCAS: as teclas brancas tocam os acordes do tom, em qualquer tom."));
    pianoPill.setTooltip(utf("Mostra um piano em cada acorde com as notas que ele toca (a nota mais grave em amarelo). Clique para ligar ou desligar."));
    beatPill.setTooltip(utf("Batida de referência para ouvir os acordes com ritmo. Só toca no preview, não vai para o MIDI nem para o WAV."));
    settings.setTooltip(utf("Configurações: tamanho da janela, licença e atualizações."));
    updateTimbreTooltip();
}

ChordCodeProEditor::~ChordCodeProEditor() {
    stopTimer();
    license->removeChangeListener(this);
    updateChecker.cancelAndWait();
    backgroundUpdateCheck.cancelAndWait();
    chooser.reset();
    for (auto& card : cards) { card.onSelect = {}; card.onPlay = {}; card.onRegenerate = {}; card.onLock = {}; card.onShowChordChoices = {}; card.onRecord = {}; }
    pro.armCapture(-1);
    pro.stopPreview();
    setLookAndFeel(nullptr);
}

void ChordCodeProEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(background));
    g.addTransform(juce::AffineTransform::scale(uiScale()));
    const auto full = juce::Rectangle<float>(0.0f, 0.0f, (float) designWidth, (float) designHeight);
    g.setGradientFill(juce::ColourGradient(juce::Colour(backgroundTop), 0.0f, 0.0f,
                                           juce::Colour(background), 0.0f, full.getBottom(), false));
    g.fillRoundedRectangle(full, radiusWindow);

    // Ambient warm light: one glow behind the brand, one rising behind the chord cards.
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.10f), 120.0f, 40.0f,
                                           juce::Colour(orange).withAlpha(0.0f), 620.0f, 300.0f, true));
    g.fillRoundedRectangle(full, radiusWindow);
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.06f), 768.0f, 600.0f,
                                           juce::Colour(orange).withAlpha(0.0f), 768.0f, 180.0f, true));
    g.fillRoundedRectangle(full, radiusWindow);

    // Subtle dot grid texture in the header.
    g.setColour(juce::Colours::white.withAlpha(0.025f));
    for (int y = 18; y < 88; y += 14)
        for (int x = 560; x < 1340; x += 14)
            g.fillRect((float) x, (float) y, 1.5f, 1.5f);

    auto window = full.reduced(0.75f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(borderStrong), 0.0f, 0.0f,
                                           juce::Colour(borderQuiet), 0.0f, full.getBottom(), false));
    g.drawRoundedRectangle(window, radiusWindow, 1.5f);

    // Brand mark: a small equalizer glyph followed by the wordmark.
    auto mark = juce::Rectangle<float>(34.0f, 26.0f, 38.0f, 38.0f);
    g.setGradientFill(accentGradient(mark));
    g.fillRoundedRectangle(mark, 10.0f);
    AMUI::drawIcon(g, AMUI::Icon::bars, mark.withSizeKeepingCentre(26.0f, 26.0f), juce::Colour(0xff1a0a02));

    const auto logoFont = font(32.0f, true).withExtraKerningFactor(0.01f);
    g.setFont(logoFont);
    g.setColour(juce::Colour(textPrimary));
    g.drawText("CHORD", 86, 20, 170, 50, juce::Justification::centredLeft);
    const int codeX = 86 + juce::GlyphArrangement::getStringWidthInt(logoFont, "CHORD") + 8;
    const auto codeArea = juce::Rectangle<float>((float) codeX, 20.0f, 130.0f, 50.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange), codeArea.getX(), 0.0f,
                                           juce::Colour(amber), codeArea.getRight(), 0.0f, false));
    g.drawText("CODE", codeArea, juce::Justification::centredLeft);

    {
        juce::ColourGradient line(juce::Colour(divider).withAlpha(0.0f), 30.0f, 0.0f,
                                  juce::Colour(divider).withAlpha(0.0f), 1506.0f, 0.0f, false);
        line.addColour(0.15, juce::Colour(borderStrong).withAlpha(0.6f));
        line.addColour(0.85, juce::Colour(borderStrong).withAlpha(0.6f));
        g.setGradientFill(line);
        g.fillRect(30.0f, 90.0f, 1476.0f, 1.0f);
    }

    label(g, "TOM", 34, 116, 150); label(g, "ESCALA", 396, 116, 150);
    label(g, "ESTILO", 768, 116, 150); label(g, "VIBE", 1130, 116, 150);
    label(g, "COMPLEXIDADE", 34, 218, 180); label(g, "TENSÃO", 504, 218, 150); label(g, "TIMBRE", 964, 218, 150);

    const auto drawGroup = [&g](juce::Rectangle<float> bounds) {
        drawRaisedPanel(g, bounds, radiusPanel, juce::Colour(borderQuiet));
    };
    drawGroup({ 34.5f, 608.5f, 460.0f, 118.0f });
    drawGroup({ 504.5f, 608.5f, 450.0f, 118.0f });
    drawGroup({ 964.5f, 608.5f, 536.0f, 118.0f });
    label(g, "FORMA DO ACORDE", 55, 625, 220); label(g, "RITMO", 525, 625, 180);
    label(g, "DURAÇÃO", 981, 625, 120); label(g, "ALTERNÂNCIA", 1113, 625, 200); label(g, "VOLUME", 1327, 625, 160);

}

void ChordCodeProEditor::resized() {
    overlay.setBounds(getLocalBounds());
    if (auto* panel = overlay.getPanel()) overlay.setPanelScale(panelScaleFor(*panel));
    content.setBounds(0, 0, designWidth, designHeight);
    content.setTransform(juce::AffineTransform::scale(uiScale()));
    settings.setBounds(1450, 24, 50, 50);
    key.setBounds(34, 140, 320, 50); scale.setBounds(396, 140, 330, 50);
    style.setBounds(768, 140, 330, 50); mood.setBounds(1130, 140, 370, 50);
    complexity.setBounds(34, 242, 460, 50); tension.setBounds(504, 242, 450, 50); timbre.setBounds(964, 242, 536, 50);
    timbreNext.setBounds(timbre.getRight() - 52 - 38, timbre.getY() + 7, 36, 36);
    timbrePrev.setBounds(timbre.getRight() - 52 - 78, timbre.getY() + 7, 36, 36);

    const int margin = (int) ChordCardComponent::glowMargin;
    cardsViewport.setBounds(34 - margin, 332 - margin, 1288 + margin * 2, 252 + margin * 2);
    // header, right-aligned: beat, keyboard, piano, summary, settings
    beatPill.setBounds(470, 27, 236, 44);
    keyModePill.setBounds(718, 27, 236, 44);
    pianoPill.setBounds(966, 27, 120, 44);
    status.setBounds(1098, 27, 340, 44);
    layoutChordPages();
    continueCard.setBounds(1358, 332, 142, 252);

    voicing.setBounds(53, 657, 416, 46);
    rhythm.setBounds(523, 657, 404, 46);
    length.setBounds(981, 655, 120, 48);
    groove.setBounds(1113, 655, 202, 48);
    velocity.setBounds(1323, 647, 166, 64);

    play.setBounds(34, 762, 160, 76); variation.setBounds(204, 762, 166, 76);
    random.setBounds(380, 762, 168, 76); generate.setBounds(558, 762, 240, 76);
    history.setBounds(808, 762, 156, 76); drag.setBounds(974, 762, 181, 76);
    dragWav.setBounds(1165, 762, 181, 76); save.setBounds(1356, 762, 144, 76);
}

float ChordCodeProEditor::uiScale() const {
    return getWidth() > 0 ? (float) getWidth() / (float) designWidth : defaultWindowScale;
}

void ChordCodeProEditor::applyWindowScale(float scale) {
    const float s = defaultWindowScale * juce::jlimit(minWindowScale, maxWindowScale, std::isfinite(scale) ? scale : 1.0f);
    setSize(juce::roundToInt((float) designWidth * s), juce::roundToInt((float) designHeight * s));
}

// Popups keep their proportion to the window, shrinking if needed so they always fit.
float ChordCodeProEditor::panelScaleFor(const juce::Component& panel) const {
    const float proportional = panelScaleAt100 * uiScale() / defaultWindowScale;
    const float fitHeight = (float) getHeight() * 0.94f / (float) juce::jmax(1, panel.getHeight());
    const float fitWidth = (float) getWidth() * 0.94f / (float) juce::jmax(1, panel.getWidth());
    return juce::jmin(proportional, fitHeight, fitWidth);
}

void ChordCodeProEditor::setUpdateAvailable(bool available) {
    updateAvailable = available;
    settings.getProperties().set("updateDot", available);
    settings.repaint();
}

void ChordCodeProEditor::timerCallback() {
    if (updateAvailable) settings.repaint();   // keeps the yellow dot pulsing
    const int now = pro.activeChord();
    animationPhase += 0.28f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;
    if (pro.applyPendingCapture()) refresh();
    const int armed = pro.armedSlot();
    updatePills();
    for (int i = 0; i < 8; ++i) {
        cards[(size_t) i].setArmed(i == armed);
        if (i == armed) cards[(size_t) i].repaint();
        cards[(size_t) i].setPlaying(i == now);
        cards[(size_t) i].setPlaybackPhase(animationPhase);
        cards[(size_t) i].advanceAnimation();
    }
    status.setLive(now >= 0, animationPhase);
    if (++licenseTicks >= 30 * 60) { licenseTicks = 0; license->revalidate(); }
    if (now != lastPlaying) {
        lastPlaying = now;
        play.setButtonText(now >= 0 || pro.isSequencePlaying() ? "PARAR" : "OUVIR");
    }

    if (std::abs(slidePosition - slideTarget) > 0.001f) {
        slidePosition += (slideTarget - slidePosition) * 0.24f;
        if (std::abs(slidePosition - slideTarget) < 0.008f)
            slidePosition = slideTarget;
        layoutChordPages();
        if (slidePosition == 0.0f && pendingChordCount != 0) {
            const int count = pendingChordCount;
            pendingChordCount = 0;
            pro.setChordCount(count);
            refresh();
        }
    }
}

void ChordCodeProEditor::animateToPage(int offset) {
    pageOffset = offset == 4 ? 4 : 0;
    slideTarget = pageOffset == 4 ? 1.0f : 0.0f;
    continueCard.setBackMode(pageOffset == 4);
    if (pro.getProgression().chordCount > 4)
        drag.setButtonText("ARRASTAR MIDI");
}

void ChordCodeProEditor::layoutChordPages() {
    static constexpr int xPositions[] { 0, 326, 652, 978 };
    const int travel = juce::roundToInt(slidePosition * (float) cardPageWidth);
    for (int i = 0; i < 8; ++i) {
        const int page = i / 4;
        const int column = i % 4;
        const int margin = (int) ChordCardComponent::glowMargin;
        cards[(size_t) i].setBounds(xPositions[column] + page * cardPageWidth - travel, 0, 310 + margin * 2, 252 + margin * 2);
    }
}

void ChordCodeProEditor::updateLengthLabels(int chordCount) {
    const int selected = length.getSelectedItemIndex();
    const bool oldIgnore = ignore;
    ignore = true;
    length.clear(juce::dontSendNotification);
    length.addItemList(chordCount > 4 ? juce::StringArray { "2/8", "4/8", "8/8" }
                                      : juce::StringArray { "1/4", "2/4", "4/4" }, 1);
    length.setSelectedItemIndex(juce::jlimit(0, 2, selected), juce::dontSendNotification);
    ignore = oldIgnore;
}

void ChordCodeProEditor::selectCard(int slot) {
    selectedSlot = selectedSlot == slot ? -1 : slot;
    for (int i = 0; i < 8; ++i) cards[(size_t) i].setSelected(i == selectedSlot);
}

void ChordCodeProEditor::refresh() {
    const auto progression = pro.getProgression();
    if (pageOffset == 4 && progression.chordCount <= 4) pageOffset = 0;
    for (int slot = 0; slot < 8; ++slot) {
        cards[(size_t) slot].setSlot(slot);
        cards[(size_t) slot].setVisible(slot < 4 || progression.chordCount > 4);
        cards[(size_t) slot].setAvailable(slot < progression.chordCount);
        cards[(size_t) slot].setKeyRoot(progression.settings.key);
        if (slot < progression.chordCount) cards[(size_t) slot].setChord(progression.chords[(size_t) slot]);
        cards[(size_t) slot].setLocked(pro.isLocked(slot));
        cards[(size_t) slot].setSelected(slot == selectedSlot);
        cards[(size_t) slot].setPlaying(slot == pro.activeChord());
    }
    continueCard.setBackMode(pageOffset == 4);
    slideTarget = pageOffset == 4 ? 1.0f : 0.0f;
    continueCard.setVisible(true);
    updateLengthLabels(progression.chordCount);
    drag.setButtonText("ARRASTAR MIDI");
    voicing.setSelected(voicingParameter.getSelectedItemIndex());
    rhythm.setSelected(rhythmParameter.getSelectedItemIndex());
    updateStatus();
    updatePills();
    repaint();
}

void ChordCodeProEditor::updateStatus() {
    const auto progression = pro.getProgression();
    status.setText(key.getText() + " " + utf(ccp::scaleName(progression.settings.scale)) + utf("  ·  ") + juce::String(progression.chordCount)
                   + utf(" ACORDES  ·  ") + juce::String(juce::roundToInt(pro.hostBpm())) + " BPM");
}

void ChordCodeProEditor::updateFromSettings() {
    ignore = true;
    for (auto* combo : { &key, &scale, &style, &mood, &complexity, &tension, &timbre, &voicingParameter, &rhythmParameter, &length })
        combo->repaint();
    voicing.setSelected(voicingParameter.getSelectedItemIndex());
    rhythm.setSelected(rhythmParameter.getSelectedItemIndex());
    ignore = false;
}

bool ChordCodeProEditor::exportBytes(ExportKind kind, std::vector<uint8_t>& bytes) {
    if (kind == ExportKind::midi) {
        bytes = ccp::MidiFile::bytes(pro.getProgression(), pro.midiOptions());
        return !bytes.empty();
    }
    juce::MemoryBlock wav;
    if (!pro.renderWav(wav)) return false;
    bytes.assign(static_cast<const uint8_t*>(wav.getData()), static_cast<const uint8_t*>(wav.getData()) + wav.getSize());
    return true;
}

juce::File ChordCodeProEditor::writeDragFile(ExportKind kind, juce::String& error) {
    std::vector<uint8_t> bytes;
    if (!exportBytes(kind, bytes)) {
        error = kind == ExportKind::wav ? utf("não foi possível gerar o áudio") : utf("não foi possível gerar o MIDI");
        return {};
    }
    const auto stem = chordFileStem(pro.getProgression());
    const auto extension = kind == ExportKind::wav ? juce::String(".wav") : juce::String(".mid");
    for (const auto& dir : dragFolders()) {
        if (dir.isDirectory())
            for (const auto& old : dir.findChildFiles(juce::File::findFiles, false, "*" + extension))
                if (old.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days(3)) old.deleteFile();
        auto file = dir.getNonexistentChildFile(stem, extension, false);
        juce::String why;
        if (writeMidiBytes(file, bytes, why)) return file;
        error << dir.getFullPathName() << "  ->  " << why << "\n";
    }
    return {};
}

void ChordCodeProEditor::exportDrag(ExportKind kind) {
    const auto name = kind == ExportKind::wav ? juce::String("WAV") : juce::String("MIDI");
    juce::String error;
    const auto file = writeDragFile(kind, error);
    if (!file.existsAsFile()) {
        showMidiProblem(utf("Não deu pra criar o ") + name,
            utf("O Windows bloqueou a gravação do arquivo em todas as pastas que o Chord Code tentou.\n\n")
            + error.trim()
            + utf("\n\nSe o \"Acesso controlado a pastas\" do Windows Defender estiver ligado, libere a sua DAW e o Chord Code nele."));
        return;
    }
    // Copy only: hosts that "move" the dropped file would delete it from under us.
    const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this);
    if (!started) revealForDrag(kind);
}

void ChordCodeProEditor::revealForDrag(ExportKind kind) {
    const auto name = kind == ExportKind::wav ? juce::String("WAV") : juce::String("MIDI");
    juce::String error;
    const auto file = writeDragFile(kind, error);
    if (!file.existsAsFile()) {
        showMidiProblem(utf("Não deu pra criar o ") + name, error.trim());
        return;
    }
    file.revealToUser();
    showMidiProblem(name + " pronto",
        utf("Abrimos a pasta com o arquivo:\n") + file.getFileName()
        + utf("\n\nArraste ele da pasta direto para a timeline da sua DAW.\n(Dica: para arrastar do plugin, clique em ARRASTAR ")
        + name + utf(" e, sem soltar, puxe até a DAW.)"));
}

void ChordCodeProEditor::showMidiProblem(const juce::String& title, const juce::String& message) {
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::NoIcon)
        .withTitle(title)
        .withMessage(message)
        .withButton("OK")
        .withAssociatedComponent(this);
    juce::AlertWindow::showAsync(options, nullptr);
}

void ChordCodeProEditor::showSaveMenu() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    menu.addItem(1, "Salvar MIDI");
    menu.addItem(2, utf("Salvar WAV (áudio com o timbre)"));
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(save).withParentComponent(this),
        [safe](int result) {
            if (safe != nullptr && result > 0) safe->saveFile(result == 2 ? ExportKind::wav : ExportKind::midi);
        });
}

void ChordCodeProEditor::saveFile(ExportKind kind) {
    const bool isWav = kind == ExportKind::wav;
    const auto name = isWav ? juce::String("WAV") : juce::String("MIDI");
    const auto extension = isWav ? juce::String("wav") : juce::String("mid");
    const auto stem = chordFileStem(pro.getProgression());
    auto startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (!startDir.isDirectory()) startDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    chooser = std::make_unique<juce::FileChooser>(isWav ? utf("Salvar acordes em WAV") : utf("Salvar acordes MIDI"),
                                                  startDir.getChildFile(stem + "." + extension), "*." + extension);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe, kind, name, extension](const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            const auto chosen = fc.getResult();
            if (chosen == juce::File {}) return;
            const auto file = chosen.withFileExtension(extension);
            std::vector<uint8_t> bytes;
            if (!safe->exportBytes(kind, bytes)) {
                safe->showMidiProblem(utf("Não deu pra salvar o ") + name, utf("O arquivo não pôde ser gerado."));
                return;
            }
            juce::String why;
            if (writeMidiBytes(file, bytes, why)) return;

            // The chosen folder refused the write (usually Windows "Controlled folder access" on
            // Documents/Desktop, or a OneDrive folder offline). Save to a safe folder and show it.
            juce::String error;
            const auto fallback = safe->writeDragFile(kind, error);
            if (fallback.existsAsFile()) {
                fallback.revealToUser();
                safe->showMidiProblem(name + " salvo em outra pasta",
                    utf("O Windows não deixou salvar em:\n") + file.getFullPathName() + "\n(" + why + ")"
                    + utf("\n\nSalvamos aqui e abrimos a pasta:\n") + fallback.getFullPathName());
            } else {
                safe->showMidiProblem(utf("Não deu pra salvar o ") + name,
                    file.getFullPathName() + "\n(" + why + ")\n\n" + error.trim());
            }
        });
}

void ChordCodeProEditor::updatePills() {
    static const char* modes[] { "TECLADO: NOTAS", "TECLADO: ACORDES DO TOM", "TECLADO: TECLAS BRANCAS" };
    const int mode = juce::jlimit(0, 2, (int) pro.keyMode());
    keyModePill.setState(modes[mode], mode != 0);
    const int beat = (int) *pro.apvts.getRawParameterValue("beat");
    beatPill.setState(juce::String("BATIDA: ") + utf(DrumMachine::patternName(beat)), beat != 0);
}

void ChordCodeProEditor::showKeyModeMenu() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    const int current = (int) pro.keyMode();
    menu.addSectionHeader("O QUE O TECLADO MIDI TOCA");
    menu.addItem(1, utf("Notas (cada tecla é uma nota)"), true, current == 0);
    menu.addItem(2, utf("Acordes do tom (a tecla vira o acorde da escala)"), true, current == 1);
    menu.addItem(3, utf("Teclas brancas (C D E F G A B = graus I a VII)"), true, current == 2);
    menu.addSeparator();
    menu.addItem(10, utf("Gravar no card: clique no ● do card e toque o acorde"), false);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(keyModePill).withParentComponent(this),
        [safe](int result) {
            if (safe == nullptr || result < 1 || result > 3) return;
            if (auto* parameter = safe->pro.apvts.getParameter("keyMode"))
                parameter->setValueNotifyingHost(parameter->convertTo0to1((float) (result - 1)));
        });
}

void ChordCodeProEditor::showBeatMenu() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    const int current = (int) *pro.apvts.getRawParameterValue("beat");
    menu.addSectionHeader(utf("BATIDA DE REFERÊNCIA (TOCA NO OUVIR)"));
    for (int i = 0; i < DrumMachine::patternCount; ++i)
        menu.addItem(i + 1, utf(DrumMachine::patternName(i)), true, current == i);
    menu.addSeparator();
    menu.addItem(100, utf("A batida toca junto com o OUVIR. Não vai no MIDI nem no WAV."), false);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(beatPill).withParentComponent(this),
        [safe](int result) {
            if (safe == nullptr || result < 1 || result > DrumMachine::patternCount) return;
            if (auto* parameter = safe->pro.apvts.getParameter("beat"))
                parameter->setValueNotifyingHost(parameter->convertTo0to1((float) (result - 1)));
        });
}

void ChordCodeProEditor::showHistory() {
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    if (pro.historySize() == 0) menu.addItem(100, utf("Nenhuma geração anterior"), false);
    for (int i = 0; i < pro.historySize(); ++i)
        menu.addItem(i + 1, juce::String(i + 1) + ".  " + pro.historyLabel(i));
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(history), [safe](int result) {
        if (safe == nullptr || result <= 0 || result > 10) return;
        safe->ignore = true;
        const bool restored = safe->pro.restoreHistory(result - 1);
        safe->ignore = false;
        if (restored) { safe->updateFromSettings(); safe->refresh(); }
    });
}

void ChordCodeProEditor::showChordChoices(int slot) {
    const auto choices = pro.chordChoices(slot);
    if (choices.empty()) return;
    auto everyChord = pro.allChordChoices(slot);
    auto options = choices;
    options.insert(options.end(), everyChord.begin(), everyChord.end());
    const auto progression = pro.getProgression();

    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    auto chosen = std::make_shared<int>(-1);
    menu.addCustomItem(1, std::make_unique<ChordPickerPanel>(
        slot, progression.settings.key, ccp::HarmonyEngine::scaleNotes(progression.settings.key, progression.settings.scale),
        progression.chords[(size_t) slot], choices, std::move(everyChord), showAllChords,
        [safe, slot](const ccp::Chord& choice) { if (safe != nullptr) safe->pro.auditionChord(slot, choice); },
        chosen), nullptr, utf("Trocar acorde"));

    menu.showMenuAsync(juce::PopupMenu::Options().withParentComponent(this)
                                                   .withTargetComponent(this)
                                                   .withTargetScreenArea(cards[(size_t) slot].getScreenBounds()),
        [safe, slot, options, chosen](int result) {
            if (safe == nullptr) return;
            safe->pro.stopPreview();
            safe->pro.apvts.state.setProperty("chordPickerAllChords", *safe->showAllChords, nullptr);
            if (result == 1 && *chosen >= 0 && *chosen < (int) options.size()) {
                safe->pro.chooseChord(slot, options[(size_t) *chosen]);
                safe->refresh();
            }
        });
}

void ChordCodeProEditor::presentPanel(std::unique_ptr<juce::Component> panel, bool dismissable) {
    content.setEnabled(false);
    const float scale = panelScaleFor(*panel);
    overlay.present(*this, std::move(panel), dismissable, scale);
    overlay.grabKeyboardFocus();
}

void ChordCodeProEditor::showSettings() {
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    SettingsPanel::Model model;
    model.isSynced = [safe] { return safe != nullptr && *safe->pro.apvts.getRawParameterValue("bpmSync") > 0.5f; };
    model.manualBpm = [safe] { return safe != nullptr ? (int) *safe->pro.apvts.getRawParameterValue("manualBpm") : 120; };
    model.hostBpm = [safe] { return safe != nullptr ? juce::roundToInt(safe->pro.hostBpm()) : 120; };
    model.setSynced = [safe](bool value) {
        if (safe == nullptr) return;
        if (auto* param = safe->pro.apvts.getParameter("bpmSync"))
            param->setValueNotifyingHost(param->convertTo0to1(value ? 1.0f : 0.0f));
        safe->updateStatus();
    };
    model.setManualBpm = [safe](int bpm) {
        if (safe == nullptr) return;
        if (auto* sync = safe->pro.apvts.getParameter("bpmSync")) sync->setValueNotifyingHost(0.0f);
        if (auto* param = safe->pro.apvts.getParameter("manualBpm"))
            param->setValueNotifyingHost(param->convertTo0to1((float) bpm));
        safe->updateStatus();
    };
    model.licenseEmail = [safe] { return safe != nullptr ? safe->license->getEmail() : juce::String(); };
    model.updateAvailable = [safe] { return safe != nullptr && safe->updateAvailable; };
    model.checkForUpdates = [safe] { if (safe != nullptr) safe->checkForUpdates(); };
    model.changeLicense = [safe] { if (safe != nullptr) safe->showLicense(LicensePanel::Mode::change); };
    model.close = [safe] { if (safe != nullptr) safe->overlay.dismiss(); };
    model.windowScale = [safe] { return safe != nullptr ? safe->uiScale() / defaultWindowScale : 1.0f; };
    model.setWindowScale = [safe](float scale) {
        if (safe == nullptr) return;
        safe->license->setPreference(windowScaleKey, scale);
        safe->applyWindowScale(scale);
    };
    presentPanel(std::make_unique<SettingsPanel>(std::move(model)), true);
}

void ChordCodeProEditor::checkForUpdates() {
    auto panel = std::make_unique<UpdatePanel>();
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    panel->onClose = [safe] { if (safe != nullptr) safe->overlay.dismiss(); };
    panel->onRetry = [safe] { if (safe != nullptr) safe->checkForUpdates(); };
    presentPanel(std::move(panel), true);
    if (updateChecker.isChecking()) return;
    updateChecker.start(JucePlugin_VersionString, [safe](UpdateChecker::Result result) mutable {
        if (safe != nullptr) safe->showUpdateResult(std::move(result));
    });
}

void ChordCodeProEditor::showUpdateResult(UpdateChecker::Result result) {
    if (result.succeeded) setUpdateAvailable(result.updateAvailable);
    auto* panel = dynamic_cast<UpdatePanel*>(overlay.getPanel());
    if (panel == nullptr) return;
    UpdatePanel::Info info { result.installedVersion, result.latestVersion, result.releaseNotes, result.error, result.downloadUrl };
    panel->setState(!result.succeeded ? UpdatePanel::State::failed
                    : result.updateAvailable ? UpdatePanel::State::available : UpdatePanel::State::upToDate, info);
}

void ChordCodeProEditor::checkLicense() {
    const auto licenseStatus = license->getStatus();
    if (licenseStatus == LicenseManager::Status::active) {
        if (dynamic_cast<LicensePanel*>(overlay.getPanel()) != nullptr) overlay.dismiss();
        license->revalidate(true);
        return;
    }
    if (auto* current = dynamic_cast<LicensePanel*>(overlay.getPanel()); current != nullptr && current->isLocked()) return;
    showLicense(licenseStatus == LicenseManager::Status::revoked ? LicensePanel::Mode::revoked : LicensePanel::Mode::activate);
}

void ChordCodeProEditor::showLicense(LicensePanel::Mode mode) {
    auto panel = std::make_unique<LicensePanel>(mode, license->getEmail());
    juce::Component::SafePointer<ChordCodeProEditor> safe(this);
    juce::Component::SafePointer<LicensePanel> safePanel(panel.get());
    panel->onSubmit = [safe, safePanel](juce::String email) {
        if (safe == nullptr) return;
        safe->license->activate(email, [safe, safePanel](LicenseManager::Outcome outcome) {
            const bool ok = outcome.kind == LicenseManager::Outcome::Kind::authorized;
            if (safePanel != nullptr) safePanel->finish(ok, outcome.message);
            if (ok && safe != nullptr && safe->overlay.getPanel() == safePanel.getComponent()) safe->overlay.dismiss();
        });
    };
    panel->onCancel = [safe] { if (safe != nullptr) safe->showSettings(); };
    presentPanel(std::move(panel), mode == LicensePanel::Mode::change);
}

void ChordCodeProEditor::changeListenerCallback(juce::ChangeBroadcaster*) {
    // A revoked license locks the window again; a new activation is handled by its panel.
    if (license->getStatus() != LicenseManager::Status::active) checkLicense();
}
