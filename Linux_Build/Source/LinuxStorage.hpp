#pragma once

// Linux storage paths: ~/.config/<OXO_DATA_FOLDER> for settings, ~/Music/<OXO_LIBRARY_FOLDER> for banks.
// Portable mode: a Synth2_Data folder next to the .vst3 bundle holds all of it.

#include "EditorCore/Identity.h"
#include <filesystem>
#include <string>
#include <optional>
#include <dlfcn.h>

namespace oxo::storage::lin {

struct Paths {
    std::filesystem::path data;      // settings folder
    std::filesystem::path library;   // bank library folder
    std::filesystem::path factory;   // factory bank in bundle
    bool portable = false;
};

inline Paths Choose(const std::filesystem::path &bundlePath = {}) {
    Paths p;
    // Portable mode: Synth2_Data next to the .vst3 bundle
    if (!bundlePath.empty()) {
        std::filesystem::path portableRoot = bundlePath.parent_path().parent_path() / "Synth2_Data";
        std::error_code ec;
        if (std::filesystem::exists(portableRoot, ec) && !ec) {
            p.data = portableRoot;
            p.library = portableRoot / "Presets";
            p.factory = bundlePath.parent_path() / "Resources" / "Synth2 Factory";
            p.portable = true;
            return p;
        }
    }
    // Standard install
    const char *home = std::getenv("HOME");
    std::filesystem::path homePath = home ? home : "";
    p.data = homePath / ".config" / OXO_DATA_FOLDER;
    p.library = homePath / "Music" / OXO_LIBRARY_FOLDER;
    // Factory bank: from our own binary (the .vst3 bundle)
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void *>(&Choose), &info) && info.dli_fname) {
        p.factory = std::filesystem::path(info.dli_fname).parent_path().parent_path() / "Resources" / "Synth2 Factory";
    }
    return p;
}

inline std::filesystem::path ChosenData() { return Choose().data; }
inline std::filesystem::path ChosenLibrary() { return Choose().library; }
inline std::filesystem::path Factory() { return Choose().factory; }
inline bool IsPortable() { return Choose().portable; }

} // namespace oxo::storage::lin