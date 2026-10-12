#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#include <string>

#ifndef CCP_VERSION
#define CCP_VERSION L"1.2.2"
#endif

namespace {
constexpr int idVstPath = 101;
constexpr int idVstBrowse = 102;
constexpr int idInstall = 103;
constexpr int idCancel = 104;
constexpr int idOptionVst = 105;
constexpr int idOptionApp = 106;
constexpr int idOptionShortcut = 107;
constexpr int payloadVst = 201;
constexpr int payloadApp = 202;
constexpr wchar_t uninstallKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AudioMakers.ChordCode";
constexpr wchar_t appTitle[] = L"Chord Code";

HWND vstPath {}, vstBrowse {}, installButton {}, cancelButton {};
HFONT titleFont {}, subtitleFont {}, bodyFont {}, captionFont {};
HBRUSH backgroundBrush {}, surfaceBrush {}, editBrush {};
HICON brandIcon {};
float dpiScale = 1.0f;
bool installVst = true, installApp = true, desktopShortcut = true;
bool finished = false;
std::wstring installedApp;

int px(int value) { return static_cast<int>(value * dpiScale + 0.5f); }

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

std::wstring textOf(HWND control) {
    const int size = GetWindowTextLengthW(control) + 1;
    std::wstring value(static_cast<size_t>(size), L'\0');
    GetWindowTextW(control, value.data(), size);
    value.resize(wcslen(value.c_str()));
    return value;
}

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

HWND addControl(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id) {
    auto control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
                                   px(x), px(y), px(width), px(height), parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), TRUE);
    return control;
}

void drawText(HDC dc, const wchar_t* text, RECT bounds, HFONT font, COLORREF colour,
              UINT format = DT_LEFT | DT_SINGLELINE | DT_VCENTER) {
    const auto previous = SelectObject(dc, font);
    SetTextColor(dc, colour);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text, -1, &bounds, format);
    SelectObject(dc, previous);
}

