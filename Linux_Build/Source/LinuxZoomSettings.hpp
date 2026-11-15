#pragma once

// Linux zoom settings persistence (editor width).
// Stored in settings.json under "editorWidth".

#include "EditorCore/Settings.hpp"
#include "LinuxStorage.hpp"
#include <optional>

namespace oxo::linux {

inline std::optional<int> LoadEditorWidth() {
    auto root = storage::lin::ChosenData();
    if (root.empty()) return std::nullopt;
    auto v = settings::Load(root, "editorWidth");
    if (!v) return std::nullopt;
    try {
        int w = std::stoi(*v);
        if (w >= 578 && w <= 2310) return w;
    } catch (...) {}
    return std::nullopt;
}

inline bool SaveEditorWidth(int width) {
    if (width < 578 || width > 2310) return false;
    return settings::Save(storage::lin::ChosenData(), "editorWidth", std::to_string(width));
}

} // namespace oxo::linux