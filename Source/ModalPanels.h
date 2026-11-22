#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

// Full-window layer that blurs and darkens the editor behind a centred panel.
class ModalOverlay final : public juce::Component, private juce::Timer {
public:
    ModalOverlay();
    // Shows the panel above a blurred snapshot of the owner. A dismissable panel
    // closes on Escape or on a click outside it.
    void present(juce::Component& owner, std::unique_ptr<juce::Component> panel, bool dismissable, float scale);
    void dismiss();
    bool isPresenting() const { return content != nullptr; }
    juce::Component* getPanel() const { return content.get(); }
    // Re-centres the panel after it changed size.
    void layoutPanel();
    // Used when the editor window is resized while a panel is open.
    void setPanelScale(float scale) { uiScale = scale; layoutPanel(); repaint(); }
    std::function<void()> onDismiss;

    void paint(juce::Graphics&) override;
    void resized() override { layoutPanel(); }
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    void timerCallback() override;
    juce::Image backdrop;
    std::unique_ptr<juce::Component> content;
    bool canDismiss {};
    float uiScale { 1.0f }, reveal {};
};

// Pill button drawn in the plugin's style; primary buttons use the orange gradient.
class PanelButton final : public juce::Button {
public:
    PanelButton(const juce::String& label, bool isPrimary) : juce::Button(label), primary(isPrimary) {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paintButton(juce::Graphics&, bool over, bool down) override;
    bool showUpdateDot {};
private:
    bool primary {};
};

class SettingsPanel final : public juce::Component, private juce::Timer {
public:
    struct Model {
        std::function<bool()> isSynced;
        std::function<int()> manualBpm, hostBpm;
        std::function<void(bool)> setSynced;
        std::function<void(int)> setManualBpm;
        std::function<juce::String()> licenseEmail;
        std::function<bool()> updateAvailable;
        std::function<void()> checkForUpdates, changeLicense, close;
        // Window size as a fraction of the 100% size; choosing one also makes it the default.
        std::function<float()> windowScale;
        std::function<void(float)> setWindowScale;
    };
    static constexpr std::array<float, 5> windowScales { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f };
    explicit SettingsPanel(Model);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override { mouse = e.position; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { mouse = { -1.0f, -1.0f }; repaint(); }
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override { updates.repaint(); }
    static constexpr int panelWidth = 340;
    static constexpr std::array<int, 8> bpmValues { 80, 90, 100, 110, 120, 128, 140, 160 };
    juce::Rectangle<float> syncRow() const { return { 18.0f, 100.0f, (float) panelWidth - 36.0f, 40.0f }; }
    juce::Rectangle<float> licenseRow() const { return { 18.0f, 390.0f, (float) panelWidth - 36.0f, 54.0f }; }
    juce::Rectangle<float> changeLink() const { return licenseRow().removeFromRight(78.0f).reduced(8.0f, 13.0f); }
    juce::Rectangle<float> closeArea() const { return { (float) panelWidth - 46.0f, 14.0f, 30.0f, 30.0f }; }
    bool isOver(juce::Rectangle<float> r) const { return r.contains(mouse); }

    Model model;
    PanelButton updates { juce::String::fromUTF8("VERIFICAR ATUALIZAÇÕES"), false };
    std::array<juce::Rectangle<float>, 8> bpmAreas {};
    std::array<juce::Rectangle<float>, 5> scaleAreas {};
    juce::Point<float> mouse { -1.0f, -1.0f };
};

class UpdatePanel final : public juce::Component, private juce::Timer {
public:
    enum class State { checking, upToDate, available, failed };
    struct Info { juce::String installed, latest, notes, error, downloadUrl; };
    UpdatePanel();
    void setState(State, Info = {});
    std::function<void()> onClose, onRetry;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { spin += 0.18f; repaint(); }
    juce::String title() const;
    juce::String body() const;
    State state { State::checking };
    Info info;
    float spin {};
    PanelButton primary { "OK", true }, secondary { juce::String::fromUTF8("AGORA NÃO"), false };
};

class LicensePanel final : public juce::Component {
public:
    enum class Mode { activate, revoked, change };
    LicensePanel(Mode, juce::String currentEmail);
    // Called with the typed email; the panel shows a busy state until finish() is called.
    std::function<void(juce::String)> onSubmit;
    std::function<void()> onCancel;
    void finish(bool success, const juce::String& message);
    // Activation and revoked panels cannot be closed without a valid email.
    bool isLocked() const { return mode != Mode::change; }
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void submit();
    Mode mode;
    juce::String email, message;
    bool busy {}, failed {};
    juce::TextEditor field;
    PanelButton activate { "ATIVAR", true }, cancel { "CANCELAR", false };
    bool closeHover {};
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    juce::Rectangle<float> closeArea() const { return { (float) getWidth() - 44.0f, 16.0f, 28.0f, 28.0f }; }
};
