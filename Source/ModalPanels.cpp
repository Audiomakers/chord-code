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

// Audio Makers UI window: radius 18, accent light at the top, a tinted badge with the subject's icon.
void drawCard(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(surface));
    g.fillRoundedRectangle(r, AMUI::Size::radiusWindow);
    g.setGradientFill(juce::ColourGradient(juce::Colour(orange).withAlpha(0.10f), 0.0f, r.getY(),
                                           juce::Colour(orange).withAlpha(0.0f), 0.0f, r.getY() + 96.0f, false));
    g.fillRoundedRectangle(r, AMUI::Size::radiusWindow);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(r.reduced(0.5f), AMUI::Size::radiusWindow, 1.0f);
}

void drawBrandMark(juce::Graphics& g, juce::Rectangle<float> mark, AMUI::Icon icon = AMUI::Icon::gear) {
    const float radius = mark.getWidth() * 0.26f;
    g.setColour(juce::Colour(orange).withAlpha(0.12f));
    g.fillRoundedRectangle(mark, radius);
    g.setColour(juce::Colour(orange).withAlpha(0.35f));
    g.drawRoundedRectangle(mark.reduced(0.5f), radius, 1.0f);
    AMUI::drawIcon(g, icon, mark.withSizeKeepingCentre(mark.getWidth() * 0.5f, mark.getWidth() * 0.5f), juce::Colour(orangeLight));
}

void drawCloseIcon(juce::Graphics& g, juce::Rectangle<float> r, bool over) {
    if (over) { g.setColour(juce::Colour(border)); g.fillRoundedRectangle(r, 8.0f); }
    AMUI::drawIcon(g, AMUI::Icon::close, r.withSizeKeepingCentre(16.0f, 16.0f),
                   over ? juce::Colour(textPrimary) : juce::Colour(textSecondary));
}

