#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <dwmapi.h>
#include <algorithm>
#include <string>
#include <vector>
#include <gdiplus.h>

#ifndef CCP_VERSION
#define CCP_VERSION L"1.4.0"
#endif

namespace {
constexpr int payloadVst = 201;
constexpr int payloadApp = 202;
constexpr wchar_t uninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AudioMakers.ChordCode";
constexpr wchar_t appTitle[] = L"Chord Code";
constexpr float windowWidth = 640.0f, windowHeight = 500.0f;

std::wstring installedApp;

std::wstring lastError() {
    const auto code = GetLastError();
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = buffer ? buffer : L"Erro desconhecido";
    if (buffer) LocalFree(buffer);
    return result;
}

std::wstring knownFolder(REFKNOWNFOLDERID id) {
    wchar_t* path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &path))) result = path;
    CoTaskMemFree(path);
    return result;
}

std::wstring appFolder() { return knownFolder(FOLDERID_ProgramFiles) + L"\\Audio Makers\\Chord Code"; }
std::wstring desktopLink() { return knownFolder(FOLDERID_PublicDesktop) + L"\\Chord Code.lnk"; }
std::wstring startMenuLink() { return knownFolder(FOLDERID_CommonPrograms) + L"\\Audio Makers\\Chord Code.lnk"; }

bool ensureDirectory(const std::wstring& path, std::wstring& error) {
    const int result = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS || result == ERROR_FILE_EXISTS) return true;
    SetLastError(static_cast<DWORD>(result));
    error = L"Não foi possível criar a pasta:\n" + path + L"\n\n" + lastError();
    return false;
}

bool removeTreeIfPresent(const std::wstring& path, std::wstring& error) {
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return true;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (DeleteFileW(path.c_str())) return true;
        error = L"Feche o Chord Code e sua DAW e tente novamente. Não foi possível substituir:\n" + path + L"\n\n" + lastError();
        return false;
    }
    std::wstring from = path;
    from.push_back(L'\0');
    from.push_back(L'\0');
    SHFILEOPSTRUCTW operation {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    if (SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted) return true;
    error = L"Feche o Chord Code e sua DAW e tente novamente. Não foi possível substituir:\n" + path;
    return false;
}

bool writePayload(int resourceId, const std::wstring& output, std::wstring& error) {
    auto module = GetModuleHandleW(nullptr);
    auto resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
    if (!resource) { error = L"Um arquivo interno do instalador não foi encontrado."; return false; }
    auto loaded = LoadResource(module, resource);
    auto data = LockResource(loaded);
    const auto size = SizeofResource(module, resource);
    if (!data || !size) { error = L"Um arquivo interno do instalador está inválido."; return false; }
    auto file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"Não foi possível criar:\n" + output + L"\n\n" + lastError();
        return false;
    }
    DWORD written {};
    const bool ok = WriteFile(file, data, size, &written, nullptr) != 0;
    FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || written != size) {
        DeleteFileW(output.c_str());
        error = L"Falha ao gravar:\n" + output + L"\n\n" + lastError();
        return false;
    }
    return true;
}

bool createShortcut(const std::wstring& link, const std::wstring& target, std::wstring& error) {
    const auto slash = link.find_last_of(L'\\');
    if (slash != std::wstring::npos && !ensureDirectory(link.substr(0, slash), error)) return false;
    IShellLinkW* shell = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void**>(&shell)))) {
        error = L"Não foi possível criar o atalho do Chord Code.";
        return false;
    }
    shell->SetPath(target.c_str());
    shell->SetWorkingDirectory(appFolder().c_str());
    shell->SetIconLocation(target.c_str(), 0);
    shell->SetDescription(L"Chord Code — Audio Makers");
    IPersistFile* file = nullptr;
    bool ok = SUCCEEDED(shell->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)));
    if (ok) {
        ok = SUCCEEDED(file->Save(link.c_str(), TRUE));
        file->Release();
    }
    shell->Release();
    if (!ok) error = L"Não foi possível criar o atalho:\n" + link;
    return ok;
}

