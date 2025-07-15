#include "resolveplt.h"
#include "chunk/resolver.h"
#include "elf/symbol.h"
#include "chunk/program.h"
#include "load/emulator.h"
#include "operation/find2.h"

#include "log/log.h"
#include "log/temp.h"
#include "conductor/conductor.h"

void ResolvePLTPass::visit(Module *module) {
    LOG(1, "resolving PLT for " << module->getName());
    this->module = module;
    recurse(module);
}

void ResolvePLTPass::visit(PLTList *pltList) {
    recurse(pltList);
}

void ResolvePLTPass::visit(PLTTrampoline *pltTrampoline) {
    if(pltTrampoline->getTarget()) return;  // already resolved

    Chunk *target = nullptr;
    auto symbol = pltTrampoline->getExternalSymbol();
	
    auto link = PerfectLinkResolver().resolveExternallyStrongWeak(
        symbol, conductor, module, true);
    if(link) {
        target = link->getTarget();
        delete link;
    }

    if(symbol->getName().rfind("unresolved_plt", 0) == 0)
    {
	    auto address = pltTrampoline->getGotPLTEntry();
	    LOG(1, "Processing "<<symbol->getName()<<" "<<symbol<<" @ "<<address);
	    auto ds = module->getDataRegionList()->findDataSectionContaining(address);
	    if(ds->getType() == DataSection:: TYPE_DATA)
	    {
		    LOG(1,"Address "<<std::hex<<address<<" DS data");
		    for(auto dv : CIter::children(ds))
		    {
			    if(dv->getAddress() != address)
				    continue;
			    LOG(1, "DV address "<<dv->getAddress()<<" "<<dv->getName());
			    auto dvLink = dv->getDest();
			    if(dvLink)
			    {
				    //LOG(1, "DV Link");
				    if(dvLink->getTarget())
				    {
					    //LOG(1, "DV Target"<<dvLink->getTargetAddress());
					    if(auto link_target = dynamic_cast<Function *>(&*dvLink->getTarget()))
					    {
						    symbol->setResolved(link_target);
						    symbol->setName(link_target->getName());
                                                    target = link_target;
                                                    LOG(1, "Fixed: Resolved unresolved PLT to "<<link_target->getName());
						    break;
					    }
				    }
			    }
		    }
	    }


    }

    if(!target) {
        // sometimes a PLT target is found in itself with version (e.g. __netf2)
        auto version = symbol->getVersion();
        std::string lookupName = symbol->getName();
        LOG(1, "internal lookup of [" << lookupName << "]");
        if(version) {
            if(!target) target = ChunkFind2().findFunctionInModule(
                (lookupName + "@@" + version->getName()).c_str(), module);
            if(!target) target = ChunkFind2().findFunctionInModule(
                (lookupName + "@" + version->getName()).c_str(), module);
            //lookupName.push_back('@');
            //if(!version->isHidden()) lookupName.push_back('@');
            //lookupName.append(version->getName());
        }
        if(!target) {
            target = ChunkFind2().findFunctionInModule(symbol->getName().c_str(), module);
        }
    }
    if(target) {
        LOG(10, "PLT to " << symbol->getName()
            << " resolved to " << target->getName()
            << " in " << target->getParent()->getParent()->getName());
        symbol->setResolved(target);

        if(target->getParent()) {
            symbol->setResolvedModule(dynamic_cast<Module *>(
                target->getParent()->getParent()));
        }
    }
    else {
        LOG(1, "unresolved pltTrampoline target "
            << symbol->getName() << " unused?");
#if 0
        if(symbol->getName() == "__netf2") {
            for(auto m : CIter::modules(conductor->getProgram())) {
                LOG(1, "checking in " << m->getName());
                for(auto f : CIter::functions(m)) {
                    if(m->getName() == "module-libgcc_s.so.1") {
                        LOG(1, "    " << f->getName());
                    }
                    if(f->hasName(symbol->getName())) {
                        LOG(1, "here! " << f->getName()
                            << " in " << m->getName());
                    }
                }
            }
            LOG(1, "not found?");
            std::cout.flush();
            exit(1);
        }
#endif
    }
}
