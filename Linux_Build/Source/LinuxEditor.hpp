#pragma once

#include "public.sdk/source/common/pluginview.h"
#include <X11/Xlib.h>

namespace oxo {

class OxoSynth2Controller;

// Minimal Linux editor - implements IPlugView for VST3
// Full UI implementation can be added later
class LinuxEditor final : public Steinberg::CPluginView {
public:
    explicit LinuxEditor(OxoSynth2Controller* controller);
    ~LinuxEditor() override;

    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* r) override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) override;

    void attachedToParent() override;
    void removedFromParent() override;

private:
    OxoSynth2Controller* controller_ = nullptr;
    Display* display_ = nullptr;
    Window window_ = 0;
    Window parentWindow_ = 0;
    int width_ = 770;
    int height_ = 560;
    bool attached_ = false;
};

} // namespace oxo