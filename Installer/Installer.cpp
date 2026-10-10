#include <windows.h>
#include <shlobj.h>
#include <string>

namespace {
constexpr int idVstPath = 101;
constexpr int idVstBrowse = 102;
constexpr int idInstall = 103;
constexpr int idCancel = 104;
constexpr int payloadVst = 201;

HWND vstPath {};
HFONT titleFont {}, subtitleFont {}, bodyFont {}, captionFont {};
HBRUSH backgroundBrush {}, surfaceBrush {}, editBrush {};
HICON brandIcon {};
float dpiScale = 1.0f;

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
        error = L"Feche sua DAW e tente novamente. Não foi possível substituir:\n" + path + L"\n\n" + lastError();
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
    error = L"Feche sua DAW e tente novamente. Não foi possível substituir:\n" + path;
    return false;
}

bool writePayload(int resourceId, const std::wstring& output, std::wstring& error) {
    auto module = GetModuleHandleW(nullptr);
    auto resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10));
    if (!resource) { error = L"O plugin interno do instalador não foi encontrado."; return false; }
    auto loaded = LoadResource(module, resource);
    auto data = LockResource(loaded);
    const auto size = SizeofResource(module, resource);
    if (!data || !size) { error = L"O plugin interno está inválido."; return false; }
    auto file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
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

void install(HWND window) {
    std::wstring error;
    const auto root = textOf(vstPath);
    if (root.empty()) {
        MessageBoxW(window, L"Escolha uma pasta para instalar o VST3.",
                    L"Chord Code", MB_OK | MB_ICONWARNING);
        return;
    }

    const auto currentBundle = root + L"\\Chord Code.vst3";
    const auto previousBundle = root + L"\\Chord Code Pro.vst3";
    const auto legacyBundle = root + L"\\Audio Makers\\Chord Code Pro.vst3";
    const auto versionedLoose = root + L"\\Chord Code Pro 2.1.vst3";
    const auto versionedLoose211 = root + L"\\Chord Code Pro 2.1.1.vst3";
    const auto versionedLoose110 = root + L"\\Chord Code Pro 1.1.0.vst3";
    if (!removeTreeIfPresent(currentBundle, error)
        || !removeTreeIfPresent(previousBundle, error)
        || !removeTreeIfPresent(legacyBundle, error)
        || !removeTreeIfPresent(versionedLoose, error)
        || !removeTreeIfPresent(versionedLoose211, error)
        || !removeTreeIfPresent(versionedLoose110, error)) {
        MessageBoxW(window, error.c_str(), L"Falha na atualização", MB_OK | MB_ICONERROR);
        return;
    }

    const auto folder = currentBundle + L"\\Contents\\x86_64-win";
    if (!ensureDirectory(folder, error)
        || !writePayload(payloadVst, folder + L"\\Chord Code.vst3", error)) {
        MessageBoxW(window, error.c_str(), L"Falha na instalação", MB_OK | MB_ICONERROR);
        return;
    }

    MessageBoxW(window,
        L"CHORD CODE 1.2.1 foi instalado com sucesso.\n\n"
        L"Abra o gerenciador de plugins da sua DAW e faça uma nova busca por VST3.",
        L"Audio Makers", MB_OK | MB_ICONINFORMATION);
    DestroyWindow(window);
}

