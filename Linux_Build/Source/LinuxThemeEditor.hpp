#pragma once

// Theme editor window for Linux (X11 + Cairo).
// Mirrors the Windows ThemeEditorWindow and macOS ThemeDialog.

#include "LinuxEditor.hpp"
#include "EditorCore/Themes.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace oxo {

class ThemeEditorWindow {
public:
    using Palette = themes::Palette;
    using NamedPalette = themes::Named;

    ThemeEditorWindow(Display *display, Window parent, const Palette &current,
                      const std::vector<NamedPalette> &factory,
                      const std::vector<NamedPalette> &user,
                      std::function<void(const Palette&, bool)> onChange,
                      std::function<void()> onClose);
    ~ThemeEditorWindow();

    void Show();
    void HandleEvent(XEvent &event);

private:
    enum class Tab { Factory, User, Editor };
    struct ColorEditor {
        int slot = -1;
        uint32_t rgb = 0;
        bool editing = false;
    };

    void CreateWindow();
    void Draw();
    void DrawTabs(cairo_t *cr, int width, int height);
    void DrawPaletteList(cairo_t *cr, int x, int y, int w, int h, Tab tab);
    void DrawColorEditor(cairo_t *cr, int x, int y, int w, int h);
    void DrawButtons(cairo_t *cr, int width, int height);
    void HandleButtonPress(int x, int y);
    void HandleKeyPress(KeySym key);
    void SaveTheme();
    void DeleteTheme();
    uint32_t PickColor(uint32_t current);

    Display *display_ = nullptr;
    Window window_ = 0;
    cairo_surface_t *surface_ = nullptr;
    cairo_t *cr_ = nullptr;
    int width_ = 520;
    int height_ = 480;

    Palette current_;
    std::vector<NamedPalette> factory_;
    std::vector<NamedPalette> user_;
    std::function<void(const Palette&, bool)> onChange_;
    std::function<void()> onClose_;

    Tab activeTab_ = Tab::Factory;
    int selectedIndex_ = -1;
    ColorEditor editor_;
    bool dragging_ = false;
    int dragStartX_ = 0, dragStartY_ = 0;

    // Colors
    static constexpr uint32_t kBg = 0x2D2D2D;
    static constexpr uint32_t kPanel = 0x3A3A3A;
    static constexpr uint32_t kText = 0xE0E0E0;
    static constexpr uint32_t kAccent = 0x5CF266;
    static constexpr uint32_t kBorder = 0x555555;
    static constexpr uint32_t kSelBg = 0x4A4A4A;
};

} // namespace oxo