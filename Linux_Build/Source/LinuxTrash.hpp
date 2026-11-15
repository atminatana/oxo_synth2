#pragma once

// Linux trash (freedesktop.org spec)

#include <filesystem>

namespace oxo::trash {

bool MoveToTrash(const std::filesystem::path &path);
bool CanTrash();

} // namespace oxo::trash