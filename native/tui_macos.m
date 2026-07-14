/* TachyonUI native shim — macOS (Cocoa).
 *
 * Exposes a flat, FFI-safe C surface (numbers + C strings) that the
 * Tachyon-side library drives, and forwards window events to functions
 * the Tachyon program @exports:
 *
 *   void tuiRender(int32_t w, int32_t h);
 *   void tuiMouseMove(double x, double y);
 *   void tuiMouseDown(double x, double y);
 *   void tuiMouseUp(double x, double y);
 *   void tuiResize(int32_t w, int32_t h);
 *   void tuiKeyDown(int32_t keycode);
 *
 * Coordinates are top-left-origin points (the view is flipped).
 * Set TUI_AUTOQUIT_MS in the environment to auto-close (used by tests).
 */
#import <Cocoa/Cocoa.h>
#include <stdint.h>
#include <stdlib.h>

extern void tuiRender(int32_t w, int32_t h);
extern void tuiMouseMove(double x, double y);
extern void tuiMouseDown(double x, double y);
extern void tuiMouseUp(double x, double y);
extern void tuiResize(int32_t w, int32_t h);
extern void tuiKeyDown(int32_t keycode);
extern void tuiTextInput(int32_t codepoint);   // a typed printable character
extern void tuiWheel(double dx, double dy);    // scroll wheel / trackpad

static NSWindow* g_window = nil;
static NSView* g_view = nil;
static NSMutableDictionary<NSString*, NSImage*>* g_images = nil;
static BOOL g_frame_pending = NO;

/* normalized key codes shared across platforms (JS-like) */
enum { TUI_KEY_BACKSPACE = 8, TUI_KEY_TAB = 9, TUI_KEY_ENTER = 13,
       TUI_KEY_ESC = 27, TUI_KEY_LEFT = 37, TUI_KEY_UP = 38,
       TUI_KEY_RIGHT = 39, TUI_KEY_DOWN = 40 };

/* ---------- view ---------- */

@interface TUIView : NSView
@end

@implementation TUIView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirty {
    NSRect b = [self bounds];
    tuiRender((int32_t)b.size.width, (int32_t)b.size.height);
}
- (void)updateTrackingAreas {
    for (NSTrackingArea* a in [self.trackingAreas copy]) [self removeTrackingArea:a];
    NSTrackingArea* area = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
                      NSTrackingActiveInKeyWindow)
               owner:self userInfo:nil];
    [self addTrackingArea:area];
    [super updateTrackingAreas];
}
- (void)mouseMoved:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    tuiMouseMove(p.x, p.y);
}
- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)mouseExited:(NSEvent*)e { tuiMouseMove(-1.0, -1.0); }
- (void)mouseDown:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    tuiMouseDown(p.x, p.y);
}
- (void)mouseUp:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    tuiMouseUp(p.x, p.y);
}
- (void)scrollWheel:(NSEvent*)e {
    NSPoint p = [self convertPoint:e.locationInWindow fromView:nil];
    tuiMouseMove(p.x, p.y);                 /* keep the hovered node current */
    double m = e.hasPreciseScrollingDeltas ? 1.0 : 16.0;
    tuiWheel(-e.scrollingDeltaX * m, -e.scrollingDeltaY * m);
}
- (void)keyDown:(NSEvent*)e {
    NSString* chars = e.characters;
    if (chars.length == 0) { tuiKeyDown((int32_t)e.keyCode); return; }
    for (NSUInteger i = 0; i < chars.length; i++) {
        unichar ch = [chars characterAtIndex:i];
        switch (ch) {
            case 0x7F: case 0x08: tuiKeyDown(TUI_KEY_BACKSPACE); break;
            case 0x0D: case 0x03: tuiKeyDown(TUI_KEY_ENTER); break;
            case 0x1B:            tuiKeyDown(TUI_KEY_ESC); break;
            case 0x09:            tuiKeyDown(TUI_KEY_TAB); break;
            case NSLeftArrowFunctionKey:  tuiKeyDown(TUI_KEY_LEFT); break;
            case NSUpArrowFunctionKey:    tuiKeyDown(TUI_KEY_UP); break;
            case NSRightArrowFunctionKey: tuiKeyDown(TUI_KEY_RIGHT); break;
            case NSDownArrowFunctionKey:  tuiKeyDown(TUI_KEY_DOWN); break;
            default:
                /* printable, non-function characters become text input */
                if (ch >= 0x20 && !(ch >= 0xF700 && ch <= 0xF8FF))
                    tuiTextInput((int32_t)ch);
                break;
        }
    }
}
- (void)setFrameSize:(NSSize)sz {
    [super setFrameSize:sz];
    tuiResize((int32_t)sz.width, (int32_t)sz.height);
}
@end

/* ---------- app delegate ---------- */

@interface TUIDelegate : NSObject <NSApplicationDelegate>
@end
@implementation TUIDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app { return YES; }
@end

/* ---------- lifecycle ---------- */

void tui_create_window(const char* title, int32_t w, int32_t h) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        static TUIDelegate* delegate = nil;
        delegate = [TUIDelegate new];
        [NSApp setDelegate:delegate];

        NSRect frame = NSMakeRect(0, 0, w, h);
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        g_window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        [g_window setTitle:[NSString stringWithUTF8String:title ? title : "Tachyon"]];
        [g_window center];
        [g_window setReleasedWhenClosed:NO];

        g_view = [[TUIView alloc] initWithFrame:frame];
        [g_window setContentView:g_view];
        [g_window makeFirstResponder:g_view];
        g_images = [NSMutableDictionary new];
    }
}

