#include <fcntl.h>
#include <io.h>
#include <windows.h>

HBITMAP g_screen = nullptr;

int g_width = 0;
int g_height = 0;

int g_x = 0;
int g_y = 0;

int g_mx = 0;
int g_my = 0;

float g_zoom = 1;

bool g_is_dragging = false;

void UpdateScreenshot() {
    if (g_screen) {
        DeleteObject(g_screen);
    }
    
    g_width = GetSystemMetrics(SM_CXSCREEN);
    g_height = GetSystemMetrics(SM_CYSCREEN);
    
    HDC h_screen_dc = GetDC(nullptr);
    if (!h_screen_dc) return;
    
    HDC h_memory_dc = CreateCompatibleDC(h_screen_dc);
    if (!h_memory_dc) {
        ReleaseDC(nullptr, h_screen_dc);
        return;
    }
    
    g_screen = CreateCompatibleBitmap(h_screen_dc, g_width, g_height);
    if (!g_screen) {
        DeleteDC(h_memory_dc);
        ReleaseDC(nullptr, h_screen_dc);
        return;
    }
    
    HGDIOBJ h_old_bmp = SelectObject(h_memory_dc, g_screen);
    BitBlt(h_memory_dc, 0, 0, g_width, g_height, h_screen_dc, 0, 0, SRCCOPY);
    SelectObject(h_memory_dc, h_old_bmp);
    
    DeleteDC(h_memory_dc);
    ReleaseDC(nullptr, h_screen_dc);
}

void center_image(HWND h_wnd) {
    RECT client_rect;
    GetClientRect(h_wnd, &client_rect);
    int client_width = client_rect.right - client_rect.left;
    int client_height = client_rect.bottom - client_rect.top;
    
    int img_width = g_width * g_zoom;
    int img_height = g_height * g_zoom;
    
    g_x = (client_width - img_width) / 2;
    g_y = (client_height - img_height) / 2;
    
    if (g_x > 0) g_x = 0;
    if (g_y > 0) g_y = 0;
}

LRESULT CALLBACK wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT client_rect;
        GetClientRect(hWnd, &client_rect);
        int client_width = client_rect.right - client_rect.left;
        int client_height = client_rect.bottom - client_rect.top;
        
        int img_width = g_width * g_zoom;
        int img_height = g_height * g_zoom;
        
        HBRUSH bg_brush = CreateSolidBrush(RGB(0x51, 0x51, 0x51));
        
        if (g_y > 0) {
            RECT top_rect = {0, 0, client_rect.right, g_y};
            FillRect(hdc, &top_rect, bg_brush);
        }

        if (g_y + img_height < client_rect.bottom) {
            RECT bottom_rect = {0, g_y + img_height, client_rect.right, client_rect.bottom};
            FillRect(hdc, &bottom_rect, bg_brush);
        }

        if (g_x > 0) {
            RECT left_rect = {0, g_y, g_x, g_y + img_height};
            FillRect(hdc, &left_rect, bg_brush);
        }

        if (g_x + img_width < client_rect.right) {
            RECT right_rect = {g_x + img_width, g_y, client_rect.right, g_y + img_height};
            FillRect(hdc, &right_rect, bg_brush);
        }
    
        DeleteObject(bg_brush);

        if (g_screen != nullptr) {
            HDC hdc_mem = CreateCompatibleDC(hdc);
            HGDIOBJ h_old_bmp = SelectObject(hdc_mem, g_screen);
            
            StretchBlt(hdc, g_x, g_y, img_width, img_height,
                       hdc_mem, 0, 0, g_width, g_height, SRCCOPY);
            
            SelectObject(hdc_mem, h_old_bmp);
            DeleteDC(hdc_mem);
        } else {
            TextOutA(hdc, 10, 10, "No screenshot available", 22);
        }
        
        EndPaint(hWnd, &ps);
        break;
    }
    
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == 'Q') {
            PostQuitMessage(0);
        } else if (wParam == 'R') {
            center_image(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    
    case WM_LBUTTONDOWN: {
        g_is_dragging = true;
        SetCapture(hWnd);

        g_mx = LOWORD(lParam);
        g_my = HIWORD(lParam);

        break;    
    }

    case WM_LBUTTONUP:
        ReleaseCapture();

    case WM_CAPTURECHANGED: {
        g_is_dragging = false;
        break;
    }

    case WM_MOUSEMOVE:
        if (g_is_dragging) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
        
            int dx = g_mx - x;
            int dy = g_my - y;
        
            g_x -= dx;
            g_y -= dy;
        
            g_mx = x;
            g_my = y;
        
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
        
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        double old_zoom = g_zoom;
        
        if (delta > 0) g_zoom *= 1.1;
        else g_zoom *= 0.9;

        if (g_zoom < 0.01) g_zoom = 0.01;
        if (g_zoom > 100.0) g_zoom = 100.0;

        POINT mouse_pos;
        mouse_pos.x = LOWORD(lParam);
        mouse_pos.y = HIWORD(lParam);
        ScreenToClient(hWnd, &mouse_pos);

        int old_width = (int)(g_width * old_zoom);
        int old_height = (int)(g_height * old_zoom);
        int new_width = (int)(g_width * g_zoom);
        int new_height = (int)(g_height * g_zoom);
        
        double ratio_x = (double)(mouse_pos.x - g_x) / old_width;
        double ratio_y = (double)(mouse_pos.y - g_y) / old_height;

        g_x = mouse_pos.x - (int)(new_width * ratio_x);
        g_y = mouse_pos.y - (int)(new_height * ratio_y);

        RECT client_rect;
        GetClientRect(hWnd, &client_rect);

        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }
    
    case WM_ERASEBKGND:
        return 1;
    
    case WM_HOTKEY: {
        if (wParam == 1) {
            if (IsWindowVisible(hWnd)) {
                ShowWindow(hWnd, SW_HIDE);
            } else {
                UpdateScreenshot();
                g_zoom = 1.0;
                center_image(hWnd);
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
                InvalidateRect(hWnd, NULL, TRUE);
                UpdateWindow(hWnd);
            }
        }
        break;
    }
    
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR argv,
    int argc) {
    UpdateScreenshot();
    
    // ---- CREATING WINDOW ----
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = h_instance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 3);
    wc.lpszClassName = TEXT("MappClass");

    if (!RegisterClass(&wc)) {
        MessageBox(nullptr, "Window class initialization failed", "Error", MB_ICONERROR);
        DeleteObject(g_screen);
        return 1;
    }
    
    HWND w = CreateWindowW(L"MappClass", L"",
        WS_POPUP, 0, 0, g_width, g_height,
        NULL, NULL, NULL, NULL);
    
    if (!w) {
        MessageBox(nullptr, "Window creation failed", "Error", MB_ICONERROR);
        DeleteObject(g_screen);
        return 2;
    }
    
    if (!RegisterHotKey(w, 1, MOD_CONTROL | MOD_SHIFT, VK_F10)) {
        MessageBox(nullptr, "Failed to register hotkey (Ctrl+Shift+F10)", "Warning", MB_ICONWARNING);
    }
    
    ShowWindow(w, SW_HIDE);
    
    MSG msg = {0};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    UnregisterHotKey(w, 1);
    
    // ---- DEINIT  ----
    DeleteObject(g_screen);
    
    return 0;
}
