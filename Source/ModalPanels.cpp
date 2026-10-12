#include "ModalPanels.h"
#include "ChordCodeTheme.h"

using namespace ChordCodeTheme;

namespace {
juce::String utf(const char* text) { return juce::String::fromUTF8(text); }
constexpr juce::uint32 success = 0xff3ddc84;
constexpr juce::uint32 danger = 0xffff6b5e;

void sectionLabel(juce::Graphics& g, const juce::String& text, float y) {
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.5f));
    g.drawText(text, juce::Rectangle<float>(18.0f, y, 260.0f, 16.0f), juce::Justification::centredLeft);
}

void drawDivider(juce::Graphics& g, float y, float width) {
    g.setColour(juce::Colour(borderQuiet));
    g.fillRect(18.0f, y, width - 36.0f, 1.0f);
}

void drawCard(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setGradientFill(raisedGradient(r, juce::Colour(0xff12151a), juce::Colour(surface)));
    g.fillRoundedRectangle(r, 16.0f);
    // Warm light along the top edge, then the outline.
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.10f), r.getCentreX(), r.getY(),
                                           juce::Colour(orange).withAlpha(0.0f), r.getCentreX(), r.getY() + 90.0f, false));
    g.fillRoundedRectangle(r, 16.0f);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(r.reduced(0.5f), 16.0f, 1.0f);
}

void drawBrandMark(juce::Graphics& g, juce::Rectangle<float> mark) {
    g.setGradientFill(accentGradient(mark));
    g.fillRoundedRectangle(mark, mark.getWidth() * 10.0f / 38.0f);
    g.setColour(juce::Colour(0xff160904));
    const float u = mark.getWidth() / 38.0f;
    static constexpr float bars[] { 10.0f, 20.0f, 14.0f, 24.0f };
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle(mark.getX() + (8.0f + (float) i * 6.5f) * u, mark.getCentreY() - bars[i] * 0.5f * u,
                               3.5f * u, bars[i] * u, 1.75f * u);
}

void drawCloseIcon(juce::Graphics& g, juce::Rectangle<float> r, bool over) {
    g.setColour(over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
    g.fillEllipse(r);
    g.setColour(over ? juce::Colour(textPrimary) : juce::Colour(textSecondary));
    const auto c = r.reduced(r.getWidth() * 0.34f);
    g.drawLine({ c.getTopLeft(), c.getBottomRight() }, 1.6f);
    g.drawLine({ c.getBottomLeft(), c.getTopRight() }, 1.6f);
}

void drawVersionPill(juce::Graphics& g, juce::Rectangle<float> pill) {
    g.setColour(juce::Colour(orange).withAlpha(0.12f));
    g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
    g.setColour(juce::Colour(orange).withAlpha(0.35f));
    g.drawRoundedRectangle(pill.reduced(0.5f), pill.getHeight() * 0.5f, 1.0f);
    g.setColour(juce::Colour(orangeLight));
    g.setFont(font(12.5f, true));
    g.drawText("v" JucePlugin_VersionString, pill, juce::Justification::centred);
}
}

// ---------------------------------------------------------------------------

ModalOverlay::ModalOverlay() {
    setWantsKeyboardFocus(true);
    setVisible(false);
}

void ModalOverlay::present(juce::Component& owner, std::unique_ptr<juce::Component> panel, bool dismissable, float scale) {
    const bool wasPresenting = isPresenting();
    if (content != nullptr) removeChildComponent(content.get());
    content = std::move(panel);
    canDismiss = dismissable;
    uiScale = scale;

    if (!wasPresenting) {
        setVisible(false);
        // Blur by shrinking the snapshot, softening it, and stretching it back when drawn.
        const auto snapshot = owner.createComponentSnapshot(owner.getLocalBounds(), true, 1.0f);
        const int w = juce::jmax(1, snapshot.getWidth() / 6), h = juce::jmax(1, snapshot.getHeight() / 6);
        juce::Image small(juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g(small);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(snapshot, 0, 0, w, h, 0, 0, snapshot.getWidth(), snapshot.getHeight());
        }
        juce::ImageConvolutionKernel kernel(7);
        kernel.createGaussianBlur(2.2f);
        kernel.applyToImage(small, small, small.getBounds());
        backdrop = small;
        reveal = 0.0f;
    }

    setBounds(owner.getLocalBounds());
    addAndMakeVisible(*content);
    layoutPanel();
    setVisible(true);
    toFront(true);
    if (!wasPresenting) startTimerHz(60);
}

