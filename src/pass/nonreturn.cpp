#include "nonreturn.h"
#include "analysis/controlflow.h"
#include "analysis/dominance.h"
#include "analysis/usedef.h"
#include "analysis/usedefutil.h"
#include "analysis/walker.h"
#include "chunk/concrete.h"
#ifdef ARCH_X86_64
    #include "instr/linked-x86_64.h"
#endif
#ifdef ARCH_AARCH64
    #include "instr/linked-aarch64.h"
#endif
#ifdef ARCH_RISCV
    #include "instr/linked-riscv.h"
#endif
#include "log/log.h"
#include "log/temp.h"
#include "chunk/dump.h"

// known to be non-returning in glibc (not all are standard)
const std::vector<std::string> NonReturnFunction::knownList = {
    "exit", "_exit", "abort",
    "__libc_fatal", "__assert_fail", "__stack_chk_fail",
    "__malloc_assert", "_dl_signal_error",
    "__cxa_throw",
    "_ZSt20__throw_out_of_rangePKc",
    "_ZSt19__throw_logic_errorPKc",
    "_ZSt17__throw_bad_allocv",
    "_ZSt24__throw_invalid_argumentPKc"
};

void NonReturnFunction::visit(FunctionList *functionList) {
    size_t size = 0;

    //TemporaryLogLevel tll("pass", 10);
    //TemporaryLogLevel tll2("analysis", 10);

    do {
        size = nonReturnList.size();
        recurse(functionList);
    } while(size != nonReturnList.size());
}

// Since Dominance requires an exit node to be spotted in the control flow
// graph, we should do this in two passes
void NonReturnFunction::visit(Function *function) {
    if(!function->returns()) return;

    //TemporaryLogLevel tll("pass", 10, function->hasName("mabort"));

    // step-1
    std::vector<Instruction *> GNUErrorCalls;
    for(auto block : CIter::children(function)) {
        for(auto instr : CIter::children(block)) {
            if(auto cfi = dynamic_cast<ControlFlowInstruction *>(
                instr->getSemantic())) {

                if(!cfi->returns()) continue;

                if(hasLinkToNeverReturn(cfi)) {
                    LOG(10, "non-returning call at "
                        << std::hex << instr->getAddress());
                    cfi->setNonreturn();
                    continue;
                }

                if(hasLinkToGNUError(cfi)) {
                    GNUErrorCalls.push_back(instr);
                }
            }
        }
    }

    if(!GNUErrorCalls.empty()) {
        ControlFlowGraph cfg(function);
        UDConfiguration config(&cfg);
        UDRegMemWorkingSet working(function, &cfg);
        UseDef usedef(&config, &working);

        SccOrder order(&cfg);
        order.genFull(0);
        usedef.analyze(order.get());

        for(auto instr : GNUErrorCalls) {
            bool found;
            int value;
            std::tie(found, value) = getArg0Value(working.getState(instr));
            if(found && value != 0) {
                LOG(10, "non-returning call at "
                    << std::hex << instr->getAddress());
                auto cfi = dynamic_cast<ControlFlowInstruction *>(
                    instr->getSemantic());
                cfi->setNonreturn();
            }
        }
    }

    // step-2
    if(neverReturns(function)) {
        LOG(10, "=== " << function->getName() << " never returns");
        function->setNonreturn();
        nonReturnList.insert(function);
    }
}

int NonReturnFunction::isNonReturn(ControlFlowGraph *cfg, Block* bl, std::set<Block*> visited)
{
    int ret = 1;    //Assume it is non-return

    auto noreturn_iter = noreturn_done.find(bl);
    if(noreturn_iter != noreturn_done.end())
    {
        return noreturn_iter->second;
    }
    if(visited.count(bl) != 0)  //If already visited in this path
    {
        //cout<<std::hex <<bl->getAddress()<<" returning -1 A"<<endl;
        return -1;
    }
    visited.insert(bl);

    for(auto instr : CIter::children(bl))
    {
        if(auto cfi = dynamic_cast<ControlFlowInstruction *>(
                instr->getSemantic()))
        {
            if(hasLinkToNeverReturn(cfi))
            {
                cfi->setNonreturn();
            }
            if(cfi->getMnemonic() != "jmp" && cfi->getMnemonic() != "callq")
            {
                if(auto target = dynamic_cast<Function *>(&*cfi->getLink()->getTarget())) //if cfg is a conditional jump to a function
                {
                    if(cfi->returns())
                        ret = 0;
                    break;
                }
            }
            if(!cfi->returns())   //Non-returning
            {
                //cout<<std::hex <<bl->getAddress()<<" returning 1 B"<<endl;
                return 1;
            }
        }
    }
    bool endNode = true;
    auto node_id = cfg->getIDFor(bl);
    auto node = cfg->get(node_id);
    for(auto& link : node->forwardLinks())
    {
        endNode = false;
        auto cflink = dynamic_cast<ControlFlowLink *>(&*link);
        auto dest_id = cflink->getTargetID();
        auto dest_node = (cfg->get(dest_id))->getBlock();
        int t = isNonReturn(cfg, dest_node, visited);
        if(t == -1)
            continue;
        ret = ret & t;          //Even if one path is returning, this would make the block returning
    }
    if(endNode)
        ret = 0;
    //cout<<std::hex <<bl->getAddress()<<" returning "<<ret<<endl;
    noreturn_done[bl] = ret;
    return ret;
}