void drawVersionPill(juce::Graphics& g, juce::Rectangle<float> pill) {
    g.setColour(juce::Colour(orange).withAlpha(0.12f));
    g.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
    g.setColour(juce::Colour(orange).withAlpha(0.35f));
    g.drawRoundedRectangle(pill.reduced(0.5f), pill.getHeight() * 0.5f, 1.0f);
    g.setColour(juce::Colour(orangeLight));
    g.setFont(semi(12.5));
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
    const float radius = AMUI::Size::radiusControl;
    if (primary) {
        juce::Path p; p.addRoundedRectangle(r, radius);
        if (isEnabled()) drawGlow(g, p, juce::Colour(orange).withAlpha(over ? 0.36f : 0.22f), 14);
        g.setGradientFill(accentGradient(r));
        g.fillRoundedRectangle(r, radius);
        if (over || down) { g.setColour(juce::Colours::white.withAlpha(down ? 0.06f : 0.12f)); g.fillRoundedRectangle(r, radius); }
        if (!isEnabled()) { g.setColour(juce::Colour(surface).withAlpha(0.45f)); g.fillRoundedRectangle(r, radius); }
        g.setColour(juce::Colour(0xff1a0a02));
    } else {
        g.setColour(over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
        g.fillRoundedRectangle(r, radius);
        g.setColour(over ? juce::Colour(orange).withAlpha(0.7f)
                    : showUpdateDot ? juce::Colour(updateYellow).withAlpha(0.5f) : juce::Colour(border));
        g.drawRoundedRectangle(r, radius, 1.0f);
        g.setColour(isEnabled() ? juce::Colour(textPrimary) : juce::Colour(textSecondary));
    }
    g.setFont(buttonFont(13.0f));
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
    const float chipWidth = ((float) panelWidth - 36.0f - 4.0f * 7.0f) / 5.0f;
    for (size_t i = 0; i < scaleAreas.size(); ++i)
        scaleAreas[i] = { 18.0f + (float) i * (chipWidth + 7.0f), 290.0f, chipWidth, 32.0f };
    addAndMakeVisible(updates);
    updates.onClick = [this] { if (model.checkForUpdates) model.checkForUpdates(); };
    updates.showUpdateDot = model.updateAvailable && model.updateAvailable();
    if (updates.showUpdateDot) startTimerHz(30);
    setSize(panelWidth, 594);
}

void SettingsPanel::resized() {
    updates.setBounds(18, 492, panelWidth - 36, 42);
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
    g.setFont(font(18.0f, true));
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
    g.setFont(semi(14.5));
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
        g.setFont(semi(14.5));
        g.drawText(juce::String(bpmValues[i]), chip, juce::Justification::centred);
    }
    g.setColour(juce::Colour(textQuiet));
    g.setFont(font(12.0f));
    g.drawText(utf("Escolher um BPM desliga a sincronização"), 18, 228, panelWidth - 36, 18, juce::Justification::centredLeft);
    drawDivider(g, 258.0f, width);

    // Window: default size for every new Chord Code window on this computer.
    sectionLabel(g, "JANELA", 268.0f);
    const float currentScale = model.windowScale ? model.windowScale() : 1.0f;
    for (size_t i = 0; i < scaleAreas.size(); ++i) {
        const auto chip = scaleAreas[i];
        const bool active = std::abs(currentScale - windowScales[i]) < 0.01f;
        const bool over = isOver(chip);
        if (active) { g.setGradientFill(accentGradient(chip)); g.fillRoundedRectangle(chip, 8.0f); }
        else {
            g.setColour(over ? juce::Colour(surfaceHover) : juce::Colour(surfaceRaised));
            g.fillRoundedRectangle(chip, 8.0f);
            g.setColour(over ? juce::Colour(orange).withAlpha(0.7f) : juce::Colour(border));
            g.drawRoundedRectangle(chip.reduced(0.5f), 8.0f, 1.0f);
        }
        g.setColour(active ? juce::Colour(0xff160904) : juce::Colour(textPrimary));
        g.setFont(semi(14.5));
        g.drawText(juce::String(juce::roundToInt(windowScales[i] * 100.0f)) + "%", chip, juce::Justification::centred);
    }
    g.setColour(juce::Colour(textQuiet));
    g.setFont(font(12.0f));
    g.drawText(utf("Tamanho padrão ao abrir o Chord Code"), 18, 328, panelWidth - 36, 18,
               juce::Justification::centredLeft);
    drawDivider(g, 358.0f, width);

    // License: who this copy is registered to.
    sectionLabel(g, utf("LICENÇA"), 368.0f);
    auto lic = licenseRow();
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(lic, 10.0f);
    auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ lic.getX() + 18.0f, lic.getY() + 18.0f });
    g.setColour(juce::Colour(success).withAlpha(0.25f));
    g.fillEllipse(dot.expanded(4.0f));
    g.setColour(juce::Colour(success));
    g.fillEllipse(dot);
    g.setFont(semi(14.5));
    g.drawText("Licenciado", juce::Rectangle<float>(lic.getX() + 32.0f, lic.getY() + 8.0f, 160.0f, 20.0f), juce::Justification::centredLeft);
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(12.5f));
    g.drawFittedText(model.licenseEmail ? model.licenseEmail() : juce::String(),
                     juce::Rectangle<float>(lic.getX() + 32.0f, lic.getY() + 28.0f, lic.getWidth() - 116.0f, 18.0f).toNearestInt(),
                     juce::Justification::centredLeft, 1, 0.8f);
    const auto link = changeLink();
    g.setColour(isOver(link) ? juce::Colour(orangeLight) : juce::Colour(textSecondary));
    g.setFont(semi(12.5));
    g.drawText("Trocar", link, juce::Justification::centredRight);
    drawDivider(g, 460.0f, width);

    sectionLabel(g, utf("ATUALIZAÇÕES"), 470.0f);
    g.setColour(juce::Colour(textQuiet));
    g.setFont(labelFont(11.0f));
    g.drawText("BY AUDIO MAKERS", 18, 548, panelWidth - 36, 16, juce::Justification::centredLeft);
    g.setFont(font(11.0f));
    g.drawText(utf("Piano: Upright Piano KW, FreePats (CC0) · Batidas: AMKR Formula 30"), 18, 564, panelWidth - 36, 16,
               juce::Justification::centredLeft);
}