void ModalOverlay::dismiss() {
    if (content == nullptr) return;
    stopTimer();
    removeChildComponent(content.get());
    // The panel may be the one asking to close; destroy it after this call returns.
    juce::MessageManager::callAsync([old = std::shared_ptr<juce::Component>(content.release())] {});
    backdrop = {};
    setVisible(false);
    if (onDismiss) onDismiss();
}

void ModalOverlay::layoutPanel() {
    if (content == nullptr) return;
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    const float w = (float) content->getWidth() * uiScale, h = (float) content->getHeight() * uiScale;
    const float x = ((float) getWidth() - w) * 0.5f;
    const float y = ((float) getHeight() - h) * 0.5f + (1.0f - ease) * 14.0f;
    content->setTopLeftPosition(0, 0);
    content->setTransform(juce::AffineTransform::scale(uiScale).translated(x, y));
    content->setAlpha(ease);
}

void ModalOverlay::timerCallback() {
    reveal = juce::jmin(1.0f, reveal + 1.0f / 9.0f);
    layoutPanel();
    repaint();
    if (reveal >= 1.0f) stopTimer();
}

void ModalOverlay::paint(juce::Graphics& g) {
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    if (backdrop.isValid()) {
        g.setOpacity(ease);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(backdrop, getLocalBounds().toFloat());
        g.setOpacity(1.0f);
    }
    g.setColour(juce::Colour(0xff040506).withAlpha(0.58f * ease));
    g.fillAll();

    if (content != nullptr) {
        auto shadowArea = content->getBounds().toFloat().transformedBy(content->getTransform());
        juce::Path shape;
        shape.addRoundedRectangle(shadowArea, 16.0f * uiScale);
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f * ease), juce::roundToInt(40.0f * uiScale),
                         { 0, juce::roundToInt(14.0f * uiScale) }).drawForPath(g, shape);
    }
}

void ModalOverlay::mouseUp(const juce::MouseEvent& e) {
    if (!canDismiss || content == nullptr) return;
    const auto area = content->getBounds().toFloat().transformedBy(content->getTransform());
    if (!area.contains(e.position)) dismiss();
}

bool ModalOverlay::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && canDismiss) { dismiss(); return true; }
    return false;
}

// ---------------------------------------------------------------------------

void PanelButton::paintButton(juce::Graphics& g, bool over, bool down) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = 10.0f;
    if (primary) {
        g.setGradientFill(accentGradient(r));
        g.fillRoundedRectangle(r, radius);
        if (over || down) { g.setColour(juce::Colours::white.withAlpha(down ? 0.06f : 0.12f)); g.fillRoundedRectangle(r, radius); }
        if (!isEnabled()) { g.setColour(juce::Colour(surface).withAlpha(0.45f)); g.fillRoundedRectangle(r, radius); }
        g.setColour(juce::Colour(0xff160904));
    } else {
        g.setColour(over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
        g.fillRoundedRectangle(r, radius);
        g.setColour(over ? juce::Colour(orange).withAlpha(0.7f)
                    : showUpdateDot ? juce::Colour(updateYellow).withAlpha(0.5f) : juce::Colour(border));
        g.drawRoundedRectangle(r, radius, 1.0f);
        g.setColour(isEnabled() ? juce::Colour(textPrimary) : juce::Colour(textSecondary));
    }
    g.setFont(font(14.5f, true));
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
    if (showUpdateDot) drawUpdateDot(g, { 22.0f, (float) getHeight() * 0.5f });
}

// ---------------------------------------------------------------------------

SettingsPanel::SettingsPanel(Model m) : model(std::move(m)) {
    int x = 18, y = 152;
    for (size_t i = 0; i < bpmValues.size(); ++i) {
        bpmAreas[i] = { (float) x, (float) y, 70.0f, 32.0f };
        x += 77;
        if (i == 3) { x = 18; y += 38; }
    }
    addAndMakeVisible(updates);
    updates.onClick = [this] { if (model.checkForUpdates) model.checkForUpdates(); };
    updates.showUpdateDot = model.updateAvailable && model.updateAvailable();
    if (updates.showUpdateDot) startTimerHz(30);
    setSize(panelWidth, 494);
}

