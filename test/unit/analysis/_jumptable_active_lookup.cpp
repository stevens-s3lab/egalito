#include "config.h"
#include "framework/include.h"

#define private public
#include "analysis/jumptabledetection.h"
#undef private

TEST_CASE("skip active base address cycle", "[analysis][fast]") {
    JumptableDetection jt(nullptr);
    auto state = reinterpret_cast<UDState *>(0x1);
    int reg = 9;

    jt.activeBaseAddressLookups.insert(std::make_pair(state, reg));

    bool found;
    address_t addr;
    std::tie(found, addr) = jt.parseBaseAddress(state, reg);

    REQUIRE(!found);
    REQUIRE(addr == 0);
}