void fillRound(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border) {
    auto brush = CreateSolidBrush(fill);
    auto pen = CreatePen(PS_SOLID, px(1), border);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void updateControls() {
    EnableWindow(vstPath, installVst && !finished);
    EnableWindow(vstBrowse, installVst && !finished);
    EnableWindow(installButton, finished || installVst || installApp);
    SetWindowTextW(installButton, finished ? (installedApp.empty() ? L"Concluir" : L"Abrir Chord Code") : L"Instalar");
    SetWindowTextW(cancelButton, finished ? L"Fechar" : L"Cancelar");
}

bool install(HWND window) {
    std::wstring error;
    std::wstring bundle;
    if (installVst) {
        const auto root = textOf(vstPath);
        if (root.empty()) {
            MessageBoxW(window, L"Escolha uma pasta para instalar o VST3.", appTitle, MB_OK | MB_ICONWARNING);
            return false;
        }
        bundle = root + L"\\Chord Code.vst3";
        for (const auto& old : { bundle, root + L"\\Chord Code Pro.vst3", root + L"\\Audio Makers\\Chord Code Pro.vst3",
                                 root + L"\\Chord Code Pro 2.1.vst3", root + L"\\Chord Code Pro 2.1.1.vst3",
                                 root + L"\\Chord Code Pro 1.1.0.vst3" })
            if (!removeTreeIfPresent(old, error)) {
                MessageBoxW(window, error.c_str(), L"Falha na atualização", MB_OK | MB_ICONERROR);
                return false;
            }
        const auto folder = bundle + L"\\Contents\\x86_64-win";
        if (!ensureDirectory(folder, error) || !writePayload(payloadVst, folder + L"\\Chord Code.vst3", error)) {
            MessageBoxW(window, error.c_str(), L"Falha na instalação", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    if (installApp) {
        const auto app = appFolder() + L"\\Chord Code.exe";
        if (!ensureDirectory(appFolder(), error) || !removeTreeIfPresent(app, error)
            || !writePayload(payloadApp, app, error)
            || !createShortcut(startMenuLink(), app, error)
            || (desktopShortcut && !createShortcut(desktopLink(), app, error))) {
            MessageBoxW(window, error.c_str(), L"Falha na instalação", MB_OK | MB_ICONERROR);
            return false;
        }
        if (!desktopShortcut) DeleteFileW(desktopLink().c_str());
        installedApp = app;
    }

    if (!registerUninstaller(bundle, error))
        MessageBoxW(window, error.c_str(), appTitle, MB_OK | MB_ICONWARNING);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
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

void drawButton(const DRAWITEMSTRUCT& item) {
    const bool primary = item.CtlID == idInstall;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    FillRect(item.hDC, &item.rcItem, backgroundBrush);
    COLORREF fill = primary ? (pressed ? RGB(226, 78, 8) : RGB(255, 90, 10)) : (pressed ? RGB(42, 46, 50) : RGB(21, 24, 29));
    if (disabled) fill = RGB(40, 34, 30);
    const COLORREF border = primary ? fill : RGB(58, 64, 74);
    fillRound(item.hDC, item.rcItem, px(10), fill, border);
    wchar_t text[64] {};
    GetWindowTextW(item.hwndItem, text, 64);
    drawText(item.hDC, text, item.rcItem, bodyFont, primary ? RGB(22, 9, 4) : RGB(245, 246, 248),
             DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

// Option rows: an orange check square, a title and a short description.
void drawOption(const DRAWITEMSTRUCT& item) {
    const bool checked = item.CtlID == idOptionVst ? installVst : item.CtlID == idOptionApp ? installApp : desktopShortcut;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    FillRect(item.hDC, &item.rcItem, backgroundBrush);
    RECT card = item.rcItem;
    fillRound(item.hDC, card, px(12), RGB(21, 24, 29), checked && !disabled ? RGB(150, 64, 20) : RGB(39, 43, 50));
    RECT box { card.left + px(16), card.top + (card.bottom - card.top - px(20)) / 2, 0, 0 };
    box.right = box.left + px(20);
    box.bottom = box.top + px(20);
    fillRound(item.hDC, box, px(6), checked ? RGB(255, 90, 10) : RGB(13, 15, 18), checked ? RGB(255, 90, 10) : RGB(70, 76, 86));
    if (checked) {
        auto pen = CreatePen(PS_SOLID, px(2), RGB(22, 9, 4));
        const auto oldPen = SelectObject(item.hDC, pen);
        MoveToEx(item.hDC, box.left + px(5), box.top + px(10), nullptr);
        LineTo(item.hDC, box.left + px(9), box.top + px(14));
        LineTo(item.hDC, box.left + px(15), box.top + px(6));
        SelectObject(item.hDC, oldPen);
        DeleteObject(pen);
    }
    const wchar_t* title = item.CtlID == idOptionVst ? L"Plugin VST3"
                         : item.CtlID == idOptionApp ? L"Aplicativo Chord Code" : L"Atalho na área de trabalho";
    const wchar_t* detail = item.CtlID == idOptionVst ? L"Para FL Studio, Ableton e outras DAWs"
                          : item.CtlID == idOptionApp ? L"Abre sozinho, sem precisar de uma DAW"
                                                      : L"Ícone do Chord Code na sua área de trabalho";
    const COLORREF titleColour = disabled ? RGB(85, 92, 102) : RGB(245, 246, 248);
    drawText(item.hDC, title, { box.right + px(14), card.top + px(9), card.right - px(12), card.top + px(31) },
             bodyFont, titleColour);
    drawText(item.hDC, detail, { box.right + px(14), card.top + px(30), card.right - px(12), card.bottom - px(8) },
             subtitleFont, RGB(139, 146, 156));
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        backgroundBrush = CreateSolidBrush(RGB(9, 10, 12));
        surfaceBrush = CreateSolidBrush(RGB(15, 17, 21));
        editBrush = CreateSolidBrush(RGB(13, 15, 18));
        const auto makeFont = [](int height, int weight) {
            return CreateFontW(px(height), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        };
        titleFont = makeFont(30, FW_BOLD);
        subtitleFont = makeFont(14, FW_NORMAL);
        bodyFont = makeFont(16, FW_SEMIBOLD);
        captionFont = makeFont(12, FW_BOLD);
        brandIcon = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                        IMAGE_ICON, px(64), px(64), LR_DEFAULTCOLOR));

        addControl(window, L"BUTTON", L"", BS_OWNERDRAW, 32, 156, 606, 56, idOptionApp);
        addControl(window, L"BUTTON", L"", BS_OWNERDRAW, 32, 220, 606, 56, idOptionShortcut);
        addControl(window, L"BUTTON", L"", BS_OWNERDRAW, 32, 284, 606, 56, idOptionVst);
        vstPath = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, 32, 374, 468, 38, idVstPath);
        vstBrowse = addControl(window, L"BUTTON", L"Selecionar", BS_OWNERDRAW, 512, 374, 126, 38, idVstBrowse);
        cancelButton = addControl(window, L"BUTTON", L"Cancelar", BS_OWNERDRAW, 344, 456, 136, 44, idCancel);
        installButton = addControl(window, L"BUTTON", L"Instalar", BS_OWNERDRAW | BS_DEFPUSHBUTTON, 490, 456, 148, 44, idInstall);

        SetWindowTextW(vstPath, (knownFolder(FOLDERID_ProgramFilesCommon) + L"\\VST3").c_str());
        SendMessageW(vstPath, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(px(11), px(11)));
        updateControls();
        return 0;
    }

    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == idInstall) {
            if (finished) {
                if (!installedApp.empty()) ShellExecuteW(nullptr, L"open", installedApp.c_str(), nullptr, appFolder().c_str(), SW_SHOWNORMAL);
                DestroyWindow(window);
            } else if (install(window)) {
                finished = true;
                for (int option : { idOptionVst, idOptionApp, idOptionShortcut }) EnableWindow(GetDlgItem(window, option), FALSE);
                updateControls();
                InvalidateRect(window, nullptr, TRUE);
            }
        } else if (id == idCancel) DestroyWindow(window);
        else if (id == idVstBrowse) {
            const auto selected = browseFolder(window);
            if (!selected.empty()) SetWindowTextW(vstPath, selected.c_str());
        } else if (!finished && (id == idOptionVst || id == idOptionApp || id == idOptionShortcut)) {
            if (id == idOptionVst) installVst = !installVst;
            if (id == idOptionApp) { installApp = !installApp; EnableWindow(GetDlgItem(window, idOptionShortcut), installApp); }
            if (id == idOptionShortcut) desktopShortcut = !desktopShortcut;
            if (!installApp) desktopShortcut = false;
            InvalidateRect(GetDlgItem(window, idOptionShortcut), nullptr, TRUE);
            InvalidateRect(GetDlgItem(window, id), nullptr, TRUE);
            updateControls();
        }
        return 0;
    }

    if (message == WM_DRAWITEM) {
        const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item.CtlID == idOptionVst || item.CtlID == idOptionApp || item.CtlID == idOptionShortcut) drawOption(item);
        else drawButton(item);
        return TRUE;
    }

    if (message == WM_PAINT) {
        PAINTSTRUCT ps {};
        auto dc = BeginPaint(window, &ps);
        RECT client {};
        GetClientRect(window, &client);
        FillRect(dc, &client, backgroundBrush);

        RECT header { 0, 0, client.right, px(124) };
        FillRect(dc, &header, surfaceBrush);
        RECT accent { 0, 0, client.right, px(3) };
        auto orange = CreateSolidBrush(RGB(255, 90, 10));
        FillRect(dc, &accent, orange);
        DeleteObject(orange);

        if (brandIcon) DrawIconEx(dc, px(32), px(30), brandIcon, px(64), px(64), 0, nullptr, DI_NORMAL);
        drawText(dc, L"CHORD CODE", { px(114), px(30), px(620), px(66) }, titleFont, RGB(245, 246, 248));
        const std::wstring subtitle = std::wstring(L"Audio Makers  •  Versão ") + CCP_VERSION;
        drawText(dc, subtitle.c_str(), { px(115), px(68), px(620), px(92) }, subtitleFont, RGB(139, 146, 156));

        drawText(dc, finished ? L"INSTALAÇÃO CONCLUÍDA" : L"O QUE INSTALAR", { px(32), px(132), px(638), px(152) },
                 captionFont, RGB(255, 140, 58));
        drawText(dc, L"Pasta do VST3", { px(32), px(350), px(638), px(370) }, subtitleFont, RGB(139, 146, 156));

        const wchar_t* note = finished
            ? (installedApp.empty() ? L"Faça uma nova busca de plugins VST3 na sua DAW."
                                    : L"O Chord Code está no Menu Iniciar e na área de trabalho.")
            : L"Feche o Chord Code e a DAW antes de instalar ou atualizar.";
        drawText(dc, note, { px(32), px(418), px(638), px(442) }, subtitleFont,
                 finished ? RGB(61, 220, 132) : RGB(139, 146, 156));
        EndPaint(window, &ps);
        return 0;
    }

    if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC) {
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(239, 240, 238));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(13, 15, 18));
        return reinterpret_cast<LRESULT>(editBrush);
    }

    if (message == WM_DESTROY) {
        if (brandIcon) DestroyIcon(brandIcon);
        for (auto object : { reinterpret_cast<HGDIOBJ>(titleFont), reinterpret_cast<HGDIOBJ>(subtitleFont),
                             reinterpret_cast<HGDIOBJ>(bodyFont), reinterpret_cast<HGDIOBJ>(captionFont),
                             reinterpret_cast<HGDIOBJ>(backgroundBrush), reinterpret_cast<HGDIOBJ>(surfaceBrush),
                             reinterpret_cast<HGDIOBJ>(editBrush) })
            if (object) DeleteObject(object);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (wcsstr(GetCommandLineW(), L"--uninstall") != nullptr) {
        const int result = uninstall();
        CoUninitialize();
        return result;
    }

    auto dc = GetDC(nullptr);
    dpiScale = static_cast<float>(GetDeviceCaps(dc, LOGPIXELSX)) / 96.0f;
    ReleaseDC(nullptr, dc);
    if (dpiScale < 1.0f) dpiScale = 1.0f;

    WNDCLASSEXW wc { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ChordCodeInstaller";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = CreateSolidBrush(RGB(9, 10, 12));
    wc.hIcon = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    wc.hIconSm = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    RegisterClassExW(&wc);

    RECT bounds { 0, 0, px(670), px(530) };
    AdjustWindowRectEx(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    auto window = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"Instalar Chord Code",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
