// Linux trash implementation using freedesktop.org Trash specification.
// Moves files/folders to ~/.local/share/Trash/files with .trashinfo in ~/.local/share/Trash/info.

#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace oxo {

static std::filesystem::path trashRoot() {
    const char *home = std::getenv("HOME");
    std::filesystem::path base = home ? std::filesystem::path(home) : std::filesystem::path("/tmp");
    return base / ".local" / "share" / "Trash";
}

static std::filesystem::path trashFiles() { return trashRoot() / "files"; }
static std::filesystem::path trashInfo() { return trashRoot() / "info"; }

static void ensureTrashDirs() {
    std::error_code ec;
    std::filesystem::create_directories(trashFiles(), ec);
    std::filesystem::create_directories(trashInfo(), ec);
}

static std::string makeTrashName(const std::filesystem::path &path) {
    std::string stem = path.filename().string();
    std::string ext = path.extension().string();
    std::string name = stem.substr(0, stem.size() - ext.size());
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s_%lld%s", name.c_str(), static_cast<long long>(ms), ext.c_str());
    return std::string(buf);
}

static void writeTrashInfo(const std::filesystem::path &infoPath, const std::filesystem::path &originalPath) {
    std::ofstream f(infoPath);
    if (!f) return;
    char timeBuf[32];
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", std::localtime(&tt));
    f << "[Trash Info]\n";
    f << "Path=" << originalPath.string() << "\n";
    f << "DeletionDate=" << timeBuf << "\n";
}

bool MoveToTrash(const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) return false;

    ensureTrashDirs();

    std::string trashName = makeTrashName(path);
    std::filesystem::path destFile = trashFiles() / trashName;
    std::filesystem::path destInfo = trashInfo() / (trashName + ".trashinfo");

    // Handle name collisions
    int suffix = 1;
    while (std::filesystem::exists(destFile, ec) && !ec) {
        std::string stem = path.stem().string();
        std::string ext = path.extension().string();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s_%d%s", stem.c_str(), suffix++, ext.c_str());
        trashName = buf;
        destFile = trashFiles() / trashName;
        destInfo = trashInfo() / (trashName + ".trashinfo");
    }

    // Move the file/folder
    try {
        std::filesystem::rename(path, destFile, ec);
        if (ec) return false;
    } catch (...) {
        return false;
    }

    writeTrashInfo(destInfo, path);
    return true;
}

} // namespace oxo