void SettingsPanel::resized() {
    updates.setBounds(18, 392, panelWidth - 36, 42);
}

void SettingsPanel::paint(juce::Graphics& g) {
    drawCard(g, getLocalBounds().toFloat());
    const bool synced = model.isSynced();
    const int manual = model.manualBpm();
    const float width = (float) panelWidth;

    drawBrandMark(g, { 18.0f, 18.0f, 34.0f, 34.0f });
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.5f));
    g.drawText(utf("CONFIGURAÇÕES"), 62, 17, 200, 16, juce::Justification::centredLeft);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(19.0f, true));
    g.drawText("Chord Code", 62, 32, 130, 24, juce::Justification::centredLeft);
    drawVersionPill(g, { 172.0f, 34.0f, 62.0f, 22.0f });
    drawCloseIcon(g, closeArea(), isOver(closeArea()));
    drawDivider(g, 70.0f, width);

    // Tempo: sync switch, then manual BPM chips.
    sectionLabel(g, "TEMPO", 80.0f);
    auto row = syncRow();
    g.setColour(isOver(row) ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(row, 10.0f);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(14.5f, true));
    g.drawText(utf("Sincronizar BPM"), row.withTrimmedLeft(12.0f).toNearestInt(), juce::Justification::centredLeft);
    auto track = juce::Rectangle<float>(40.0f, 22.0f).withCentre({ row.getRight() - 32.0f, row.getCentreY() });
    if (synced) g.setGradientFill(accentGradient(track)); else g.setColour(juce::Colour(0xff2a2f37));
    g.fillRoundedRectangle(track, 11.0f);
    g.setColour(juce::Colour(textPrimary));
    g.fillEllipse(juce::Rectangle<float>(16.0f, 16.0f).withCentre({ synced ? track.getRight() - 11.0f : track.getX() + 11.0f, track.getCentreY() }));
    if (synced) {
        g.setColour(juce::Colour(textSecondary));
        g.setFont(font(12.5f));
        g.drawText(utf("projeto: ") + juce::String(model.hostBpm()) + " BPM", row.withTrimmedRight(62.0f).toNearestInt(),
                   juce::Justification::centredRight);
    }

    for (size_t i = 0; i < bpmValues.size(); ++i) {
        const auto chip = bpmAreas[i];
        const bool active = !synced && manual == bpmValues[i];
        const bool over = isOver(chip);
        if (active) { g.setGradientFill(accentGradient(chip)); g.fillRoundedRectangle(chip, 8.0f); }
        else {
            g.setColour(over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
            g.fillRoundedRectangle(chip, 8.0f);
            g.setColour(over ? juce::Colour(orange).withAlpha(0.7f) : juce::Colour(border));
            g.drawRoundedRectangle(chip.reduced(0.5f), 8.0f, 1.0f);
        }
        g.setColour(active ? juce::Colour(0xff160904) : synced ? juce::Colour(textSecondary) : juce::Colour(textPrimary));
        g.setFont(font(14.5f, true));
        g.drawText(juce::String(bpmValues[i]), chip, juce::Justification::centred);
    }
    g.setColour(juce::Colour(textQuiet));
    g.setFont(font(12.0f));
    g.drawText(utf("Escolher um BPM desliga a sincronização"), 18, 228, panelWidth - 36, 18, juce::Justification::centredLeft);
    drawDivider(g, 258.0f, width);

    // License: who this copy is registered to.
    sectionLabel(g, utf("LICENÇA"), 268.0f);
    auto lic = licenseRow();
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(lic, 10.0f);
    auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ lic.getX() + 18.0f, lic.getY() + 18.0f });
    g.setColour(juce::Colour(success).withAlpha(0.25f));
    g.fillEllipse(dot.expanded(4.0f));
    g.setColour(juce::Colour(success));
    g.fillEllipse(dot);
    g.setFont(font(14.5f, true));
    g.drawText("Licenciado", juce::Rectangle<float>(lic.getX() + 32.0f, lic.getY() + 8.0f, 160.0f, 20.0f), juce::Justification::centredLeft);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(12.5f));
    g.drawFittedText(model.licenseEmail ? model.licenseEmail() : juce::String(),
                     juce::Rectangle<float>(lic.getX() + 32.0f, lic.getY() + 28.0f, lic.getWidth() - 116.0f, 18.0f).toNearestInt(),
                     juce::Justification::centredLeft, 1, 0.8f);
    const auto link = changeLink();
    g.setColour(isOver(link) ? juce::Colour(orangeLight) : juce::Colour(textSecondary));
    g.setFont(font(12.5f, true));
    g.drawText("Trocar", link, juce::Justification::centredRight);
    drawDivider(g, 360.0f, width);

    sectionLabel(g, utf("ATUALIZAÇÕES"), 370.0f);
    g.setColour(juce::Colour(textQuiet));
    g.setFont(labelFont(11.0f));
    g.drawText("BY AUDIO MAKERS", 18, 448, panelWidth - 36, 16, juce::Justification::centredLeft);
    g.setFont(font(11.0f));
    g.drawText(utf("Piano: Salamander Grand Piano, Alexander Holm (CC BY 3.0)"), 18, 464, panelWidth - 36, 16,
               juce::Justification::centredLeft);
}