void SettingsPanel::mouseUp(const juce::MouseEvent& e) {
    if (closeArea().contains(e.position)) { if (model.close) model.close(); return; }
    if (syncRow().contains(e.position)) { model.setSynced(!model.isSynced()); repaint(); return; }
    if (changeLink().contains(e.position)) { if (model.changeLicense) model.changeLicense(); return; }
    for (size_t i = 0; i < bpmValues.size(); ++i)
        if (bpmAreas[i].contains(e.position)) { model.setManualBpm(bpmValues[i]); repaint(); return; }
    for (size_t i = 0; i < scaleAreas.size(); ++i)
        if (scaleAreas[i].contains(e.position)) { if (model.setWindowScale) model.setWindowScale(windowScales[i]); repaint(); return; }
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
    primary.setButtonText(state == State::available ? utf("BAIXAR ATUALIZAÇÃO")
                          : state == State::failed ? utf("TENTAR DE NOVO") : juce::String("OK"));
    secondary.setButtonText(state == State::available ? utf("AGORA NÃO") : juce::String("FECHAR"));
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
        g.setFont(semi(13.0));
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
    for (auto* f : { &field, &confirm }) {
        f->setFont(font(15.0f));
        f->setTextToShowWhenEmpty(f == &field ? "seuemail@gmail.com" : utf("digite o e-mail de novo"), juce::Colour(textQuiet));
        f->setIndents(40, 12);
        f->setJustification(juce::Justification::centredLeft);
        f->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);   // the rounded box is painted by the panel
        f->setColour(juce::TextEditor::textColourId, juce::Colour(textPrimary));
        f->setColour(juce::TextEditor::highlightColourId, juce::Colour(orange).withAlpha(0.35f));
        f->setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(textPrimary));
        f->setColour(juce::CaretComponent::caretColourId, juce::Colour(orangeLight));
        f->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        f->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        f->setInputRestrictions(254);
        f->onTextChange = [this] { if (failed) { failed = false; message.clear(); } repaint(); };
        addAndMakeVisible(*f);
    }
    if (mode == Mode::revoked) field.setText(email, false);
    field.onReturnKey = [this] { confirm.grabKeyboardFocus(); };
    confirm.onReturnKey = [this] { submit(); };
    addAndMakeVisible(activate);
    addChildComponent(cancel);
    cancel.setVisible(mode == Mode::change);
    activate.onClick = [this] { submit(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    setSize(400, 392);
}

void LicensePanel::visibilityChanged() {
    if (isShowing()) juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::TextEditor>(&field)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

void LicensePanel::submit() {
    if (busy) return;
    const auto a = field.getText().trim().toLowerCase(), b = confirm.getText().trim().toLowerCase();
    if (a.isEmpty() || !a.containsChar('@') || !a.fromFirstOccurrenceOf("@", false, false).containsChar('.')) {
        failed = true; message = utf("Digite um e-mail válido."); repaint(); return;
    }
    if (a != b) {
        failed = true; message = utf("Os dois e-mails não são iguais. Confira e digite de novo."); repaint(); return;
    }
    busy = true;
    failed = false;
    message = utf("Verificando sua compra...");
    activate.setEnabled(false);
    activate.setButtonText("VERIFICANDO...");
    field.setReadOnly(true);
    confirm.setReadOnly(true);
    repaint();
    if (onSubmit) onSubmit(field.getText().trim());
}

void LicensePanel::finish(bool ok, const juce::String& text) {
    busy = false;
    failed = !ok;
    message = text;
    activate.setEnabled(true);
    activate.setButtonText("ATIVAR");
    field.setReadOnly(false);
    confirm.setReadOnly(false);
    repaint();
}

// Same dialog as Melody Code: key badge, one field, the two buttons the same size with ATIVAR on the right.
void LicensePanel::resized() {
    field.setBounds(24, 164, getWidth() - 48, AMUI::Size::field);
    confirm.setBounds(24, 240, getWidth() - 48, AMUI::Size::field);
    auto row = juce::Rectangle<int>(24, getHeight() - 24 - AMUI::Size::button, getWidth() - 48, AMUI::Size::button);
    activate.setBounds(row.removeFromRight(cancel.isVisible() ? 128 : 160));
    if (cancel.isVisible()) { row.removeFromRight(10); cancel.setBounds(row.removeFromRight(128)); }
}

void LicensePanel::mouseMove(const juce::MouseEvent& e) {
    const bool over = mode == Mode::change && closeArea().contains(e.position);
    if (over != closeHover) { closeHover = over; repaint(); }
}
void LicensePanel::mouseExit(const juce::MouseEvent&) { if (closeHover) { closeHover = false; repaint(); } }
void LicensePanel::mouseUp(const juce::MouseEvent& e) {
    if (mode == Mode::change && closeArea().contains(e.position) && onCancel) onCancel();
}

void LicensePanel::paint(juce::Graphics& g) {
    drawCard(g, getLocalBounds().toFloat());
    drawBrandMark(g, { 24.0f, 24.0f, 40.0f, 40.0f }, AMUI::Icon::key);
    g.setColour(mode == Mode::revoked ? juce::Colour(danger) : juce::Colour(textSecondary));
    g.setFont(labelFont(11.0f));
    g.drawText(mode == Mode::revoked ? utf("LICENÇA DESATIVADA") : utf("LICENÇA"), 78, 25, 240, 16, juce::Justification::centredLeft);
    g.setColour(juce::Colour(textPrimary));
    g.setFont(font(18.0f, true));
    g.drawText(mode == Mode::activate ? utf("Ative o Chord Code") : mode == Mode::revoked ? utf("Seu acesso foi desativado")
                                                                                         : utf("Trocar e-mail da licença"),
               78, 41, getWidth() - 126, 24, juce::Justification::centredLeft);
    if (mode == Mode::change) drawCloseIcon(g, closeArea(), closeHover);

    juce::String text;
    if (mode == Mode::activate)
        text = utf("Digite o e-mail usado na compra do Chord Code.");
    else if (mode == Mode::revoked)
        text = utf("O acesso de ") + email + utf(" não está mais ativo. Informe um e-mail com acesso ativo.");
    else
        text = utf("Informe outro e-mail com acesso ativo ao Chord Code.");
    g.setColour(juce::Colour(textSecondary));
    g.setFont(font(14.0f));
    g.drawFittedText(text, 24, 82, getWidth() - 48, 42, juce::Justification::topLeft, 2, 1.0f);

    g.setColour(juce::Colour(textSecondary));
    g.setFont(labelFont(11.0f));
    g.drawText(utf("E-MAIL DA COMPRA"), 24, 142, 240, 16, juce::Justification::centredLeft);
    g.drawText(utf("CONFIRME O E-MAIL"), 24, 218, 240, 16, juce::Justification::centredLeft);
    const bool matches = confirm.getText().isNotEmpty() && confirm.getText().trim().equalsIgnoreCase(field.getText().trim());
    for (auto* f : { &field, &confirm }) {   // the boxes: the focused one glows; the confirmation turns green when it matches
        const auto box = f->getBounds().toFloat();
        const bool focused = f == &field ? confirm.isEmpty() : !confirm.isEmpty();
        if (focused) {
            g.setColour(juce::Colour(orange).withAlpha(0.12f));
            g.drawRoundedRectangle(box.expanded(2.0f), AMUI::Size::radiusControl + 2.0f, 3.0f);
        }
        g.setColour(juce::Colour(background));
        g.fillRoundedRectangle(box, AMUI::Size::radiusControl);
        const auto edge = f == &confirm && matches ? juce::Colour(0xff4ade80) : juce::Colour(orange).withAlpha(focused ? 0.8f : 0.35f);
        g.setColour(edge);
        g.drawRoundedRectangle(box.reduced(0.5f), AMUI::Size::radiusControl, 1.0f);
        AMUI::drawIcon(g, f == &confirm && matches ? AMUI::Icon::check : AMUI::Icon::mail,
                       { box.getX() + 14.0f, box.getCentreY() - 8.0f, 16.0f, 16.0f }, f == &confirm && matches ? juce::Colour(0xff4ade80) : juce::Colour(orangeLight));
    }
    g.setColour(message.isNotEmpty() ? (failed ? juce::Colour(danger) : juce::Colour(textSecondary)) : juce::Colour(textQuiet));
    g.setFont(font(12.5f));
    g.drawFittedText(message.isNotEmpty() ? message : utf("Verificação só na primeira vez; depois funciona offline."),
                     24, 290, getWidth() - 48, 30, juce::Justification::topLeft, 2, 1.0f);
}