void tui_run(void) {
    @autoreleasepool {
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        const char* aq = getenv("TUI_AUTOQUIT_MS");
        if (aq) {
            double s = atof(aq) / 1000.0;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(s * NSEC_PER_SEC)),
                           dispatch_get_main_queue(), ^{ [NSApp terminate:nil]; });
        }
        [NSApp run];
    }
}

void tui_quit(void) { [NSApp terminate:nil]; }

int32_t tui_width(void)  { return g_view ? (int32_t)g_view.bounds.size.width : 0; }
int32_t tui_height(void) { return g_view ? (int32_t)g_view.bounds.size.height : 0; }

void tui_redraw(void) { [g_view setNeedsDisplay:YES]; }

/* schedule exactly one more frame ~1/60s out; coalesced so a whole animating
   tree costs one timer, not one per widget. */
void tui_request_frame(void) {
    if (g_frame_pending || !g_view) return;
    g_frame_pending = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(NSEC_PER_SEC / 60)),
                   dispatch_get_main_queue(), ^{
        g_frame_pending = NO;
        [g_view setNeedsDisplay:YES];
    });
}

/* ---------- transforms (valid inside tuiRender) ---------- */

void tui_save(void)    { [NSGraphicsContext saveGraphicsState]; }
void tui_restore(void) { [NSGraphicsContext restoreGraphicsState]; }
void tui_translate(double dx, double dy) {
    NSAffineTransform* t = [NSAffineTransform transform];
    [t translateXBy:dx yBy:dy]; [t concat];
}
void tui_scale(double sx, double sy) {
    NSAffineTransform* t = [NSAffineTransform transform];
    [t scaleXBy:sx yBy:sy]; [t concat];
}
void tui_rotate(double deg) {
    NSAffineTransform* t = [NSAffineTransform transform];
    [t rotateByDegrees:deg]; [t concat];
}
void tui_clip(double x, double y, double w, double h) {
    NSRectClip(NSMakeRect(x, y, w, h));
}

/* ---------- drawing (valid inside tuiRender) ---------- */

void tui_clear(double r, double g, double b) {
    [[NSColor colorWithSRGBRed:r green:g blue:b alpha:1.0] setFill];
    NSRectFill(g_view.bounds);
}

void tui_rect(double x, double y, double w, double h, double radius,
              double r, double g, double b, double a) {
    if (a <= 0.0) return;
    NSBezierPath* p = radius > 0.0
        ? [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x, y, w, h)
                                          xRadius:radius yRadius:radius]
        : [NSBezierPath bezierPathWithRect:NSMakeRect(x, y, w, h)];
    [[NSColor colorWithSRGBRed:r green:g blue:b alpha:a] setFill];
    [p fill];
}

void tui_border(double x, double y, double w, double h, double radius, double bw,
                double r, double g, double b, double a) {
    if (a <= 0.0 || bw <= 0.0) return;
    NSRect rc = NSMakeRect(x + bw / 2, y + bw / 2, w - bw, h - bw);
    NSBezierPath* p = radius > 0.0
        ? [NSBezierPath bezierPathWithRoundedRect:rc xRadius:radius yRadius:radius]
        : [NSBezierPath bezierPathWithRect:rc];
    [p setLineWidth:bw];
    [[NSColor colorWithSRGBRed:r green:g blue:b alpha:a] setStroke];
    [p stroke];
}

static NSDictionary* tui_attrs(double size, int32_t bold,
                               double r, double g, double b, double a) {
    NSFont* font = bold ? [NSFont boldSystemFontOfSize:size]
                        : [NSFont systemFontOfSize:size];
    NSColor* color = [NSColor colorWithSRGBRed:r green:g blue:b alpha:a];
    return @{ NSFontAttributeName: font, NSForegroundColorAttributeName: color };
}

void tui_text(const char* s, double x, double y, double size, int32_t bold,
              double r, double g, double b, double a) {
    if (!s) return;
    NSString* str = [NSString stringWithUTF8String:s];
    [str drawAtPoint:NSMakePoint(x, y) withAttributes:tui_attrs(size, bold, r, g, b, a)];
}

double tui_text_width(const char* s, double size, int32_t bold) {
    if (!s) return 0;
    NSString* str = [NSString stringWithUTF8String:s];
    NSSize sz = [str sizeWithAttributes:tui_attrs(size, bold, 0, 0, 0, 1)];
    return sz.width;
}

double tui_text_height(double size) {
    NSFont* font = [NSFont systemFontOfSize:size];
    return ceil(font.ascender - font.descender + font.leading);
}

static NSImage* tui_load(const char* path) {
    if (!path) return nil;
    NSString* key = [NSString stringWithUTF8String:path];
    NSImage* img = g_images[key];
    if (!img) {
        img = [[NSImage alloc] initWithContentsOfFile:key];
        if (img) g_images[key] = img;
    }
    return img;
}

void tui_image(const char* path, double x, double y, double w, double h) {
    NSImage* img = tui_load(path);
    if (!img) return;
    [img drawInRect:NSMakeRect(x, y, w, h)
           fromRect:NSZeroRect
          operation:NSCompositingOperationSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];
}

double tui_image_width(const char* path) {
    NSImage* img = tui_load(path);
    return img ? img.size.width : 0;
}
double tui_image_height(const char* path) {
    NSImage* img = tui_load(path);
    return img ? img.size.height : 0;
}

void tui_set_cursor(int32_t kind) {
    switch (kind) {
        case 1: [[NSCursor pointingHandCursor] set]; break;
        case 2: [[NSCursor IBeamCursor] set]; break;
        default: [[NSCursor arrowCursor] set]; break;
    }
}