void setString(HKEY key, const wchar_t* name, const std::wstring& value) {
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

void setNumber(HKEY key, const wchar_t* name, DWORD value) {
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

// Copies this installer next to the app as its uninstaller and lists it in "Apps instalados".
bool registerUninstaller(const std::wstring& vstBundle, std::wstring& error) {
    wchar_t self[MAX_PATH] {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const auto uninstaller = appFolder() + L"\\Desinstalar Chord Code.exe";
    if (!ensureDirectory(appFolder(), error)) return false;
    if (!CopyFileW(self, uninstaller.c_str(), FALSE)) {
        error = L"Não foi possível preparar o desinstalador.\n\n" + lastError();
        return false;
    }
    HKEY key {};
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, uninstallKey, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &key, nullptr)
        != ERROR_SUCCESS) {
        error = L"Não foi possível registrar o Chord Code no Windows.";
        return false;
    }
    setString(key, L"DisplayName", L"Chord Code");
    setString(key, L"DisplayVersion", CCP_VERSION);
    setString(key, L"Publisher", L"Audio Makers");
    setString(key, L"DisplayIcon", installedApp.empty() ? uninstaller : installedApp);
    setString(key, L"InstallLocation", appFolder());
    setString(key, L"UninstallString", L"\"" + uninstaller + L"\" --uninstall");
    setString(key, L"VstBundle", vstBundle);
    setNumber(key, L"NoModify", 1);
    setNumber(key, L"NoRepair", 1);
    RegCloseKey(key);
    return true;
}

std::wstring browseFolder(HWND owner) {
    wchar_t display[MAX_PATH] {};
    BROWSEINFOW info {};
    info.hwndOwner = owner;
    info.pszDisplayName = display;
    info.lpszTitle = L"Selecione a pasta VST3";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    auto list = SHBrowseForFolderW(&info);
    if (!list) return {};
    wchar_t path[MAX_PATH] {};
    const bool ok = SHGetPathFromIDListW(list, path) != 0;
    CoTaskMemFree(list);
    return ok ? std::wstring(path) : std::wstring();
}
int uninstall() {
    if (MessageBoxW(nullptr, L"Remover o Chord Code deste computador?", appTitle, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;
    std::wstring error, bundle;
    HKEY key {};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, uninstallKey, 0, KEY_READ | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        wchar_t value[MAX_PATH] {};
        DWORD size = sizeof(value);
        if (RegQueryValueExW(key, L"VstBundle", nullptr, nullptr, reinterpret_cast<BYTE*>(value), &size) == ERROR_SUCCESS)
            bundle = value;
        RegCloseKey(key);
    }
    bool ok = (bundle.empty() || removeTreeIfPresent(bundle, error))
           && removeTreeIfPresent(appFolder() + L"\\Chord Code.exe", error);
    DeleteFileW(desktopLink().c_str());
    DeleteFileW(startMenuLink().c_str());
    RemoveDirectoryW((knownFolder(FOLDERID_CommonPrograms) + L"\\Audio Makers").c_str());
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, uninstallKey, KEY_WOW64_64KEY, 0);
    // The uninstaller cannot delete itself while running; Windows removes it on the next restart.
    wchar_t self[MAX_PATH] {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    MoveFileExW(self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileExW(appFolder().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    MessageBoxW(nullptr, ok ? L"O Chord Code foi removido." : error.c_str(), appTitle,
                MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONWARNING));
    return ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Interface: everything is painted with GDI+ in 640×500 logical units, scaled
// to the monitor DPI, so edges stay anti-aliased and text renders correctly.

using namespace Gdiplus;

enum class Page { options, installing, done };
enum class Hit { none, optionApp, optionShortcut, optionVst, changeFolder, secondary, primary };

struct State {
    Page page = Page::options;
    bool installApp = true, desktopShortcut = true, installVst = true;
    std::wstring vstRoot;
    float progress = 0.0f;
    std::wstring progressText;
    Hit hover = Hit::none, pressed = Hit::none;
    float dpiScale = 1.0f;
} state;

Color rgb(int r, int g, int b, int a = 255) { return Color(static_cast<BYTE>(a), static_cast<BYTE>(r), static_cast<BYTE>(g), static_cast<BYTE>(b)); }
const Color background = rgb(9, 10, 12), surface = rgb(17, 19, 23), surfaceHover = rgb(23, 26, 31),
            border = rgb(39, 43, 50), textPrimary = rgb(245, 246, 248), textSecondary = rgb(139, 146, 156),
            textQuiet = rgb(85, 92, 102), orange = rgb(255, 90, 10), orangeLight = rgb(255, 140, 58),
            amber = rgb(255, 178, 61), success = rgb(61, 220, 132);

RectF rect(float x, float y, float w, float h) { return RectF(x, y, w, h); }

void roundedPath(GraphicsPath& path, RectF r, float radius) {
    const float d = (std::min)(radius * 2.0f, (std::min)(r.Width, r.Height));
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

void fillRounded(Graphics& g, RectF r, float radius, const Brush& brush) {
    GraphicsPath path;
    roundedPath(path, r, radius);
    g.FillPath(&brush, &path);
}

void strokeRounded(Graphics& g, RectF r, float radius, Color colour, float width = 1.0f) {
    GraphicsPath path;
    roundedPath(path, r, radius);
    Pen pen(colour, width);
    g.DrawPath(&pen, &path);
}

void fillAccent(Graphics& g, RectF r, float radius) {
    LinearGradientBrush brush(PointF(r.X, r.Y + r.Height), PointF(r.X + r.Width, r.Y), orange, orangeLight);
    fillRounded(g, r, radius, brush);
}

void text(Graphics& g, const std::wstring& value, RectF r, float size, int style, Color colour,
          StringAlignment horizontal = StringAlignmentNear, StringAlignment vertical = StringAlignmentCenter) {
    FontFamily family(L"Segoe UI");
    Font font(&family, size, style, UnitPixel);
    StringFormat format;
    format.SetAlignment(horizontal);
    format.SetLineAlignment(vertical);
    format.SetTrimming(StringTrimmingEllipsisPath);
    SolidBrush brush(colour);
    g.DrawString(value.c_str(), -1, &font, r, &format, &brush);
}

void brandMark(Graphics& g, RectF r) {
    fillAccent(g, r, r.Width * 10.0f / 38.0f);
    SolidBrush bar(rgb(22, 9, 4));
    const float u = r.Width / 38.0f;
    const float heights[] { 10.0f, 20.0f, 14.0f, 24.0f };
    for (int i = 0; i < 4; ++i)
        fillRounded(g, rect(r.X + (8.0f + i * 6.5f) * u, r.Y + r.Height * 0.5f - heights[i] * 0.5f * u, 3.5f * u, heights[i] * u),
                    1.75f * u, bar);
}

void tick(Graphics& g, PointF c, float size, Color colour, float width) {
    Pen pen(colour, width);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    PointF points[] { PointF(c.X - size * 0.5f, c.Y), PointF(c.X - size * 0.15f, c.Y + size * 0.35f), PointF(c.X + size * 0.55f, c.Y - size * 0.4f) };
    g.DrawLines(&pen, points, 3);
}

RectF optionArea(Hit option) {
    switch (option) {
        case Hit::optionApp: return rect(32, 148, 576, 62);
        case Hit::optionShortcut: return rect(32, 218, 576, 62);
        default: return rect(32, 288, 576, 108);
    }
}
RectF folderField() { return rect(48, 342, 544, 38); }
RectF changeButton() { return rect(500, 346, 86, 30); }
RectF secondaryButton() { return rect(328, 432, 120, 44); }
RectF primaryButton() { return rect(460, 432, 148, 44); }

bool optionEnabled(Hit option) { return option != Hit::optionShortcut || state.installApp; }
bool optionChecked(Hit option) {
    return option == Hit::optionApp ? state.installApp : option == Hit::optionShortcut ? state.desktopShortcut : state.installVst;
}

void toggleSwitch(Graphics& g, RectF r, bool on, bool enabled) {
    if (on && enabled) fillAccent(g, r, r.Height * 0.5f);
    else { SolidBrush off(on ? rgb(70, 46, 34) : rgb(42, 47, 55)); fillRounded(g, r, r.Height * 0.5f, off); }
    SolidBrush knob(enabled ? textPrimary : rgb(120, 126, 136));
    const float k = r.Height - 6.0f;
    g.FillEllipse(&knob, on ? r.X + r.Width - k - 3.0f : r.X + 3.0f, r.Y + 3.0f, k, k);
}

void option(Graphics& g, Hit id, const wchar_t* title, const wchar_t* detail) {
    const auto r = optionArea(id);
    const bool enabled = optionEnabled(id), on = optionChecked(id) && enabled;
    SolidBrush fill(state.hover == id && enabled ? surfaceHover : surface);
    fillRounded(g, r, 14, fill);
    strokeRounded(g, r, 14, on ? rgb(255, 90, 10, 110) : border);
    text(g, title, rect(r.X + 20, r.Y + 11, r.Width - 110, 22), 15.5f, FontStyleBold, enabled ? textPrimary : textQuiet);
    text(g, detail, rect(r.X + 20, r.Y + 32, r.Width - 110, 20), 13, FontStyleRegular, enabled ? textSecondary : textQuiet);
    toggleSwitch(g, rect(r.X + r.Width - 64, r.Y + 18, 44, 26), on, enabled);
}

void button(Graphics& g, RectF r, const std::wstring& label, bool primary, Hit id) {
    const bool over = state.hover == id, down = state.pressed == id;
    if (primary) {
        fillAccent(g, r, 12);
        if (over || down) { SolidBrush light(rgb(255, 255, 255, down ? 18 : 34)); fillRounded(g, r, 12, light); }
        text(g, label, r, 15, FontStyleBold, rgb(22, 9, 4), StringAlignmentCenter);
    } else {
        SolidBrush fill(over ? surfaceHover : surface);
        fillRounded(g, r, 12, fill);
        strokeRounded(g, r, 12, over ? rgb(255, 90, 10, 170) : border);
        text(g, label, r, 15, FontStyleBold, textPrimary, StringAlignmentCenter);
    }
}

void paint(Graphics& g) {
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.Clear(background);

    // Warm light behind the brand.
    {
        GraphicsPath glow;
        glow.AddEllipse(-160.0f, -230.0f, 620.0f, 420.0f);
        PathGradientBrush brush(&glow);
        brush.SetCenterColor(rgb(255, 90, 10, 46));
        Color edge = rgb(255, 90, 10, 0);
        int count = 1;
        brush.SetSurroundColors(&edge, &count);
        g.FillPath(&brush, &glow);
    }

    brandMark(g, rect(32, 30, 56, 56));
    text(g, L"Chord Code", rect(104, 30, 360, 34), 26, FontStyleBold, textPrimary);
    text(g, std::wstring(L"Audio Makers  ·  versão ") + CCP_VERSION, rect(105, 62, 360, 22), 13.5f, FontStyleRegular, textSecondary);

    if (state.page == Page::options) {
        text(g, L"O QUE INSTALAR", rect(32, 118, 300, 20), 11.5f, FontStyleBold, orangeLight);
        option(g, Hit::optionApp, L"Aplicativo Chord Code", L"Abre sozinho, sem precisar de uma DAW");
        option(g, Hit::optionShortcut, L"Atalho na área de trabalho", L"Ícone do Chord Code na sua área de trabalho");
        option(g, Hit::optionVst, L"Plugin VST3", L"Para FL Studio, Ableton e outras DAWs");

        const auto field = folderField();
        SolidBrush fieldFill(rgb(10, 12, 15));
        fillRounded(g, field, 10, fieldFill);
        strokeRounded(g, field, 10, border);
        text(g, state.vstRoot, rect(field.X + 14, field.Y, field.Width - 120, field.Height), 13.5f, FontStyleRegular,
             state.installVst ? textPrimary : textQuiet);
        const auto change = changeButton();
        SolidBrush changeFill(state.hover == Hit::changeFolder && state.installVst ? rgb(36, 40, 47) : rgb(26, 29, 35));
        fillRounded(g, change, 8, changeFill);
        text(g, L"Alterar", change, 13, FontStyleBold, state.installVst ? textPrimary : textQuiet, StringAlignmentCenter);

        SolidBrush line(rgb(28, 31, 36));
        g.FillRectangle(&line, 32.0f, 414.0f, 576.0f, 1.0f);
        text(g, L"Feche o Chord Code e a DAW\nantes de instalar.", rect(32, 432, 300, 44), 13, FontStyleRegular, textSecondary);
        button(g, secondaryButton(), L"Cancelar", false, Hit::secondary);
        button(g, primaryButton(), L"Instalar", true, Hit::primary);
        return;
    }

    if (state.page == Page::installing) {
        text(g, L"INSTALANDO", rect(32, 196, 576, 20), 11.5f, FontStyleBold, orangeLight, StringAlignmentCenter);
        text(g, state.progressText, rect(32, 218, 576, 30), 17, FontStyleBold, textPrimary, StringAlignmentCenter);
        const auto track = rect(120, 268, 400, 8);
        SolidBrush trackFill(rgb(30, 33, 39));
        fillRounded(g, track, 4, trackFill);
        if (state.progress > 0.0f) fillAccent(g, rect(track.X, track.Y, (std::max)(8.0f, track.Width * state.progress), track.Height), 4);
        return;
    }

    // Done.
    SolidBrush halo(rgb(61, 220, 132, 30));
    g.FillEllipse(&halo, 288.0f, 116.0f, 64.0f, 64.0f);
    tick(g, PointF(320, 150), 24, success, 3.5f);
    text(g, L"Tudo pronto!", rect(32, 192, 576, 34), 24, FontStyleBold, textPrimary, StringAlignmentCenter);
    text(g, std::wstring(L"O Chord Code ") + CCP_VERSION + L" foi instalado.", rect(32, 226, 576, 24), 14, FontStyleRegular,
         textSecondary, StringAlignmentCenter);

    std::vector<std::pair<std::wstring, std::wstring>> items;
    if (!installedApp.empty())
        items.push_back({ L"Aplicativo", state.desktopShortcut ? L"No Menu Iniciar e na área de trabalho" : L"No Menu Iniciar" });
    if (state.installVst) items.push_back({ L"Plugin VST3", L"Faça uma nova busca de plugins na sua DAW" });
    float y = 272;
    for (const auto& [title, detail] : items) {
        const auto row = rect(120, y, 400, 54);
        SolidBrush rowFill(surface);
        fillRounded(g, row, 12, rowFill);
        strokeRounded(g, row, 12, border);
        tick(g, PointF(row.X + 26, row.Y + 27), 12, success, 2.4f);
        text(g, title, rect(row.X + 48, row.Y + 8, row.Width - 60, 20), 14.5f, FontStyleBold, textPrimary);
        text(g, detail, rect(row.X + 48, row.Y + 27, row.Width - 60, 18), 12.5f, FontStyleRegular, textSecondary);
        y += 62;
    }
    button(g, secondaryButton(), L"Fechar", false, Hit::secondary);
    button(g, primaryButton(), installedApp.empty() ? L"Concluir" : L"Abrir Chord Code", true, Hit::primary);
}

Hit hitTest(float x, float y) {
    const auto inside = [x, y](RectF r) { return r.Contains(x, y) != FALSE; };
    if (state.page == Page::installing) return Hit::none;
    if (inside(primaryButton())) return Hit::primary;
    if (inside(secondaryButton())) return Hit::secondary;
    if (state.page != Page::options) return Hit::none;
    if (inside(changeButton())) return Hit::changeFolder;
    for (auto id : { Hit::optionApp, Hit::optionShortcut, Hit::optionVst })
        if (inside(optionArea(id))) return id;
    return Hit::none;
}

void repaintNow(HWND window) {
    InvalidateRect(window, nullptr, FALSE);
    UpdateWindow(window);
}

void setProgress(HWND window, float value, const wchar_t* label) {
    state.progress = value;
    state.progressText = label;
    repaintNow(window);
}

bool install(HWND window) {
    std::wstring error, bundle;
    state.page = Page::installing;
    setProgress(window, 0.05f, L"Preparando...");

    if (state.installVst) {
        setProgress(window, 0.2f, L"Instalando o plugin VST3...");
        const auto& root = state.vstRoot;
        bundle = root + L"\\Chord Code.vst3";
        for (const auto& old : { bundle, root + L"\\Chord Code Pro.vst3", root + L"\\Audio Makers\\Chord Code Pro.vst3",
                                 root + L"\\Chord Code Pro 2.1.vst3", root + L"\\Chord Code Pro 2.1.1.vst3",
                                 root + L"\\Chord Code Pro 1.1.0.vst3" })
            if (!removeTreeIfPresent(old, error)) break;
        const auto folder = bundle + L"\\Contents\\x86_64-win";
        if (error.empty() && ensureDirectory(folder, error)) writePayload(payloadVst, folder + L"\\Chord Code.vst3", error);
    }

    if (error.empty() && state.installApp) {
        setProgress(window, 0.55f, L"Instalando o aplicativo...");
        const auto app = appFolder() + L"\\Chord Code.exe";
        if (ensureDirectory(appFolder(), error) && removeTreeIfPresent(app, error) && writePayload(payloadApp, app, error)) {
            setProgress(window, 0.8f, L"Criando atalhos...");
            if (createShortcut(startMenuLink(), app, error)
                && (!state.desktopShortcut || createShortcut(desktopLink(), app, error))) {
                if (!state.desktopShortcut) DeleteFileW(desktopLink().c_str());
                installedApp = app;
            }
        }
    }

    if (!error.empty()) {
        MessageBoxW(window, error.c_str(), L"Falha na instalação", MB_OK | MB_ICONERROR);
        state.page = Page::options;
        repaintNow(window);
        return false;
    }

    setProgress(window, 0.95f, L"Finalizando...");
    if (!registerUninstaller(bundle, error)) MessageBoxW(window, error.c_str(), appTitle, MB_OK | MB_ICONWARNING);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    state.page = Page::done;
    repaintNow(window);
    return true;
}

void activate(HWND window, Hit id) {
    if (id == Hit::secondary) { DestroyWindow(window); return; }
    if (state.page == Page::done && id == Hit::primary) {
        if (!installedApp.empty()) ShellExecuteW(nullptr, L"open", installedApp.c_str(), nullptr, appFolder().c_str(), SW_SHOWNORMAL);
        DestroyWindow(window);
        return;
    }
    if (state.page != Page::options) return;
    if (id == Hit::primary) {
        if (!state.installApp && !state.installVst) {
            MessageBoxW(window, L"Escolha o aplicativo, o plugin VST3 ou os dois.", appTitle, MB_OK | MB_ICONINFORMATION);
            return;
        }
        install(window);
    } else if (id == Hit::changeFolder && state.installVst) {
        const auto selected = browseFolder(window);
        if (!selected.empty()) state.vstRoot = selected;
    } else if (id == Hit::optionApp) {
        state.installApp = !state.installApp;
    } else if (id == Hit::optionShortcut && state.installApp) {
        state.desktopShortcut = !state.desktopShortcut;
    } else if (id == Hit::optionVst) {
        state.installVst = !state.installVst;
    }
    InvalidateRect(window, nullptr, FALSE);
}

void applyDarkTitleBar(HWND window) {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    const COLORREF caption = RGB(9, 10, 12);
    DwmSetWindowAttribute(window, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps {};
            auto dc = BeginPaint(window, &ps);
            RECT client {};
            GetClientRect(window, &client);
            // Double-buffered so hover changes never flicker.
            Bitmap buffer(client.right, client.bottom, PixelFormat32bppARGB);
            {
                Graphics g(&buffer);
                g.ScaleTransform(state.dpiScale, state.dpiScale);
                paint(g);
            }
            Graphics screen(dc);
            screen.DrawImage(&buffer, 0, 0);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE: {
            const auto hit = hitTest(GET_X_LPARAM(lParam) / state.dpiScale, GET_Y_LPARAM(lParam) / state.dpiScale);
            if (hit != state.hover) {
                state.hover = hit;
                SetCursor(LoadCursorW(nullptr, hit == Hit::none ? IDC_ARROW : IDC_HAND));
                InvalidateRect(window, nullptr, FALSE);
            }
            TRACKMOUSEEVENT track { sizeof(track), TME_LEAVE, window, 0 };
            TrackMouseEvent(&track);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) { SetCursor(LoadCursorW(nullptr, state.hover == Hit::none ? IDC_ARROW : IDC_HAND)); return TRUE; }
            break;
        case WM_MOUSELEAVE:
            state.hover = Hit::none;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONDOWN:
            state.pressed = hitTest(GET_X_LPARAM(lParam) / state.dpiScale, GET_Y_LPARAM(lParam) / state.dpiScale);
            SetCapture(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_LBUTTONUP: {
            ReleaseCapture();
            const auto hit = hitTest(GET_X_LPARAM(lParam) / state.dpiScale, GET_Y_LPARAM(lParam) / state.dpiScale);
            const auto pressed = state.pressed;
            state.pressed = Hit::none;
            InvalidateRect(window, nullptr, FALSE);
            if (hit != Hit::none && hit == pressed) activate(window, hit);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) activate(window, Hit::primary);
            else if (wParam == VK_ESCAPE && state.page != Page::installing) DestroyWindow(window);
            return 0;
        case WM_DPICHANGED: {
            state.dpiScale = HIWORD(wParam) / 96.0f;
            const auto* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_CLOSE:
            if (state.page == Page::installing) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// `--screenshot <folder>` renders each page to PNG for build previews.
int saveScreenshots(const std::wstring& folder) {
    CLSID png {};
    UINT count = 0, size = 0;
    GetImageEncodersSize(&count, &size);
    std::vector<BYTE> storage(size);
    auto* encoders = reinterpret_cast<ImageCodecInfo*>(storage.data());
    GetImageEncoders(count, size, encoders);
    for (UINT i = 0; i < count; ++i)
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) png = encoders[i].Clsid;
    state.vstRoot = L"C:\\Program Files\\Common Files\\VST3";
    const auto render = [&](const wchar_t* name) {
        Bitmap image(static_cast<INT>(windowWidth), static_cast<INT>(windowHeight), PixelFormat32bppARGB);
        { Graphics g(&image); paint(g); }
        image.Save((folder + L"\\" + name).c_str(), &png, nullptr);
    };
    render(L"installer-options.png");
    state.page = Page::installing;
    state.progress = 0.55f;
    state.progressText = L"Instalando o aplicativo...";
    render(L"installer-progress.png");
    state.page = Page::done;
    installedApp = L"C:\\Program Files\\Audio Makers\\Chord Code\\Chord Code.exe";
    render(L"installer-done.png");
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken {};
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    int argc = 0;
    auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int result = 0;
    if (argc > 1 && wcscmp(argv[1], L"--uninstall") == 0) result = uninstall();
    else if (argc > 2 && wcscmp(argv[1], L"--screenshot") == 0) result = saveScreenshots(argv[2]);
    else {
        state.vstRoot = knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3";
        auto dc = GetDC(nullptr);
        state.dpiScale = (std::max)(1.0f, static_cast<float>(GetDeviceCaps(dc, LOGPIXELSX)) / 96.0f);
        ReleaseDC(nullptr, dc);

        WNDCLASSEXW wc { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = windowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"ChordCodeInstaller";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        wc.hIconSm = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        RegisterClassExW(&wc);

        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT bounds { 0, 0, static_cast<LONG>(windowWidth * state.dpiScale), static_cast<LONG>(windowHeight * state.dpiScale) };
        AdjustWindowRectEx(&bounds, style, FALSE, WS_EX_APPWINDOW);
        const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        auto window = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"Instalar Chord Code", style,
                                      (GetSystemMetrics(SM_CXSCREEN) - width) / 2, (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
                                      width, height, nullptr, nullptr, instance, nullptr);
        if (window) {
            applyDarkTitleBar(window);
            ShowWindow(window, show);
            UpdateWindow(window);
            MSG message {};
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            result = static_cast<int>(message.wParam);
        } else {
            result = 1;
        }
    }
    LocalFree(argv);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return result;
}
