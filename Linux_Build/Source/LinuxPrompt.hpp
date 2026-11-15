#pragma once

// Linux file dialogs using GTK (if available) or fallback to simple text input.
// For a minimal dependency approach, we'll use zenity if available, or a simple
// custom dialog. The Windows version uses native Win32 dialogs.

#include <string>
#include <optional>
#include <vector>

namespace oxo::linux {

// Pick a folder (for library, export bank, etc.)
std::optional<std::string> PickFolder(const char *title);

// Pick a file to open (.sy1, .zip)
std::optional<std::string> PickFileOpen(const char *title, const char *filter = "*.sy1;*.zip");

// Pick a file to save (.sy1)
std::optional<std::string> PickFileSave(const char *title, const char *defaultName = "patch.sy1");

// Show a message box (info/warning/error)
void MessageBox(const char *title, const char *message, bool isError = false);

// Show a yes/no question
bool MessageBoxYesNo(const char *title, const char *message);

} // namespace oxo::linux