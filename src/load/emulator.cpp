#include <cassert>
#include <cstring>  // for memcpy in generated code
#include "emulator.h"

namespace Emulation {
    #include "dep/rtld/rtld.h"

    struct my_rtld_global _rtld_global;
    struct my_rtld_global_ro _rtld_global_ro;
    char **_dl_argv;
    int _dl_starting_up = 1;
    void *not_yet_implemented = 0;

    static void init_rtld_global(struct my_rtld_global *s) {
        using std::memcpy;
        #include "dep/rtld/rtld_data1.c"
    }
    static void init_rtld_global_ro(struct my_rtld_global_ro *s) {
        using std::memcpy;
        #include "dep/rtld/rtld_data2.c"
    }
}

void LoaderEmulator::useArgv(char **argv) {
    Emulation::_dl_argv = argv;
    addSymbol("_dl_argv", Emulation::_dl_argv);
    addSymbol("_dl_starting_up", &Emulation::_dl_starting_up);

    addSymbol("__libc_enable_secure", Emulation::not_yet_implemented);
    addSymbol("_dl_find_dso_for_object", Emulation::not_yet_implemented);
    addSymbol("__tls_get_addr", Emulation::not_yet_implemented);

    Emulation::init_rtld_global(&Emulation::_rtld_global);
    Emulation::init_rtld_global_ro(&Emulation::_rtld_global_ro);

    addSymbol("_rtld_global", &Emulation::_rtld_global);
    addSymbol("_rtld_global_ro", &Emulation::_rtld_global_ro);
}

LoaderEmulator LoaderEmulator::instance;

LoaderEmulator::LoaderEmulator() {
}

address_t LoaderEmulator::findSymbol(const std::string &symbol) {
    auto it = symbolMap.find(symbol);
    return (it != symbolMap.end() ? (*it).second : 0);
}

void LoaderEmulator::addSymbol(const std::string &symbol, const void *address) {
    symbolMap[symbol] = reinterpret_cast<address_t>(address);
}
