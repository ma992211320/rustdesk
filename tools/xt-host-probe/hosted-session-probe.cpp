// Windows integration probe: launch an adjacent XT candidate into a real parent panel.
#include <windows.h>
#include <shellapi.h>
#include <string>

static HWND panel;
static HANDLE engine;
static LRESULT CALLBACK host_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE: {
        RECT r{};
        GetClientRect(window, &r);
        if (panel) {
            MoveWindow(panel, 0, 0, r.right, r.bottom, TRUE);
            HWND child = GetWindow(panel, GW_CHILD);
            if (child) MoveWindow(child, 0, 0, r.right, r.bottom, TRUE);
        }
        return 0;
    }
    case WM_TIMER:
        if (engine && WaitForSingleObject(engine, 0) == WAIT_OBJECT_0) DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window,message,w,l);
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring id = argv && argc == 2 ? argv[1] : L"";
    if (argv) LocalFree(argv);
    if (id.size()<6 || id.size()>16 || id.find_first_not_of(L"0123456789")!=std::wstring::npos) return 2;
    wchar_t module[MAX_PATH]{};
    auto length = GetModuleFileNameW(nullptr,module,MAX_PATH);
    if (!length || length >= MAX_PATH) return 3;
    std::wstring dir(module,length);
    dir.resize(dir.find_last_of(L'\\')+1);
    const std::wstring exe = dir + L"XTRemoteEngine.exe";
    WNDCLASSW wc{};
    wc.lpfnWndProc=host_proc; wc.hInstance=instance; wc.lpszClassName=L"XTHostedSessionProbe";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if (!RegisterClassW(&wc)) return 4;
    HWND window=CreateWindowW(wc.lpszClassName,L"XT remote session test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,1000,700,nullptr,nullptr,instance,nullptr);
    if (!window) return 5;
    RECT r{};GetClientRect(window,&r);
    panel=CreateWindowW(L"STATIC",nullptr,WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,
        0,0,r.right,r.bottom,window,nullptr,instance,nullptr);
    if (!panel) return 6;
    std::wstring cmd=L"\""+exe+L"\" --xt-hosted-desktop "+id+L" "+
        std::to_wstring(reinterpret_cast<UINT_PTR>(panel))+L" "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{};si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&si,&pi)) return 7;
    CloseHandle(pi.hThread);engine=pi.hProcess;
    ShowWindow(window,show);SetTimer(window,1,500,nullptr);
    MSG msg{};
    while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg);DispatchMessageW(&msg); }
    CloseHandle(engine);
    return 0;
}
