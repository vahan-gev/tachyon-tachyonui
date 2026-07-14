/* TachyonUI native shim — Windows (Win32 + GDI+ flat API).
 *
 * Same FFI surface as tui_macos.m. Link flags: -lgdiplus -lgdi32 -luser32
 * (set in lib/tachyonui/Tachyon.toml; build with MinGW-w64 or clang).
 *
 * NOTE: written for the v0.2 bootstrap; exercised primarily on macOS —
 * report issues on Windows.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---- minimal GDI+ flat-API declarations (avoids the C++ headers) ---- */
typedef void GpGraphics;
typedef void GpBrush;
typedef void GpPen;
typedef void GpPath;
typedef void GpImage;
typedef void GpFont;
typedef void GpFontFamily;
typedef void GpStringFormat;
typedef int GpStatus;
typedef struct { float X, Y, Width, Height; } RectF;
typedef struct { UINT32 GdiplusVersion; void* DebugEventCallback;
                 BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdiplusStartupInput;

GpStatus WINAPI GdiplusStartup(ULONG_PTR* token, const GdiplusStartupInput* in, void* out);
GpStatus WINAPI GdipCreateFromHDC(HDC hdc, GpGraphics** g);
GpStatus WINAPI GdipDeleteGraphics(GpGraphics* g);
GpStatus WINAPI GdipSetSmoothingMode(GpGraphics* g, int mode);
GpStatus WINAPI GdipSetTextRenderingHint(GpGraphics* g, int hint);
GpStatus WINAPI GdipGraphicsClear(GpGraphics* g, UINT32 argb);
GpStatus WINAPI GdipCreateSolidFill(UINT32 argb, GpBrush** brush);
GpStatus WINAPI GdipDeleteBrush(GpBrush* brush);
GpStatus WINAPI GdipCreatePen1(UINT32 argb, float width, int unit, GpPen** pen);
GpStatus WINAPI GdipDeletePen(GpPen* pen);
GpStatus WINAPI GdipFillRectangle(GpGraphics* g, GpBrush* b, float x, float y, float w, float h);
GpStatus WINAPI GdipCreatePath(int fillMode, GpPath** path);
GpStatus WINAPI GdipDeletePath(GpPath* path);
GpStatus WINAPI GdipAddPathArc(GpPath* p, float x, float y, float w, float h, float a0, float sweep);
GpStatus WINAPI GdipClosePathFigure(GpPath* p);
GpStatus WINAPI GdipFillPath(GpGraphics* g, GpBrush* b, GpPath* p);
GpStatus WINAPI GdipDrawPath(GpGraphics* g, GpPen* pen, GpPath* p);
GpStatus WINAPI GdipCreateFontFamilyFromName(const WCHAR* name, void* coll, GpFontFamily** fam);
GpStatus WINAPI GdipDeleteFontFamily(GpFontFamily* fam);
GpStatus WINAPI GdipCreateFont(const GpFontFamily* fam, float size, int style, int unit, GpFont** font);
GpStatus WINAPI GdipDeleteFont(GpFont* font);
GpStatus WINAPI GdipDrawString(GpGraphics* g, const WCHAR* s, int len, const GpFont* f,
                               const RectF* rect, const GpStringFormat* fmt, const GpBrush* b);
GpStatus WINAPI GdipMeasureString(GpGraphics* g, const WCHAR* s, int len, const GpFont* f,
                                  const RectF* layout, const GpStringFormat* fmt,
                                  RectF* bound, int* cps, int* lines);
GpStatus WINAPI GdipLoadImageFromFile(const WCHAR* path, GpImage** img);
GpStatus WINAPI GdipGetImageWidth(GpImage* img, UINT* w);
GpStatus WINAPI GdipGetImageHeight(GpImage* img, UINT* h);
GpStatus WINAPI GdipDrawImageRect(GpGraphics* g, GpImage* img, float x, float y, float w, float h);
/* world-transform stack (for §animation transforms) */
typedef UINT GraphicsState;
GpStatus WINAPI GdipSaveGraphics(GpGraphics* g, GraphicsState* state);
GpStatus WINAPI GdipRestoreGraphics(GpGraphics* g, GraphicsState state);
GpStatus WINAPI GdipTranslateWorldTransform(GpGraphics* g, float dx, float dy, int order);
GpStatus WINAPI GdipScaleWorldTransform(GpGraphics* g, float sx, float sy, int order);
GpStatus WINAPI GdipRotateWorldTransform(GpGraphics* g, float angle, int order);
GpStatus WINAPI GdipSetClipRect(GpGraphics* g, float x, float y, float w, float h, int mode);

extern void tuiRender(int32_t w, int32_t h);
extern void tuiMouseMove(double x, double y);
extern void tuiMouseDown(double x, double y);
extern void tuiMouseUp(double x, double y);
extern void tuiResize(int32_t w, int32_t h);
extern void tuiKeyDown(int32_t keycode);
extern void tuiTextInput(int32_t codepoint);
extern void tuiWheel(double dx, double dy);
extern void tuiCommand(int32_t action);

#define TUI_FRAME_TIMER 0x7ACF
#define TUI_AUTOQUIT_TIMER 0x7ACE

static HWND g_hwnd = 0;
static int g_w = 0, g_h = 0;
static GpGraphics* g_gfx = 0;   /* valid during WM_PAINT */
static ULONG_PTR g_gdipToken = 0;
static int g_cursorKind = 0;
static GraphicsState g_gstack[64];   /* transform save/restore stack */
static int g_gtop = 0;

typedef struct { char* path; GpImage* img; } TuiImg;
static TuiImg g_images[256];
static int g_nimages = 0;

static UINT32 tui_argb(double r, double g, double b, double a) {
    UINT32 A = (UINT32)(a * 255.0 + 0.5), R = (UINT32)(r * 255.0 + 0.5);
    UINT32 G = (UINT32)(g * 255.0 + 0.5), B = (UINT32)(b * 255.0 + 0.5);
    if (A > 255) A = 255; if (R > 255) R = 255; if (G > 255) G = 255; if (B > 255) B = 255;
    return (A << 24) | (R << 16) | (G << 8) | B;
}

static WCHAR* tui_wide(const char* s) {          /* caller frees */
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
    WCHAR* w = (WCHAR*)malloc((size_t)n * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

/* ---------- window proc ---------- */
static LRESULT CALLBACK tui_wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(h, &ps);
            /* double buffer */
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, g_w, g_h);
            HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
            GdipCreateFromHDC(mem, &g_gfx);
            GdipSetSmoothingMode(g_gfx, 4 /*AntiAlias*/);
            GdipSetTextRenderingHint(g_gfx, 4 /*ClearTypeGridFit*/);
            tuiRender(g_w, g_h);
            GdipDeleteGraphics(g_gfx);
            g_gfx = 0;
            BitBlt(hdc, 0, 0, g_w, g_h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SIZE:
            g_w = LOWORD(lp); g_h = HIWORD(lp);
            tuiResize(g_w, g_h);
            InvalidateRect(h, 0, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            tuiMouseMove((double)(short)LOWORD(lp), (double)(short)HIWORD(lp));
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(h);
            tuiMouseDown((double)(short)LOWORD(lp), (double)(short)HIWORD(lp));
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            tuiMouseUp((double)(short)LOWORD(lp), (double)(short)HIWORD(lp));
            return 0;
        case WM_KEYDOWN: {
            if (GetKeyState(VK_CONTROL) & 0x8000) {   /* Ctrl+C/V/X */
                if (wp == 'C') { tuiCommand(1); return 0; }
                if (wp == 'V') { tuiCommand(2); return 0; }
                if (wp == 'X') { tuiCommand(3); return 0; }
            }
            int32_t k = 0;
            switch (wp) {
                case VK_BACK:   k = 8;  break;
                case VK_TAB:    k = 9;  break;
                case VK_RETURN: k = 13; break;
                case VK_ESCAPE: k = 27; break;
                case VK_LEFT:   k = 37; break;
                case VK_UP:     k = 38; break;
                case VK_RIGHT:  k = 39; break;
                case VK_DOWN:   k = 40; break;
                default: break;         /* printable keys arrive as WM_CHAR */
            }
            if (k) tuiKeyDown(k);
            return 0;
        }
        case WM_CHAR:
            if (wp >= 0x20 && wp != 0x7F) tuiTextInput((int32_t)wp);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(h, &pt);
            tuiMouseMove((double)pt.x, (double)pt.y);
            int delta = GET_WHEEL_DELTA_WPARAM(wp);      /* 120 per notch */
            tuiWheel(0.0, -(double)delta / 120.0 * 40.0);
            return 0;
        }
        case WM_TIMER:
            if (wp == TUI_FRAME_TIMER) {
                KillTimer(h, TUI_FRAME_TIMER);
                InvalidateRect(h, 0, FALSE);
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                LPCSTR c = g_cursorKind == 1 ? IDC_HAND : g_cursorKind == 2 ? IDC_IBEAM : IDC_ARROW;
                SetCursor(LoadCursorA(0, c));
                return TRUE;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

/* ---------- lifecycle ---------- */
void tui_create_window(const char* title, int32_t w, int32_t h) {
    GdiplusStartupInput in = { 1, 0, FALSE, FALSE };
    GdiplusStartup(&g_gdipToken, &in, 0);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = tui_wndproc;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "TachyonUIWindow";
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&wc);

    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_w = w; g_h = h;
    g_hwnd = CreateWindowA("TachyonUIWindow", title ? title : "Tachyon",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           0, 0, wc.hInstance, 0);
}

void tui_run(void) {
    ShowWindow(g_hwnd, SW_SHOW);
    const char* aq = getenv("TUI_AUTOQUIT_MS");
    if (aq) SetTimer(g_hwnd, TUI_AUTOQUIT_TIMER, (UINT)atoi(aq), 0);
    MSG msg;
    while (GetMessageA(&msg, 0, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == TUI_AUTOQUIT_TIMER && aq) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void tui_quit(void) { PostQuitMessage(0); }
int32_t tui_width(void)  { return g_w; }
int32_t tui_height(void) { return g_h; }
void tui_redraw(void) { InvalidateRect(g_hwnd, 0, FALSE); }
void tui_request_frame(void) { SetTimer(g_hwnd, TUI_FRAME_TIMER, 16, 0); }

/* transforms — GDI+ world transform, with a manual save/restore token stack */
void tui_save(void) {
    if (g_gfx && g_gtop < 64) GdipSaveGraphics(g_gfx, &g_gstack[g_gtop++]);
}
void tui_restore(void) {
    if (g_gfx && g_gtop > 0) GdipRestoreGraphics(g_gfx, g_gstack[--g_gtop]);
}
void tui_translate(double dx, double dy) {
    if (g_gfx) GdipTranslateWorldTransform(g_gfx, (float)dx, (float)dy, 0 /*prepend*/);
}
void tui_scale(double sx, double sy) {
    if (g_gfx) GdipScaleWorldTransform(g_gfx, (float)sx, (float)sy, 0);
}
void tui_rotate(double deg) {
    if (g_gfx) GdipRotateWorldTransform(g_gfx, (float)deg, 0);
}
void tui_clip(double x, double y, double w, double h) {
    /* intersect with the current clip so nested scroll regions compose */
    if (g_gfx) GdipSetClipRect(g_gfx, (float)x, (float)y, (float)w, (float)h, 1 /*intersect*/);
}

/* ---------- clipboard ---------- */
static char g_clip[8192];
const char* tui_clipboard_get(void) {
    g_clip[0] = 0;
    if (!OpenClipboard(g_hwnd)) return g_clip;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        WCHAR* w = (WCHAR*)GlobalLock(h);
        if (w) {
            WideCharToMultiByte(CP_UTF8, 0, w, -1, g_clip, sizeof g_clip - 1, 0, 0);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return g_clip;
}
void tui_clipboard_set(const char* s) {
    if (!OpenClipboard(g_hwnd)) return;
    EmptyClipboard();
    int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, 0, 0);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (size_t)n * sizeof(WCHAR));
    if (h) {
        WCHAR* w = (WCHAR*)GlobalLock(h);
        MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, w, n);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

/* ---------- drawing ---------- */
static void tui_round_path(GpPath* p, float x, float y, float w, float h, float r) {
    float m = w < h ? w : h;
    if (r > m / 2) r = m / 2;
    float d = r * 2;
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
}

void tui_clear(double r, double g, double b) {
    GdipGraphicsClear(g_gfx, tui_argb(r, g, b, 1.0));
}

void tui_rect(double x, double y, double w, double h, double radius,
              double r, double g, double b, double a) {
    if (a <= 0) return;
    GpBrush* br = 0;
    GdipCreateSolidFill(tui_argb(r, g, b, a), &br);
    if (radius > 0) {
        GpPath* p = 0;
        GdipCreatePath(0, &p);
        tui_round_path(p, (float)x, (float)y, (float)w, (float)h, (float)radius);
        GdipFillPath(g_gfx, br, p);
        GdipDeletePath(p);
    } else {
        GdipFillRectangle(g_gfx, br, (float)x, (float)y, (float)w, (float)h);
    }
    GdipDeleteBrush(br);
}

void tui_border(double x, double y, double w, double h, double radius, double bw,
                double r, double g, double b, double a) {
    if (a <= 0 || bw <= 0) return;
    GpPen* pen = 0;
    GdipCreatePen1(tui_argb(r, g, b, a), (float)bw, 2 /*UnitPixel*/, &pen);
    GpPath* p = 0;
    GdipCreatePath(0, &p);
    tui_round_path(p, (float)(x + bw / 2), (float)(y + bw / 2),
                   (float)(w - bw), (float)(h - bw),
                   (float)(radius > 0 ? radius : 0.01));
    GdipDrawPath(g_gfx, pen, p);
    GdipDeletePath(p);
    GdipDeletePen(pen);
}

static GpFont* tui_font(double size, int32_t bold) {
    GpFontFamily* fam = 0;
    GdipCreateFontFamilyFromName(L"Segoe UI", 0, &fam);
    GpFont* font = 0;
    GdipCreateFont(fam, (float)size, bold ? 1 : 0, 2 /*UnitPixel*/, &font);
    GdipDeleteFontFamily(fam);
    return font;
}

void tui_text(const char* s, double x, double y, double size, int32_t bold,
              double r, double g, double b, double a) {
    if (!s || !g_gfx) return;
    WCHAR* ws = tui_wide(s);
    GpFont* font = tui_font(size, bold);
    GpBrush* br = 0;
    GdipCreateSolidFill(tui_argb(r, g, b, a), &br);
    RectF rc = { (float)x, (float)y, 100000.0f, 100000.0f };
    GdipDrawString(g_gfx, ws, -1, font, &rc, 0, br);
    GdipDeleteBrush(br);
    GdipDeleteFont(font);
    free(ws);
}

double tui_text_width(const char* s, double size, int32_t bold) {
    if (!s) return 0;
    HDC hdc = GetDC(g_hwnd);
    GpGraphics* gfx = 0;
    GdipCreateFromHDC(hdc, &gfx);
    WCHAR* ws = tui_wide(s);
    GpFont* font = tui_font(size, bold);
    RectF layout = { 0, 0, 100000.0f, 100000.0f }, bound = {0};
    GdipMeasureString(gfx, ws, -1, font, &layout, 0, &bound, 0, 0);
    GdipDeleteFont(font);
    free(ws);
    GdipDeleteGraphics(gfx);
    ReleaseDC(g_hwnd, hdc);
    return bound.Width;
}

double tui_text_height(double size) { return size * 1.35; }

static GpImage* tui_load(const char* path) {
    if (!path) return 0;
    for (int i = 0; i < g_nimages; i++)
        if (strcmp(g_images[i].path, path) == 0) return g_images[i].img;
    WCHAR* wp = tui_wide(path);
    GpImage* img = 0;
    GdipLoadImageFromFile(wp, &img);
    free(wp);
    if (img && g_nimages < 256) {
        g_images[g_nimages].path = _strdup(path);
        g_images[g_nimages].img = img;
        g_nimages++;
    }
    return img;
}

void tui_image(const char* path, double x, double y, double w, double h) {
    GpImage* img = tui_load(path);
    if (!img || !g_gfx) return;
    GdipDrawImageRect(g_gfx, img, (float)x, (float)y, (float)w, (float)h);
}

double tui_image_width(const char* path) {
    GpImage* img = tui_load(path);
    UINT w = 0;
    if (img) GdipGetImageWidth(img, &w);
    return (double)w;
}
double tui_image_height(const char* path) {
    GpImage* img = tui_load(path);
    UINT h = 0;
    if (img) GdipGetImageHeight(img, &h);
    return (double)h;
}

void tui_set_cursor(int32_t kind) { g_cursorKind = kind; }

#endif /* _WIN32 */
