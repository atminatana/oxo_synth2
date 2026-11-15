#include "LinuxPrompt.hpp"
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <array>
#include <string>
#include <optional>

namespace oxo::linux {

static std::string runCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

static bool hasZenity() {
    return system("which zenity >/dev/null 2>&1") == 0;
}

std::optional<std::string> PickFolder(const char* title) {
    if (hasZenity()) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --directory --title=%s 2>/dev/null", title);
        std::string result = runCommand(cmd);
        if (!result.empty()) return result;
    }
    // Fallback: simple text input (would need a custom dialog in practice)
    return std::nullopt;
}

std::optional<std::string> PickFileOpen(const char* title, const char* filter) {
    if (hasZenity()) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --title=%s --file-filter='%s' 2>/dev/null", title, filter);
        std::string result = runCommand(cmd);
        if (!result.empty()) return result;
    }
    return std::nullopt;
}

std::optional<std::string> PickFileSave(const char* title, const char* defaultName) {
    if (hasZenity()) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "zenity --file-selection --save --title=%s --filename=%s 2>/dev/null", title, defaultName);
        std::string result = runCommand(cmd);
        if (!result.empty()) return result;
    }
    return std::nullopt;
}

void MessageBox(const char* title, const char* message, bool isError) {
    if (hasZenity()) {
        char cmd[1024];
        const char* icon = isError ? "--error" : "--info";
        snprintf(cmd, sizeof(cmd), "zenity %s --title=%s --text=%s 2>/dev/null", icon, title, message);
        system(cmd);
    } else {
        fprintf(stderr, "%s: %s\n", title, message);
    }
}

bool MessageBoxYesNo(const char* title, const char* message) {
    if (hasZenity()) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "zenity --question --title=%s --text=%s 2>/dev/null", title, message);
        return system(cmd) == 0;
    }
    return false;
}

} // namespace oxo::linux