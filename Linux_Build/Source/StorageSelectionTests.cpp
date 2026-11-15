// Storage selection tests for Linux
// Mirrors Windows_Build/Tests/StorageSelectionTests.cpp

#include "LinuxStorage.hpp"
#include "EditorCore/Identity.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace oxo::storage::lin;

int main() {
    // Test 1: Standard paths
    {
        oxo::storage::lin::Paths p = oxo::storage::lin::Choose();
        assert(!p.data.empty());
        assert(!p.library.empty());
        assert(!p.factory.empty());
        assert(!p.portable);
        printf("Standard paths: data=%s library=%s factory=%s\n",
               p.data.string().c_str(), p.library.string().c_str(), p.factory.string().c_str());
    }

    // Test 2: Portable mode detection
    {
        // Create a temporary directory structure for portable mode
        std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / "Synth2_Linux_Test";
        std::filesystem::remove_all(tmpDir);
        std::filesystem::create_directories(tmpDir / "Synth2_Data" / "Presets");
        std::filesystem::create_directories(tmpDir / "Contents" / "Resources" / "Synth2 Factory");
        std::filesystem::path bundle = tmpDir / "Contents" / "OxoSynth2.vst3";
        std::filesystem::create_directories(bundle.parent_path());
        std::ofstream(bundle).close(); // Create empty bundle file

        oxo::storage::lin::Paths p = oxo::storage::lin::Choose(bundle);
        assert(p.portable);
        assert(p.data == tmpDir / "Synth2_Data");
        assert(p.library == tmpDir / "Synth2_Data" / "Presets");
        assert(p.factory == tmpDir / "Contents" / "Resources" / "Synth2 Factory");
        printf("Portable mode: data=%s library=%s factory=%s\n",
               p.data.string().c_str(), p.library.string().c_str(), p.factory.string().c_str());

        std::filesystem::remove_all(tmpDir);
    }

    // Test 3: ChosenData/Library/Factory helpers
    {
        assert(!oxo::storage::lin::ChosenData().empty());
        assert(!oxo::storage::lin::ChosenLibrary().empty());
        assert(!oxo::storage::lin::Factory().empty());
        assert(!oxo::storage::lin::IsPortable()); // Should be false in standard mode
    }

    printf("All Linux storage tests passed!\n");
    return 0;
}