#include "LinuxEditor.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstring>
#include <algorithm>

using namespace Steinberg;

namespace oxo {

LinuxEditor::LinuxEditor(OxoSynth2Controller* controller)
    : controller_(controller) {}

LinuxEditor::~LinuxEditor() {
    if (window_ && display_) {
        XDestroyWindow(display_, window_);
    }
    if (display_) {
        XCloseDisplay(display_);
    }
}

tresult LinuxEditor::isPlatformTypeSupported(FIDString type) {
    return strcmp(type, kPlatformTypeX11EmbedWindowID) == 0 ? kResultTrue : kResultFalse;
}

tresult LinuxEditor::checkSizeConstraint(ViewRect* r) {
    if (!r) return kResultFalse;
    // Minimum 75%, maximum 300% of design size (770x560)
    float scale = 1.0f;
    if (r->getWidth() > 0) scale = static_cast<float>(r->getWidth()) / 770.0f;
    scale = std::clamp(scale, 0.75f, 3.0f);
    r->right = r->left + static_cast<int>(770.0f * scale + 0.5f);
    r->bottom = r->top + static_cast<int>(560.0f * scale + 0.5f);
    return kResultTrue;
}

tresult LinuxEditor::onSize(ViewRect* r) {
    if (!r) return kResultFalse;
    width_ = r->right - r->left;
    height_ = r->bottom - r->top;
    if (width_ <= 0 || height_ <= 0) return kResultFalse;
    
    if (window_ && display_) {
        XResizeWindow(display_, window_, width_, height_);
    }
    return kResultTrue;
}

void LinuxEditor::attachedToParent() {
    if (attached_) return;
    
    // Get parent window from host (set via IPlugFrame::setWindow)
    parentWindow_ = reinterpret_cast<Window>(systemWindow);
    if (!parentWindow_) return;

    display_ = XOpenDisplay(nullptr);
    if (!display_) return;

    // Create editor window as child of parent
    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | StructureNotifyMask;
    attrs.background_pixel = 0x2D2D2D; // Dark background matching theme
    
    window_ = XCreateWindow(display_, parentWindow_, 0, 0, width_, height_, 0,
                            CopyFromParent, InputOutput, CopyFromParent,
                            CWEventMask | CWBackPixel, &attrs);

    XClassHint classHint;
    classHint.res_name = const_cast<char*>("OxoSynth2Editor");
    classHint.res_class = const_cast<char*>("OxoSynth2Editor");
    XSetClassHint(display_, window_, &classHint);

    XMapWindow(display_, window_);
    attached_ = true;
}

void LinuxEditor::removedFromParent() {
    if (!attached_) return;
    
    if (window_ && display_) {
        XDestroyWindow(display_, window_);
        window_ = 0;
    }
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    attached_ = false;
}

} // namespace oxo