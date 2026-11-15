#pragma once

// Linux preset index cache: rescans library in background on every editor/browser open.
// Mirrors WindowsPresetIndex.hpp.

#include "EngineCore/PatchLibrary.hpp"
#include "LinuxStorage.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdint>

namespace oxo::linuxindex {

struct IndexEntry {
    std::string bankSource;  // relative path from library root
    std::vector<std::string> patchNames; // 128 slots, empty = free
    uint64_t mtime = 0;
    uintmax_t size = 0;
};

class PresetIndex {
public:
    static PresetIndex& Instance() {
        static PresetIndex inst;
        return inst;
    }

    // Load index from file (in settings folder)
    bool Load(const std::filesystem::path &dataRoot,
              const std::filesystem::path &libraryRoot,
              const std::filesystem::path &factoryRoot,
              std::vector<s2::PatchBank> &outBanks) {
        std::filesystem::path indexFile = dataRoot / "preset_index.bin";
        std::ifstream f(indexFile, std::ios::binary);
        if (!f) return false;

        uint32_t version = 0;
        f.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) return false;

        uint64_t libMtime = 0, factMtime = 0;
        f.read(reinterpret_cast<char*>(&libMtime), sizeof(libMtime));
        f.read(reinterpret_cast<char*>(&factMtime), sizeof(factMtime));

        // Check if library/factory mtimes match
        std::error_code ec;
        auto libTime = std::filesystem::last_write_time(libraryRoot, ec);
        auto factTime = std::filesystem::last_write_time(factoryRoot, ec);
        if (ec || libTime.time_since_epoch().count() != libMtime ||
            factTime.time_since_epoch().count() != factMtime) {
            return false; // Library changed, need rescan
        }

        uint32_t bankCount = 0;
        f.read(reinterpret_cast<char*>(&bankCount), sizeof(bankCount));
        outBanks.clear();
        outBanks.reserve(bankCount);

        for (uint32_t b = 0; b < bankCount; ++b) {
            s2::PatchBank bank;
            uint32_t nameLen = 0;
            f.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
            bank.name.resize(nameLen);
            f.read(&bank.name[0], nameLen);

            uint32_t sourceLen = 0;
            f.read(reinterpret_cast<char*>(&sourceLen), sizeof(sourceLen));
            bank.source.resize(sourceLen);
            f.read(&bank.source[0], sourceLen);

            f.read(reinterpret_cast<char*>(&bank.readOnly), sizeof(bank.readOnly));

            bank.slots.resize(128);
            for (int s = 0; s < 128; ++s) {
                uint32_t slotLen = 0;
                f.read(reinterpret_cast<char*>(&slotLen), sizeof(slotLen));
                bank.slots[s].resize(slotLen);
                f.read(&bank.slots[s][0], slotLen);
            }
            outBanks.push_back(std::move(bank));
        }
        return true;
    }

    // Save index to file
    bool Save(const std::filesystem::path &dataRoot,
              const std::filesystem::path &libraryRoot,
              const std::filesystem::path &factoryRoot,
              const std::vector<s2::PatchBank> &banks) {
        std::error_code ec;
        std::filesystem::create_directories(dataRoot, ec);
        if (ec) return false;

        std::filesystem::path indexFile = dataRoot / "preset_index.bin";
        std::filesystem::path tmpFile = indexFile.string() + ".tmp";

        std::ofstream f(tmpFile, std::ios::binary);
        if (!f) return false;

        uint32_t version = 1;
        f.write(reinterpret_cast<const char*>(&version), sizeof(version));

        auto libTime = std::filesystem::last_write_time(libraryRoot, ec);
        auto factTime = std::filesystem::last_write_time(factoryRoot, ec);
        uint64_t libMtime = ec ? 0 : libTime.time_since_epoch().count();
        uint64_t factMtime = ec ? 0 : factTime.time_since_epoch().count();
        f.write(reinterpret_cast<const char*>(&libMtime), sizeof(libMtime));
        f.write(reinterpret_cast<const char*>(&factMtime), sizeof(factMtime));

        uint32_t bankCount = static_cast<uint32_t>(banks.size());
        f.write(reinterpret_cast<const char*>(&bankCount), sizeof(bankCount));

        for (const auto &bank : banks) {
            uint32_t nameLen = static_cast<uint32_t>(bank.name.size());
            f.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
            f.write(bank.name.data(), nameLen);

            uint32_t sourceLen = static_cast<uint32_t>(bank.source.size());
            f.write(reinterpret_cast<const char*>(&sourceLen), sizeof(sourceLen));
            f.write(bank.source.data(), sourceLen);

            f.write(reinterpret_cast<const char*>(&bank.readOnly), sizeof(bank.readOnly));

            for (int s = 0; s < 128; ++s) {
                const std::string &slot = (s < static_cast<int>(bank.slots.size())) ? bank.slots[s] : std::string();
                uint32_t slotLen = static_cast<uint32_t>(slot.size());
                f.write(reinterpret_cast<const char*>(&slotLen), sizeof(slotLen));
                f.write(slot.data(), slotLen);
            }
        }

        f.flush();
        if (!f) return false;

        std::filesystem::rename(tmpFile, indexFile, ec);
        return !ec;
    }
};

} // namespace oxo::linuxindex