void SettingsPanel::mouseUp(const juce::MouseEvent& e) {
    if (closeArea().contains(e.position)) { if (model.close) model.close(); return; }
    if (syncRow().contains(e.position)) { model.setSynced(!model.isSynced()); repaint(); return; }
    if (changeLink().contains(e.position)) { if (model.changeLicense) model.changeLicense(); return; }
    for (size_t i = 0; i < bpmValues.size(); ++i)
        if (bpmAreas[i].contains(e.position)) { model.setManualBpm(bpmValues[i]); repaint(); return; }
}

// ---------------------------------------------------------------------------

UpdatePanel::UpdatePanel() {
    addAndMakeVisible(primary);
    addChildComponent(secondary);
    primary.onClick = [this] {
        if (state == State::available) juce::URL(info.downloadUrl).launchInDefaultBrowser();
        if (state == State::failed) { if (onRetry) onRetry(); return; }
        if (onClose) onClose();
    };
    secondary.onClick = [this] { if (onClose) onClose(); };
    setState(State::checking);
}

void UpdatePanel::setState(State s, Info i) {
    state = s;
    info = std::move(i);
    if (state == State::checking) startTimerHz(30); else stopTimer();
    primary.setButtonText(state == State::available ? utf("Baixar atualização")
                          : state == State::failed ? utf("Tentar de novo") : juce::String("OK"));
    secondary.setButtonText(state == State::available ? utf("Agora não") : juce::String("Fechar"));
    primary.setVisible(state != State::checking);
    secondary.setVisible(state == State::available || state == State::failed);

    int height = 210;
    if (state == State::available && info.notes.isNotEmpty()) {
        juce::AttributedString notes;
        notes.append(info.notes, font(13.0f), juce::Colour(textSecondary));
        juce::TextLayout layout;
        layout.createLayout(notes, 324.0f - 64.0f);
        height += juce::jmin(110, (int) std::ceil(layout.getHeight()) + 26);
    }
    if (state == State::available) height += 34;
    if (state == State::checking) height = 170;
    setSize(360, height);
    resized();
    repaint();
    if (auto* overlay = dynamic_cast<ModalOverlay*>(getParentComponent())) overlay->layoutPanel();
}

juce::String UpdatePanel::title() const {
    switch (state) {
        case State::checking: return utf("Procurando atualizações");
        case State::upToDate: return utf("Tudo em dia");
        case State::available: return utf("Nova versão disponível");
        case State::failed: return utf("Não deu para verificar");
    }
    return {};
}

juce::String UpdatePanel::body() const {
    switch (state) {
        case State::checking: return utf("Conferindo a versão mais recente do Chord Code...");
        case State::upToDate: return utf("Você já está usando a versão mais recente do Chord Code (v") + info.installed + ").";
        case State::available: return utf("A versão ") + info.latest + utf(" já pode ser baixada. Feche a DAW antes de instalar.");
        case State::failed: return info.error;
    }
    return {};
}

void UpdatePanel::resized() {
    auto row = juce::Rectangle<int>(18, getHeight() - 60, getWidth() - 36, 42);
    if (secondary.isVisible()) {
        secondary.setBounds(row.removeFromLeft(118));
        row.removeFromLeft(10);
    }
    primary.setBounds(row);
}

