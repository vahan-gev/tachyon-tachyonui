/* TachyonUI native shim — Linux (X11 + cairo).
 *
 * Same FFI surface as tui_macos.m. Requires libX11 and libcairo
 * (Debian/Ubuntu: apt install libx11-dev libcairo2-dev).
 * Link flags: -lX11 -lcairo   (set in lib/tachyonui/Tachyon.toml)
 *
 * NOTE: written for the v0.2 bootstrap; exercised primarily on macOS —
 * report issues on Linux.
 */
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

extern void tuiRender(int32_t w, int32_t h);
extern void tuiMouseMove(double x, double y);
extern void tuiMouseDown(double x, double y);
extern void tuiMouseUp(double x, double y);
extern void tuiResize(int32_t w, int32_t h);
extern void tuiKeyDown(int32_t keycode);
extern void tuiTextInput(int32_t codepoint);
extern void tuiWheel(double dx, double dy);

static Display* g_dpy = 0;
static Window g_win = 0;
static int g_w = 0, g_h = 0;
static int g_running = 0;
static int g_dirty = 1;
static Atom g_wmDelete = 0;
static cairo_surface_t* g_surface = 0;
static cairo_t* g_cr = 0;

typedef struct { char* path; cairo_surface_t* img; } TuiImg;
static TuiImg g_images[256];
static int g_nimages = 0;

void tui_create_window(const char* title, int32_t w, int32_t h) {
    g_dpy = XOpenDisplay(0);
    if (!g_dpy) { fprintf(stderr, "tachyonui: cannot open X display\n"); exit(1); }
    int scr = DefaultScreen(g_dpy);
    g_w = w; g_h = h;
    g_win = XCreateSimpleWindow(g_dpy, RootWindow(g_dpy, scr), 0, 0, (unsigned)w, (unsigned)h,
                                0, 0, WhitePixel(g_dpy, scr));
    XStoreName(g_dpy, g_win, title ? title : "Tachyon");
    XSelectInput(g_dpy, g_win, ExposureMask | ButtonPressMask | ButtonReleaseMask |
                               PointerMotionMask | KeyPressMask | StructureNotifyMask |
                               LeaveWindowMask);
    g_wmDelete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &g_wmDelete, 1);
    g_surface = cairo_xlib_surface_create(g_dpy, g_win, DefaultVisual(g_dpy, scr), w, h);
}

