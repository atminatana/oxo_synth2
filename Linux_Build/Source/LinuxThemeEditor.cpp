#include "LinuxThemeEditor.hpp"
#include "LinuxEditor.hpp"
#include "LinuxPrompt.hpp"
#include <cairo/cairo.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace oxo {

ThemeEditorWindow::ThemeEditorWindow(Display *display, Window parent, const Palette &current,
                                     const std::vector<NamedPalette> &factory,
                                     const std::vector<NamedPalette> &user,
                                     std::function<void(const Palette&, bool)> onChange,
                                     std::function<void()> onClose)
    : display_(display), current_(current), factory_(factory), user_(user),
      onChange_(std::move(onChange)), onClose_(std::move(onClose)) {
    CreateWindow();
}

ThemeEditorWindow::~ThemeEditorWindow() {
    if (cr_) cairo_destroy(cr_);
    if (surface_) cairo_surface_destroy(surface_);
    if (window_) XDestroyWindow(display_, window_);
}

void ThemeEditorWindow::CreateWindow() {
    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | StructureNotifyMask;
    attrs.background_pixel = 0;

    Window root = DefaultRootWindow(display_);
    window_ = XCreateWindow(display_, root, 100, 100, width_, height_, 0,
                            CopyFromParent, InputOutput, CopyFromParent,
                            CWEventMask | CWBackPixel, &attrs);

    XClassHint classHint;
    classHint.res_name = const_cast<char*>("OxoSynth2ThemeEditor");
    classHint.res_class = const_cast<char*>("OxoSynth2ThemeEditor");
    XSetClassHint(display_, window_, &classHint);

    XMapWindow(display_, window_);

    surface_ = cairo_xlib_surface_create(display_, window_,
                                         DefaultVisual(display_, DefaultScreen(display_)),
                                         width_, height_);
    cr_ = cairo_create(surface_);
}

void ThemeEditorWindow::Show() {
    XRaiseWindow(display_, window_);
    Draw();
}

void ThemeEditorWindow::HandleEvent(XEvent &event) {
    switch (event.type) {
        case Expose:
            if (event.xexpose.count == 0) Draw();
            break;
        case ButtonPress:
            HandleButtonPress(event.xbutton.x, event.xbutton.y);
            break;
        case KeyPress: {
            KeySym keysym;
            XLookupString(&event.xkey, nullptr, 0, &keysym, nullptr);
            HandleKeyPress(keysym);
            break;
        }
        case ConfigureNotify:
            if (event.xconfigure.window == window_) {
                width_ = event.xconfigure.width;
                height_ = event.xconfigure.height;
                cairo_xlib_surface_set_size(surface_, width_, height_);
                Draw();
            }
            break;
    }
}

void ThemeEditorWindow::Draw() {
    if (!cr_) return;

    // Background
    cairoSetColorRGB(cr_, kBg);
    cairo_rectangle(cr_, 0, 0, width_, height_);
    cairo_fill(cr_);

    // Title bar
    cairoSetColorRGB(cr_, kPanel);
    cairo_rectangle(cr_, 0, 0, width_, 36);
    cairo_fill(cr_);

    cairoSetColorRGB(cr_, kText);
    cairo_select_font_face(cr_, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr_, 14);
    cairo_move_to(cr_, 12, 24);
    cairo_show_text(cr_, "Themes");

    // Close button
    cairoSetColorRGB(cr_, kBorder);
    cairo_rectangle(cr_, width_ - 32, 4, 24, 24);
    cairo_stroke(cr_);
    cairo_move_to(cr_, width_ - 24, 12);
    cairo_line_to(cr_, width_ - 12, 24);
    cairo_move_to(cr_, width_ - 12, 12);
    cairo_line_to(cr_, width_ - 24, 24);
    cairo_stroke(cr_);

    // Tabs
    DrawTabs(cr_, width_, height_);

    // Content area
    int contentY = 44;
    int contentH = height_ - 44 - 50;
    int listW = 200;

    DrawPaletteList(cr_, 10, contentY, listW, contentH, activeTab_);
    DrawColorEditor(cr_, 10 + listW + 10, contentY, width_ - listW - 30, contentH);
    DrawButtons(cr_, width_, height_);

    cairo_surface_flush(surface_);
}