bool NonReturnFunction::neverReturns(Function *function)
{
	bool isNonReturnFlag = false;
	ControlFlowGraph *cfg = new ControlFlowGraph(function);
	if(cfg == NULL)
		return isNonReturnFlag;
	if(cfg->get(0) == NULL)
		return isNonReturnFlag;
	auto start_bl = (cfg->get(0))->getBlock();
    	std::set<Block*> visited;
	noreturn_done.clear();
        int ret = isNonReturn(cfg, start_bl, visited);
        if(ret == 1)
        {
		isNonReturnFlag = true;
        }
	return isNonReturnFlag;
}

/*bool NonReturnFunction::neverReturns(Function *function) {
    ControlFlowGraph *cfg = nullptr;
    Dominance *dom = nullptr;
    for(auto block : CIter::children(function)) {
        for(auto instr : CIter::children(block)) {
            if(auto cfi = dynamic_cast<ControlFlowInstruction *>(
                instr->getSemantic())) {

                if(!cfi->returns()) {
                    if(!cfg) cfg = new ControlFlowGraph(function);
                    //ControlFlowGraph cfg(function);
                    LOG(11, "--Function " << function->getName());
                    IF_LOG(11) {
                        ChunkDumper dump;
                        function->accept(&dump);
                        cfg->dump();
                        cfg->dumpDot();
                        std::cout.flush();
                    }
                    //Dominance dom(cfg);
                    if(!dom) dom = new Dominance(cfg);
                    auto pdom = dom->getPostDominators(0);
                    auto nid = cfg->getIDFor(block);
                    if(std::find(pdom.begin(), pdom.end(), nid) == pdom.end()) {
                        continue;
                    }

                    delete cfg;
                    delete dom;
                    return true;
                }
            }
        }
    }
    delete cfg;
    delete dom;
    return false;
} */

bool NonReturnFunction::hasLinkToNeverReturn(ControlFlowInstruction *cfi) {
    if(auto pltLink = dynamic_cast<PLTLink *>(cfi->getLink())) {
        auto trampoline = pltLink->getPLTTrampoline();
	if(trampoline->getExternalSymbol())
	{
        auto pltName = trampoline->getExternalSymbol()->getName();
        for(auto name : knownList) {
            		if(pltName == name) 
			{
                return true;
            		}
            }
        }
    }
    else if(auto target = dynamic_cast<Function *>(
        &*cfi->getLink()->getTarget())) {

        if(!target->returns()) return true;
        if(inList(target)) return true;
        for(auto name : knownList) {
            if(target->hasName(name)) return true;
        }
    }

    return false;
}

bool NonReturnFunction::hasLinkToGNUError(ControlFlowInstruction *cfi) {
    if(auto pltLink = dynamic_cast<PLTLink *>(cfi->getLink())) {
        auto trampoline = pltLink->getPLTTrampoline();
	if(trampoline->getExternalSymbol())
	{
        auto pltName = trampoline->getExternalSymbol()->getName();
        return pltName == std::string("error");
	}
    }
    else if(auto target = dynamic_cast<Function *>(
        &*cfi->getLink()->getTarget())) {

        return target->hasName("error");
    }
    return false;
}

std::tuple<bool, int> NonReturnFunction::getArg0Value(UDState *state) {
    using ConstantForm =
        TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>;
    bool found = false;
    int value = 0;
    auto pred = [&](UDState *state, TreeCapture cap) {
        if(auto tree = dynamic_cast<TreeNodeConstant *>(cap.get(0))) {
            found = true;
            value = tree->getValue();
            return true;
        }
        return false;
    };

#ifdef ARCH_X86_64
    FlowUtil::searchUpDef<ConstantForm>(state, X86Register::R5, pred);
#elif defined(ARCH_AARCH64)
    FlowUtil::searchUpDef<ConstantForm>(state, AARCH64GPRegister::R0, pred);
#endif

    return std::make_tuple(found, value);

}

bool NonReturnFunction::inList(Function *function) {
    if(std::find(nonReturnList.begin(), nonReturnList.end(), function)
        != nonReturnList.end()) {

        return true;
    }
    return false;
}
