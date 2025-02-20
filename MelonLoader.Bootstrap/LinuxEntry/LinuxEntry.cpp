#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdlib.h>
#include "./plthook/plthook.h"

extern "C"
{
    // Init from NativeAOT
    void Init(void *hBootstrap);
}

static bool initialized = false;
void *dlsym_hook(void *handle, const char *name)
{
    if (!initialized && (std::strcmp(name, "il2cpp_init") == 0 || std::strcmp(name, "mono_jit_init_version") == 0))
    {
        initialized = true;
        void *hUnity = dlopen("MelonLoader.Bootstrap.so", RTLD_NOW);
        if (hUnity == nullptr)
        {
            std::cout << "Couldn't find the bootstrap's handle" << std::endl;
            return dlsym(handle, name);
        }
        Init(hUnity);
    }

    return dlsym(handle, name);
}

int fclose_hook(FILE *stream)
{
    // Some versions of Unity wrongly close stdout, which prevents writing
    // to console
    if (stream == stdout)
        return 0;
    return fclose(stream);
}

int dup2_hook(int od, int nd) {
    // Newer versions of Unity redirect stdout to player.log, we don't want
    // that
    if (nd == fileno(stdout) || nd == fileno(stderr))
        return 0;
    return dup2(od, nd);
}

__attribute__((constructor))
void library_init()
{
    // std::ifstream cmdLineFile;
    // cmdLineFile.open("/proc/self/cmdline");
    // char cmdLine[1024];
    // cmdLineFile.getline(cmdLine, 1024, '\0');
    // cmdLineFile.close();
    //
    // std::cout << cmdLine << std::endl;
    unsetenv("LD_PRELOAD");
    
    std::cout << "ENTERING!!!" << std::endl;

    plthook_t *hook;
    if (plthook_open(&hook, "UnityPlayer.so") == 0)
    {
        std::cout << "Found UnityPlayer, hooking into it instead" << std::endl;
    }
    else if (plthook_open(&hook, nullptr) != 0)
    {
        std::cout << "Failed to open current process PLT! Cannot run MelonLoader! Error: ";
        std::cout << plthook_error();
        std::cout << std::endl;
        return;
    }

    if (plthook_replace(hook, "dlsym", &dlsym_hook, nullptr) != 0)
    {
        std::cout << "Failed to hook dlsym, ignoring it. Error: ";
        std::cout << plthook_error();
        std::cout << std::endl;
    }

    // if (plthook_replace(hook, "fclose", &fclose_hook, nullptr) != 0)
    // {
    //     std::cout << "Failed to hook fclose, ignoring it. Error: ";
    //     std::cout << plthook_error();
    //     std::cout << std::endl;
    // }
    
    if (plthook_replace(hook, "dup2", &dup2_hook, nullptr) != 0)
    {
        std::cout << "Failed to hook dup2, ignoring it. Error: ";
        std::cout << plthook_error();
        std::cout << std::endl;
    }

    plthook_close(hook);
}