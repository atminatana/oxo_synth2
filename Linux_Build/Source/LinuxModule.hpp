#pragma once

// X11 window class management for the editor window.
// Registers our own window class on the module, unregisters with the last window.

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <mutex>
#include <unordered_map>

namespace oxo::linux {

class WindowClass {
public:
    static WindowClass &Instance() {
        static WindowClass inst;
        return inst;
    }

    // Get or create the window class atom for our editor
    Atom GetEditorClass(Display *display) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = atoms_.find(display);
        if (it != atoms_.end()) return it->second;

        // Register window class
        XClassHint classHint;
        classHint.res_name = const_cast<char*>("OxoSynth2Editor");
        classHint.res_class = const_cast<char*>("OxoSynth2Editor");

        Atom atom = XInternAtom(display, "OxoSynth2Editor", False);
        atoms_[display] = atom;
        return atom;
    }

    void Unregister(Display *display) {
        std::lock_guard<std::mutex> lock(mutex_);
        atoms_.erase(display);
    }

private:
    WindowClass() = default;
    std::mutex mutex_;
    std::unordered_map<Display*, Atom> atoms_;
};

} // namespace oxo::linux