void UpdatePanel::paint(juce::Graphics& g) {
    drawCard(g, getLocalBounds().toFloat());
    auto badge = juce::Rectangle<float>(44.0f, 44.0f).withPosition(18.0f, 20.0f);
    const auto tint = state == State::upToDate ? juce::Colour(success) : state == State::failed ? juce::Colour(danger) : juce::Colour(orange);
    g.setColour(tint.withAlpha(0.14f));
    g.fillRoundedRectangle(badge, 12.0f);
    g.setColour(tint);
    const auto c = badge.getCentre();
    if (state == State::checking) {
        juce::Path arc;
        arc.addCentredArc(c.x, c.y, 11.0f, 11.0f, spin, 0.0f, juce::MathConstants<float>::pi * 1.4f, true);
        g.strokePath(arc, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else if (state == State::upToDate) {
        juce::Path tick;
        tick.startNewSubPath(c.x - 8.0f, c.y + 0.5f);
        tick.lineTo(c.x - 2.5f, c.y + 6.0f);
        tick.lineTo(c.x + 9.0f, c.y - 6.0f);
        g.strokePath(tick, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else if (state == State::available) {
        juce::Path arrow;
        arrow.startNewSubPath(c.x, c.y - 10.0f);
        arrow.lineTo(c.x, c.y + 6.0f);
        arrow.startNewSubPath(c.x - 7.0f, c.y - 1.0f);
        arrow.lineTo(c.x, c.y + 6.0f);
        arrow.lineTo(c.x + 7.0f, c.y - 1.0f);
        arrow.startNewSubPath(c.x - 10.0f, c.y + 11.0f);
        arrow.lineTo(c.x + 10.0f, c.y + 11.0f);
        g.strokePath(arrow, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    } else {
        g.fillRoundedRectangle(c.x - 1.4f, c.y - 10.0f, 2.8f, 13.0f, 1.4f);
        g.fillEllipse(c.x - 1.8f, c.y + 6.0f, 3.6f, 3.6f);
    }

    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.5f));
    g.drawText(utf("ATUALIZAÇÕES"), 76, 22, 200, 16, juce::Justification::centredLeft);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(19.0f, true));
    g.drawText(title(), 76, 38, getWidth() - 94, 26, juce::Justification::centredLeft);

    float y = 82.0f;
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(14.0f));
    g.drawFittedText(body(), juce::Rectangle<float>(18.0f, y, (float) getWidth() - 36.0f, 44.0f).toNearestInt(),
                     juce::Justification::topLeft, 3, 1.0f);
    y += 50.0f;

    if (state == State::available) {
        // Installed → new version.
        auto from = juce::Rectangle<float>(18.0f, y, 76.0f, 26.0f);
        g.setColour(juce::Colour(surfaceRaised));
        g.fillRoundedRectangle(from, 13.0f);
        g.setColour(juce::Colour(textSecondary));
        g.setFont(font(13.0f, true));
        g.drawText("v" + info.installed, from, juce::Justification::centred);
        g.setColour(juce::Colour(textQuiet));
        g.drawText(utf("→"), juce::Rectangle<float>(from.getRight(), y, 30.0f, 26.0f), juce::Justification::centred);
        auto to = from.withX(from.getRight() + 30.0f);
        g.setColour(juce::Colour(orange).withAlpha(0.14f));
        g.fillRoundedRectangle(to, 13.0f);
        g.setColour(juce::Colour(orangeLight));
        g.drawText("v" + info.latest, to, juce::Justification::centred);
        y += 40.0f;
        if (info.notes.isNotEmpty()) {
            auto notesArea = juce::Rectangle<float>(18.0f, y, (float) getWidth() - 36.0f, (float) getHeight() - 76.0f - y);
            g.setColour(juce::Colour(surfaceRaised));
            g.fillRoundedRectangle(notesArea, 10.0f);
            juce::AttributedString notes;
            notes.append(info.notes, font(13.0f), juce::Colour(textSecondary));
            juce::TextLayout layout;
            layout.createLayout(notes, notesArea.getWidth() - 28.0f);
            layout.draw(g, notesArea.reduced(14.0f, 12.0f));
        }
    }
}

// ---------------------------------------------------------------------------

LicensePanel::LicensePanel(Mode m, juce::String currentEmail) : mode(m), email(std::move(currentEmail)) {
    field.setFont(font(15.5f));
    field.setTextToShowWhenEmpty("seuemail@gmail.com", juce::Colour(textQuiet));
    field.setIndents(14, 13);
    field.setJustification(juce::Justification::centredLeft);
    field.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0a0c0f));
    field.setColour(juce::TextEditor::textColourId, juce::Colour(textPrimary));
    field.setColour(juce::TextEditor::highlightColourId, juce::Colour(orange).withAlpha(0.35f));
    field.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(textPrimary));
    field.setColour(juce::CaretComponent::caretColourId, juce::Colour(orangeLight));
    field.setColour(juce::TextEditor::outlineColourId, juce::Colour(border));
    field.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(orange));
    field.setInputRestrictions(254);
    if (mode == Mode::revoked) field.setText(email, false);
    field.onReturnKey = [this] { submit(); };
    field.onTextChange = [this] { if (failed) { failed = false; message.clear(); repaint(); } };
    addAndMakeVisible(field);
    addAndMakeVisible(activate);
    addChildComponent(cancel);
    cancel.setVisible(mode == Mode::change);
    activate.onClick = [this] { submit(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    setSize(380, 330);
}

void LicensePanel::visibilityChanged() {
    if (isShowing()) juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::TextEditor>(&field)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

void LicensePanel::submit() {
    if (busy) return;
    busy = true;
    failed = false;
    message = utf("Verificando sua compra...");
    activate.setEnabled(false);
    activate.setButtonText("Verificando...");
    field.setReadOnly(true);
    repaint();
    if (onSubmit) onSubmit(field.getText());
}

void LicensePanel::finish(bool ok, const juce::String& text) {
    busy = false;
    failed = !ok;
    message = text;
    activate.setEnabled(true);
    activate.setButtonText("Ativar");
    field.setReadOnly(false);
    repaint();
}

void LicensePanel::resized() {
    field.setBounds(22, 166, getWidth() - 44, 46);
    auto row = juce::Rectangle<int>(22, getHeight() - 66, getWidth() - 44, 44);
    if (cancel.isVisible()) {
        cancel.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(10);
    }
    activate.setBounds(row);
}

void LicensePanel::paint(juce::Graphics& g) {
    drawCard(g, getLocalBounds().toFloat());
    drawBrandMark(g, { 22.0f, 22.0f, 40.0f, 40.0f });
    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.5f));
    g.drawText(mode == Mode::revoked ? utf("LICENÇA DESATIVADA") : utf("LICENÇA"), 74, 22, 220, 16, juce::Justification::centredLeft);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(20.0f, true));
    g.drawText(mode == Mode::activate ? utf("Ative o Chord Code") : mode == Mode::revoked ? utf("Seu acesso foi desativado")
                                                                                         : utf("Trocar e-mail da licença"),
               74, 38, getWidth() - 96, 26, juce::Justification::centredLeft);
    drawVersionPill(g, { (float) getWidth() - 84.0f, 22.0f, 62.0f, 22.0f });

    juce::String text;
    if (mode == Mode::activate)
        text = utf("Digite o e-mail usado na compra do Chord Code Pro. A verificação acontece só nesta primeira vez; depois ele funciona mesmo sem internet.");
    else if (mode == Mode::revoked)
        text = utf("O acesso de ") + email + utf(" não está mais ativo. Informe um e-mail com acesso ativo para continuar usando.");
    else
        text = utf("Informe outro e-mail com acesso ativo ao Chord Code Pro.");
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(14.0f));
    g.drawFittedText(text, 22, 80, getWidth() - 44, 58, juce::Justification::topLeft, 3, 1.0f);

    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.0f));
    g.drawText(utf("E-MAIL DA COMPRA"), 22, 146, 200, 16, juce::Justification::centredLeft);

    if (message.isNotEmpty()) {
        g.setColour(failed ? juce::Colour(danger) : juce::Colour(textSecondary));
        g.setFont(font(13.0f));
        g.drawFittedText(message, 22, 218, getWidth() - 44, 36, juce::Justification::topLeft, 2, 1.0f);
    }
}