static int64_t tui_now_ms(void) {
    struct timeval tv; gettimeofday(&tv, 0);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void tui_quit(void) { g_running = 0; }
int32_t tui_width(void)  { return g_w; }
int32_t tui_height(void) { return g_h; }
void tui_redraw(void) { g_dirty = 1; }
void tui_request_frame(void) { g_dirty = 1; }   /* the loop repaints next tick */

/* transforms — cairo carries a matrix stack of its own */
void tui_save(void)    { if (g_cr) cairo_save(g_cr); }
void tui_restore(void) { if (g_cr) cairo_restore(g_cr); }
void tui_translate(double dx, double dy) { if (g_cr) cairo_translate(g_cr, dx, dy); }
void tui_scale(double sx, double sy)     { if (g_cr) cairo_scale(g_cr, sx, sy); }
void tui_rotate(double deg)              { if (g_cr) cairo_rotate(g_cr, deg * M_PI / 180.0); }
void tui_clip(double x, double y, double w, double h) {
    if (g_cr) { cairo_rectangle(g_cr, x, y, w, h); cairo_clip(g_cr); }
}

void tui_run(void) {
    XMapWindow(g_dpy, g_win);
    XFlush(g_dpy);
    g_running = 1;
    const char* aq = getenv("TUI_AUTOQUIT_MS");
    int64_t deadline = aq ? tui_now_ms() + atoll(aq) : 0;

    while (g_running) {
        while (XPending(g_dpy)) {
            XEvent e;
            XNextEvent(g_dpy, &e);
            switch (e.type) {
                case Expose: g_dirty = 1; break;
                case ConfigureNotify:
                    if (e.xconfigure.width != g_w || e.xconfigure.height != g_h) {
                        g_w = e.xconfigure.width; g_h = e.xconfigure.height;
                        cairo_xlib_surface_set_size(g_surface, g_w, g_h);
                        tuiResize(g_w, g_h);
                        g_dirty = 1;
                    }
                    break;
                case MotionNotify: tuiMouseMove(e.xmotion.x, e.xmotion.y); break;
                case LeaveNotify:  tuiMouseMove(-1.0, -1.0); break;
                case ButtonPress:
                    if (e.xbutton.button == Button1) tuiMouseDown(e.xbutton.x, e.xbutton.y);
                    else if (e.xbutton.button == 4) { tuiMouseMove(e.xbutton.x, e.xbutton.y); tuiWheel(0.0, -40.0); }
                    else if (e.xbutton.button == 5) { tuiMouseMove(e.xbutton.x, e.xbutton.y); tuiWheel(0.0, 40.0); }
                    break;
                case ButtonRelease:
                    if (e.xbutton.button == Button1) tuiMouseUp(e.xbutton.x, e.xbutton.y);
                    break;
                case KeyPress: {
                    char kb[16]; KeySym ks = 0;
                    int kn = XLookupString(&e.xkey, kb, sizeof kb, &ks, 0);
                    switch (ks) {
                        case XK_BackSpace: tuiKeyDown(8); break;
                        case XK_Tab:       tuiKeyDown(9); break;
                        case XK_Return:
                        case XK_KP_Enter:  tuiKeyDown(13); break;
                        case XK_Escape:    tuiKeyDown(27); break;
                        case XK_Left:      tuiKeyDown(37); break;
                        case XK_Up:        tuiKeyDown(38); break;
                        case XK_Right:     tuiKeyDown(39); break;
                        case XK_Down:      tuiKeyDown(40); break;
                        default:
                            for (int ki = 0; ki < kn; ki++)
                                if ((unsigned char)kb[ki] >= 0x20)
                                    tuiTextInput((int32_t)(unsigned char)kb[ki]);
                            break;
                    }
                    break;
                }
                case ClientMessage:
                    if ((Atom)e.xclient.data.l[0] == g_wmDelete) g_running = 0;
                    break;
            }
        }
        if (g_dirty) {
            g_dirty = 0;
            g_cr = cairo_create(g_surface);
            tuiRender(g_w, g_h);
            cairo_destroy(g_cr);
            g_cr = 0;
            XFlush(g_dpy);
        }
        if (deadline && tui_now_ms() >= deadline) g_running = 0;
        usleep(4000);
    }
    XCloseDisplay(g_dpy);
}

/* ---------- drawing ---------- */

static void tui_rounded(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0) { cairo_rectangle(cr, x, y, w, h); return; }
    double m = w < h ? w : h;
    if (r > m / 2) r = m / 2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

void tui_clear(double r, double g, double b) {
    cairo_set_source_rgb(g_cr, r, g, b);
    cairo_paint(g_cr);
}

void tui_rect(double x, double y, double w, double h, double radius,
              double r, double g, double b, double a) {
    if (a <= 0) return;
    tui_rounded(g_cr, x, y, w, h, radius);
    cairo_set_source_rgba(g_cr, r, g, b, a);
    cairo_fill(g_cr);
}

void tui_border(double x, double y, double w, double h, double radius, double bw,
                double r, double g, double b, double a) {
    if (a <= 0 || bw <= 0) return;
    tui_rounded(g_cr, x + bw / 2, y + bw / 2, w - bw, h - bw, radius);
    cairo_set_source_rgba(g_cr, r, g, b, a);
    cairo_set_line_width(g_cr, bw);
    cairo_stroke(g_cr);
}

static void tui_font(cairo_t* cr, double size, int32_t bold) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

void tui_text(const char* s, double x, double y, double size, int32_t bold,
              double r, double g, double b, double a) {
    if (!s) return;
    tui_font(g_cr, size, bold);
    cairo_font_extents_t fe;
    cairo_font_extents(g_cr, &fe);
    cairo_set_source_rgba(g_cr, r, g, b, a);
    cairo_move_to(g_cr, x, y + fe.ascent);      /* y is the top of the line box */
    cairo_show_text(g_cr, s);
}

double tui_text_width(const char* s, double size, int32_t bold) {
    if (!s) return 0;
    cairo_t* cr = g_cr;
    cairo_surface_t* tmp = 0;
    if (!cr) {           /* measuring outside a render pass */
        tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
        cr = cairo_create(tmp);
    }
    tui_font(cr, size, bold);
    cairo_text_extents_t te;
    cairo_text_extents(cr, s, &te);
    double w = te.x_advance;
    if (tmp) { cairo_destroy(cr); cairo_surface_destroy(tmp); }
    return w;
}

double tui_text_height(double size) {
    return ceil(size * 1.25);
}

static cairo_surface_t* tui_load(const char* path) {
    if (!path) return 0;
    for (int i = 0; i < g_nimages; i++)
        if (strcmp(g_images[i].path, path) == 0) return g_images[i].img;
    cairo_surface_t* img = cairo_image_surface_create_from_png(path);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) return 0;
    if (g_nimages < 256) {
        g_images[g_nimages].path = strdup(path);
        g_images[g_nimages].img = img;
        g_nimages++;
    }
    return img;
}

void tui_image(const char* path, double x, double y, double w, double h) {
    cairo_surface_t* img = tui_load(path);
    if (!img) return;
    double iw = cairo_image_surface_get_width(img);
    double ih = cairo_image_surface_get_height(img);
    if (iw <= 0 || ih <= 0) return;
    cairo_save(g_cr);
    cairo_translate(g_cr, x, y);
    cairo_scale(g_cr, w / iw, h / ih);
    cairo_set_source_surface(g_cr, img, 0, 0);
    cairo_paint(g_cr);
    cairo_restore(g_cr);
}

double tui_image_width(const char* path) {
    cairo_surface_t* img = tui_load(path);
    return img ? cairo_image_surface_get_width(img) : 0;
}
double tui_image_height(const char* path) {
    cairo_surface_t* img = tui_load(path);
    return img ? cairo_image_surface_get_height(img) : 0;
}

void tui_set_cursor(int32_t kind) {
    unsigned shape = kind == 1 ? XC_hand2 : kind == 2 ? XC_xterm : XC_left_ptr;
    Cursor c = XCreateFontCursor(g_dpy, shape);
    XDefineCursor(g_dpy, g_win, c);
}

#endif /* __linux__ */
