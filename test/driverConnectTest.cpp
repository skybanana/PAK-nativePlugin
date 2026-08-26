#include "../src/main.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct PluginApi {
#ifdef _WIN32
    HMODULE module;
#endif
    unsigned int (*GetAudioDriverCount)(void);
    int (*GetAudioDriverInfo)(unsigned int, AudioDriverInfo *);
    unsigned int (*GetAudioDeviceCount)(unsigned int);
    int (*GetAudioDeviceInfo)(unsigned int, unsigned int, AudioDeviceInfo *);
    int (*InitializeWithAudioDriver)(unsigned int,
                                     unsigned int,
                                     unsigned int,
                                     unsigned int,
                                     unsigned int,
                                     unsigned int,
                                     unsigned int);
    void (*Shutdown)(void);
};

void usage(void) {
    // Prints the optional channel and sample-rate settings for this connection test.
    std::cout << "usage: driverConnectTest <inputOffset> <outputOffset> <sampleRate> <dllPath>\n";
    std::exit(0);
}

template <typename FunctionType>
bool loadPluginFunction(PluginApi *plugin, const char *name, FunctionType *outFunction) {
    // Loads one exported native plugin function by name.
#ifdef _WIN32
    *outFunction = reinterpret_cast<FunctionType>(GetProcAddress(plugin->module, name));
    return *outFunction != nullptr;
#else
    (void)plugin;
    (void)name;
    (void)outFunction;
    return false;
#endif
}

bool loadPlugin(const std::string &dllPath, PluginApi *plugin) {
    // Opens the plugin DLL and binds the audio-driver APIs used by this test.
#ifdef _WIN32
    *plugin = {};
    plugin->module = LoadLibraryA(dllPath.c_str());
    if (plugin->module == nullptr)
        return false;

    return loadPluginFunction(plugin, "GetAudioDriverCount", &plugin->GetAudioDriverCount) &&
           loadPluginFunction(plugin, "GetAudioDriverInfo", &plugin->GetAudioDriverInfo) &&
           loadPluginFunction(plugin, "GetAudioDeviceCount", &plugin->GetAudioDeviceCount) &&
           loadPluginFunction(plugin, "GetAudioDeviceInfo", &plugin->GetAudioDeviceInfo) &&
           loadPluginFunction(plugin, "InitializeWithAudioDriver", &plugin->InitializeWithAudioDriver) &&
           loadPluginFunction(plugin, "Shutdown", &plugin->Shutdown);
#else
    (void)dllPath;
    (void)plugin;
    return false;
#endif
}

void unloadPlugin(PluginApi *plugin) {
    // Releases the DLL after the test has closed its audio stream.
#ifdef _WIN32
    if (plugin->module != nullptr)
        FreeLibrary(plugin->module);
#else
    (void)plugin;
#endif
}

int main(int argc, char *argv[]) {
    unsigned int inputOffset = 0;
    unsigned int outputOffset = 0;
    unsigned int sampleRate = 48000;
    std::string dllPath = "out/build/ninja-debug/PAKNativePlugin.dll";

    if (argc > 5)
        usage();
    if (argc > 1) inputOffset = (unsigned int)std::atoi(argv[1]);
    if (argc > 2) outputOffset = (unsigned int)std::atoi(argv[2]);
    if (argc > 3) sampleRate = (unsigned int)std::atoi(argv[3]);
    if (argc > 4) dllPath = argv[4];

    PluginApi plugin = {};
    if (!loadPlugin(dllPath, &plugin)) {
        std::cerr << "Failed to load plugin DLL: " << dllPath << "\n";
        return 1;
    }

    unsigned int asioApi = 0;
    bool foundAsio = false;
    for (unsigned int index = 0; index < plugin.GetAudioDriverCount(); ++index) {
        AudioDriverInfo driver = {};
        if (plugin.GetAudioDriverInfo(index, &driver) == 0 &&
            std::strcmp(driver.name, "asio") == 0) {
            asioApi = driver.api;
            foundAsio = true;
            break;
        }
    }

    if (!foundAsio) {
        std::cerr << "ASIO is not compiled into PAKNativePlugin.dll.\n";
        unloadPlugin(&plugin);
        return 1;
    }

    AudioDeviceInfo yamaha = {};
    bool foundYamaha = false;
    unsigned int deviceCount = plugin.GetAudioDeviceCount(asioApi);
    for (unsigned int index = 0; index < deviceCount; ++index) {
        AudioDeviceInfo device = {};
        if (plugin.GetAudioDeviceInfo(asioApi, index, &device) == 0)
            std::cout << "ASIO driver: " << device.name
                      << " (input " << device.inputChannels
                      << ", output " << device.outputChannels << ")\n";

        if (std::strcmp(device.name, "Yamaha Steinberg USB ASIO") == 0) {
            yamaha = device;
            foundYamaha = true;
        }
    }

    if (!foundYamaha) {
        std::cerr << "Yamaha Steinberg USB ASIO was not selectable.\n"
                  << "RtAudio prints the ASIO driver initialization reason above.\n";
        unloadPlugin(&plugin);
        return 1;
    }

    std::cout << "Connecting Yamaha Steinberg USB ASIO: input channel " << inputOffset + 1
              << ", output channel " << outputOffset + 1
              << ", " << sampleRate << " Hz\n";
    int result = plugin.InitializeWithAudioDriver(asioApi,
                                                   1,
                                                   sampleRate,
                                                   yamaha.id,
                                                   yamaha.id,
                                                   inputOffset,
                                                   outputOffset);
    if (result != 0) {
        std::cerr << "Connection failed. RtAudio prints the driver error above.\n";
        plugin.Shutdown();
        unloadPlugin(&plugin);
        return 1;
    }

    std::cout << "Connection succeeded.\n";
    plugin.Shutdown();
    unloadPlugin(&plugin);
    return 0;
}