void ThemeEditorWindow::DrawTabs(cairo_t *cr, int w, int h) {
    const char* tabs[] = {"Factory", "User", "Editor"};
    int tabW = 80;
    for (int i = 0; i < 3; ++i) {
        int x = 10 + i * (tabW + 4);
        int y = 38;
        bool active = (static_cast<int>(activeTab_) == i);

        if (active) {
            cairoSetColorRGB(cr, kSelBg);
        } else {
            cairoSetColorRGB(cr, kPanel);
        }
        cairo_rectangle(cr, x, y, tabW, 28);
        cairo_fill(cr);

        cairoSetColorRGB(cr, active ? kAccent : kText);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        int textW = 0, textH = 0;
        cairo_text_extents_t ext;
        cairo_text_extents(cr, tabs[i], &ext);
        textW = ext.width;
        textH = ext.height;
        cairo_move_to(cr, x + (tabW - textW) * 0.5, y + (28 + textH) * 0.5);
        cairo_show_text(cr, tabs[i]);
    }
}

void ThemeEditorWindow::DrawPaletteList(cairo_t *cr, int x, int y, int w, int h, Tab tab) {
    const std::vector<NamedPalette> &source = (tab == Tab::Factory) ? factory_ : user_;
    int itemH = 28;
    int maxItems = h / itemH;

    // Panel background
    cairoSetColorRGB(cr, kPanel);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    for (size_t i = 0; i < source.size() && i < static_cast<size_t>(maxItems); ++i) {
        int iy = y + i * itemH;
        bool sel = (static_cast<int>(i) == selectedIndex_);

        if (sel) {
            cairoSetColorRGB(cr, kSelBg);
            cairo_rectangle(cr, x, iy, w, itemH - 1);
            cairo_fill(cr);
        }

        // Color preview (first color of palette)
        cairoSetColorRGB(cr, source[i].palette[0] | 0xFF000000);
        cairo_rectangle(cr, x + 4, iy + 4, 20, itemH - 8);
        cairo_fill(cr);

        // Name
        cairoSetColorRGB(cr, sel ? kAccent : kText);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11);
        cairo_move_to(cr, x + 28, iy + (itemH + 8) * 0.5);
        cairo_show_text(cr, source[i].name.c_str());
    }
}

void ThemeEditorWindow::DrawColorEditor(cairo_t *cr, int x, int y, int w, int h) {
    cairoSetColorRGB(cr, kPanel);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill(cr);

    if (editor_.editing && editor_.slot >= 0) {
        // Editing a specific color
        cairoSetColorRGB(cr, kText);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12);
        const char* slotName = themes::kTitles[editor_.slot];
        cairo_move_to(cr, x + 10, y + 20);
        cairo_show_text(cr, slotName);

        // Color picker - simplified: show current color and allow picking
        uint32_t rgb = editor_.rgb;
        cairoSetColorRGB(cr, rgb | 0xFF000000);
        cairo_rectangle(cr, x + 10, y + 40, 80, 40);
        cairo_fill(cr);

        cairoSetColorRGB(cr, kBorder);
        cairo_rectangle(cr, x + 10, y + 40, 80, 40);
        cairo_stroke(cr);

        // Hex value
        char hex[16];
        snprintf(hex, sizeof(hex), "#%06X", rgb & 0xFFFFFF);
        cairoSetColorRGB(cr, kText);
        cairo_set_font_size(cr, 11);
        cairo_move_to(cr, x + 100, y + 68);
        cairo_show_text(cr, hex);

        // Instructions
        cairoSetColorRGB(cr, 0x888888);
        cairo_set_font_size(cr, 10);
        cairo_move_to(cr, x + 10, y + 100);
        cairo_show_text(cr, "Press Enter to pick color, Esc to cancel");
    } else {
        // Show all 13 colors as editable swatches
        int swatchSize = 30;
        int cols = 4;
        for (int i = 0; i < themes::kSlotCount; ++i) {
            int col = i % cols;
            int row = i / cols;
            int sx = x + 10 + col * (swatchSize + 10);
            int sy = y + 10 + row * (swatchSize + 30);

            // Swatch
            cairoSetColorRGB(cr, current_[i] | 0xFF000000);
            cairo_rectangle(cr, sx, sy, swatchSize, swatchSize);
            cairo_fill(cr);
            cairoSetColorRGB(cr, kBorder);
            cairo_rectangle(cr, sx, sy, swatchSize, swatchSize);
            cairo_stroke(cr);

            // Label
            cairoSetColorRGB(cr, kText);
            cairo_set_font_size(cr, 9);
            cairo_move_to(cr, sx, sy + swatchSize + 14);
            cairo_show_text(cr, themes::kTitles[i]);
        }
    }
}

