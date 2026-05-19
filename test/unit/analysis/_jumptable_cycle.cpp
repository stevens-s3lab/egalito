#include "config.h"
#include "framework/include.h"

#define private public
#include "analysis/jumptabledetection.h"
#undef private

TEST_CASE("handle base address cycle", "[analysis][fast]") {
#ifdef ARCH_X86_64
    int reg = X86_REG_R9;
    auto memory = TreeFactory::instance().make<TreeNodeAddition>(
        TreeFactory::instance().make<TreeNodePhysicalRegister>(reg, 8),
        TreeFactory::instance().make<TreeNodeConstant>(-88));

    RegMemState state(nullptr, nullptr);
    state.addRegRef(reg, &state);
    state.addRegDef(reg,
        TreeFactory::instance().make<TreeNodeDereference>(memory, 8));
    state.addMemRef(reg, &state);
    state.addMemDef(reg,
        TreeFactory::instance().make<TreeNodeAddition>(
            TreeFactory::instance().make<TreeNodePhysicalRegister>(reg, 8),
            TreeFactory::instance().make<TreeNodeConstant>(-88)));

    JumptableDetection jt(nullptr);
    bool found;
    address_t addr;
    std::tie(found, addr) = jt.parseBaseAddress(&state, reg);

    REQUIRE(!found);
    REQUIRE(addr == 0);
#endif
}