void drawButton(const DRAWITEMSTRUCT& item) {
    const bool primary = item.CtlID == idInstall;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF fill = primary
        ? (pressed ? RGB(226, 88, 18) : RGB(255, 112, 31))
        : (pressed ? RGB(42, 46, 50) : RGB(27, 30, 34));
    const COLORREF border = primary ? RGB(255, 112, 31) : RGB(67, 72, 78);
    auto brush = CreateSolidBrush(fill);
    auto pen = CreatePen(PS_SOLID, px(1), border);
    const auto oldBrush = SelectObject(item.hDC, brush);
    const auto oldPen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom,
              px(8), px(8));
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    wchar_t text[64] {};
    GetWindowTextW(item.hwndItem, text, 64);
    auto bounds = item.rcItem;
    drawText(item.hDC, text, bounds, bodyFont, RGB(247, 247, 245),
             DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        backgroundBrush = CreateSolidBrush(RGB(13, 15, 17));
        surfaceBrush = CreateSolidBrush(RGB(20, 23, 26));
        editBrush = CreateSolidBrush(RGB(24, 27, 30));
        titleFont = CreateFontW(px(28), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        subtitleFont = CreateFontW(px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        bodyFont = CreateFontW(px(17), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        captionFont = CreateFontW(px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        brandIcon = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                        IMAGE_ICON, px(72), px(72), LR_DEFAULTCOLOR));

        vstPath = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                             32, 224, 468, 38, idVstPath);
        addControl(window, L"BUTTON", L"Selecionar", BS_OWNERDRAW,
                   512, 224, 126, 38, idVstBrowse);
        addControl(window, L"BUTTON", L"Cancelar", BS_OWNERDRAW,
                   366, 324, 126, 42, idCancel);
        addControl(window, L"BUTTON", L"Instalar", BS_OWNERDRAW | BS_DEFPUSHBUTTON,
                   504, 324, 134, 42, idInstall);

        wchar_t common[MAX_PATH] {};
        SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES_COMMON | CSIDL_FLAG_CREATE,
                         nullptr, SHGFP_TYPE_CURRENT, common);
        SetWindowTextW(vstPath, (std::wstring(common) + L"\\VST3").c_str());
        SendMessageW(vstPath, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELPARAM(px(11), px(11)));
        return 0;
    }

    if (message == WM_COMMAND) {
        const int id = LOWORD(wParam);
        if (id == idInstall) install(window);
        else if (id == idCancel) DestroyWindow(window);
        else if (id == idVstBrowse) {
            const auto selected = browseFolder(window);
            if (!selected.empty()) SetWindowTextW(vstPath, selected.c_str());
        }
        return 0;
    }

    if (message == WM_DRAWITEM) {
        drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    }

    if (message == WM_PAINT) {
        PAINTSTRUCT ps {};
        auto dc = BeginPaint(window, &ps);
        RECT client {};
        GetClientRect(window, &client);
        FillRect(dc, &client, backgroundBrush);

        RECT header { 0, 0, client.right, px(126) };
        FillRect(dc, &header, surfaceBrush);
        RECT accent { 0, 0, client.right, px(4) };
        auto orange = CreateSolidBrush(RGB(255, 112, 31));
        FillRect(dc, &accent, orange);
        DeleteObject(orange);

        if (brandIcon) DrawIconEx(dc, px(32), px(28), brandIcon, px(72), px(72), 0, nullptr, DI_NORMAL);
        drawText(dc, L"CHORD CODE", { px(124), px(30), px(620), px(66) },
                 titleFont, RGB(248, 248, 246));
        drawText(dc, L"Audio Makers  •  Versão 1.2.1  •  VST3", { px(125), px(69), px(620), px(94) },
                 subtitleFont, RGB(166, 170, 174));

        drawText(dc, L"INSTALAÇÃO DO VST3", { px(32), px(150), px(638), px(174) },
                 captionFont, RGB(255, 112, 31));
        drawText(dc, L"VST3 para FL Studio e outras DAWs", { px(32), px(178), px(638), px(204) },
                 bodyFont, RGB(238, 239, 237));
        drawText(dc, L"Pasta de destino", { px(32), px(202), px(638), px(222) },
                 subtitleFont, RGB(148, 153, 158));

        drawText(dc, L"Feche a DAW antes de instalar ou atualizar.",
                 { px(32), px(276), px(638), px(300) }, subtitleFont, RGB(148, 153, 158));
        auto linePen = CreatePen(PS_SOLID, px(1), RGB(43, 47, 51));
        const auto oldPen = SelectObject(dc, linePen);
        MoveToEx(dc, px(32), px(306), nullptr);
        LineTo(dc, px(638), px(306));
        SelectObject(dc, oldPen);
        DeleteObject(linePen);

        EndPaint(window, &ps);
        return 0;
    }

    if (message == WM_CTLCOLOREDIT) {
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(239, 240, 238));
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(24, 27, 30));
        return reinterpret_cast<LRESULT>(editBrush);
    }

    if (message == WM_DESTROY) {
        if (brandIcon) DestroyIcon(brandIcon);
        for (auto object : { reinterpret_cast<HGDIOBJ>(titleFont),
                             reinterpret_cast<HGDIOBJ>(subtitleFont),
                             reinterpret_cast<HGDIOBJ>(bodyFont),
                             reinterpret_cast<HGDIOBJ>(captionFont),
                             reinterpret_cast<HGDIOBJ>(backgroundBrush),
                             reinterpret_cast<HGDIOBJ>(surfaceBrush),
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

    auto dc = GetDC(nullptr);
    dpiScale = static_cast<float>(GetDeviceCaps(dc, LOGPIXELSX)) / 96.0f;
    ReleaseDC(nullptr, dc);
    if (dpiScale < 1.0f) dpiScale = 1.0f;

    WNDCLASSEXW wc { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ChordCodeProInstaller";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = CreateSolidBrush(RGB(13, 15, 17));
    wc.hIcon = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                   0, 0, LR_DEFAULTSIZE));
    wc.hIconSm = reinterpret_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                     16, 16, LR_DEFAULTCOLOR));
    RegisterClassExW(&wc);

    RECT bounds { 0, 0, px(670), px(398) };
    AdjustWindowRectEx(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_APPWINDOW);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    auto window = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName,
                                  L"CHORD CODE — Instalador VST3",
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