void ThemeEditorWindow::DrawButtons(cairo_t *cr, int w, int h) {
    int btnY = h - 40;
    int btnW = 80, btnH = 30;
    int startX = (w - 3 * (btnW + 10)) / 2;

    const char* labels[] = {"Save", "Delete", "Close"};
    for (int i = 0; i < 3; ++i) {
        int x = startX + i * (btnW + 10);
        cairoSetColorRGB(cr, i == 2 ? kBorder : kAccent);
        cairo_rectangle(cr, x, btnY, btnW, btnH);
        cairo_fill(cr);
        cairoSetColorRGB(cr, kBg);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, labels[i], &ext);
        cairo_move_to(cr, x + (btnW - ext.width) * 0.5, btnY + (btnH + ext.height) * 0.5);
        cairo_show_text(cr, labels[i]);
    }
}

void ThemeEditorWindow::HandleButtonPress(int x, int y) {
    // Tab clicks
    if (y >= 38 && y <= 66) {
        int tabIdx = (x - 10) / 84;
        if (tabIdx >= 0 && tabIdx < 3) {
            activeTab_ = static_cast<Tab>(tabIdx);
            selectedIndex_ = -1;
            Draw();
            return;
        }
    }

    // Palette list clicks
    int listX = 10, listY = 44, listW = 200, listH = height_ - 44 - 50;
    if (x >= listX && x < listX + listW && y >= listY && y < listY + listH) {
        int itemH = 28;
        int idx = (y - listY) / itemH;
        const std::vector<NamedPalette> &source = (activeTab_ == Tab::Factory) ? factory_ : user_;
        if (idx >= 0 && idx < static_cast<int>(source.size())) {
            selectedIndex_ = idx;
            editor_.editing = false;
            Draw();
            return;
        }
    }

    // Color swatch clicks (in editor tab)
    if (activeTab_ == Tab::Editor && !editor_.editing) {
        int swatchSize = 30;
        int cols = 4;
        for (int i = 0; i < themes::kSlotCount; ++i) {
            int col = i % cols;
            int row = i / cols;
            int sx = 10 + listW + 10 + 10 + col * (swatchSize + 10);
            int sy = 44 + 10 + row * (swatchSize + 30);
            if (x >= sx && x < sx + swatchSize && y >= sy && y < sy + swatchSize) {
                editor_.slot = i;
                editor_.rgb = current_[i];
                editor_.editing = true;
                Draw();
                return;
            }
        }
    }

    // Button clicks
    int btnY = height_ - 40;
    int btnW = 80, btnH = 30;
    int startX = (width_ - 3 * (btnW + 10)) / 2;
    if (y >= btnY && y < btnY + btnH) {
        for (int i = 0; i < 3; ++i) {
            int x = startX + i * (btnW + 10);
            if (x >= startX && x < startX + btnW) {
                if (i == 0) SaveTheme();
                else if (i == 1) DeleteTheme();
                else if (i == 2) { onClose_(); return; }
                break;
            }
        }
    }
}

void ThemeEditorWindow::HandleKeyPress(KeySym key) {
    if (editor_.editing) {
        if (key == XK_Return) {
            // Pick color - simplified: use zenity color picker or cycle
            uint32_t newColor = PickColor(editor_.rgb);
            if (newColor != editor_.rgb) {
                editor_.rgb = newColor;
                current_[editor_.slot] = newColor;
                onChange_(current_, false);
            }
            editor_.editing = false;
            Draw();
        } else if (key == XK_Escape) {
            editor_.editing = false;
            Draw();
        }
        return;
    }

    if (key == XK_Escape) {
        onClose_();
    }
}

void ThemeEditorWindow::SaveTheme() {
    if (selectedIndex_ < 0) return;
    // In a real implementation, this would save to themes.json
    // For now, just notify
    onChange_(current_, true);
}

void ThemeEditorWindow::DeleteTheme() {
    if (selectedIndex_ < 0 || activeTab_ == Tab::Factory) return;
    // Delete user theme
    onChange_(current_, true);
}

uint32_t ThemeEditorWindow::PickColor(uint32_t current) {
    // Simplified: cycle through some colors or use zenity
    // In practice, would use a proper color picker dialog
    static const uint32_t colors[] = {
        0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF,
        0xFFFFFF, 0x808080, 0xFF8000, 0x80FF00, 0x0080FF, 0xFF0080
    };
    for (size_t i = 0; i < sizeof(colors)/sizeof(colors[0]); ++i) {
        if (colors[i] == (current & 0xFFFFFF)) {
            return colors[(i + 1) % (sizeof(colors)/sizeof(colors[0]))] | 0xFF000000;
        }
    }
    return colors[0] | 0xFF000000;
}

} // namespace oxo