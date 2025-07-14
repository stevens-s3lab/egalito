#include <cstdio>
#include <cstring>
#include "reloc.h"
#include "symbol.h"

#undef DEBUG_GROUP
#define DEBUG_GROUP dreloc
#include "log/log.h"

#define RELA_PREFIX ".rela"

std::string Reloc::getSymbolName() const {
    return symbol ? symbol->getName() : "???";
}

void RelocSection::add(Reloc *reloc) {
    relocList.push_back(reloc);
}

bool RelocList::add(Reloc *reloc) {
    relocList.push_back(reloc);
    address_t address = reloc->getAddress();

    // return true on success (no existing duplicate element)
    return relocMap.insert(std::make_pair(address, reloc)).second;
}

Reloc *RelocList::find(address_t address) {
    auto it = relocMap.find(address);
    return (it != relocMap.end() ? (*it).second : nullptr);
}

RelocSection *RelocList::getSection(const std::string &name) {
    auto it = sectionList.find(name);
    return (it != sectionList.end() ? (*it).second : nullptr);
}

RelocList *RelocList::buildRelocList(ElfMap *elf, SymbolList *symbolList,
    SymbolList *dynamicSymbolList) {

    RelocList *list = new RelocList();

    CLOG(0, "building relocation list");
    std::vector<void *> sectionList = elf->findSectionsByType(SHT_RELA);
    for(void *p : sectionList) {
        // Note: 64-bit x86 always uses RELA relocations (not REL),
        // according to readelf source: see the function guess_is_rela()
        ElfXX_Shdr *s = static_cast<ElfXX_Shdr *>(p);

        // We never use debug relocations, and they often contain relative
        // addresses which cannot be dereferenced directly (segfault).
        // So ignore all sections with debug relocations.
        const char *name = elf->getSHStrtab() + s->sh_name;
        if(std::strstr(name, "debug")) continue;
        LOG(1, "reloc section [" << name << ']');

        SymbolList *currentSymbolList = symbolList;
        if(std::strcmp(name, ".rela.plt") == 0
            || std::strcmp(name, ".rela.dyn") == 0) {

            currentSymbolList = dynamicSymbolList;
        }

        // We don't have the appropriate symbol section for these relocations.
        // This can happen when a shared object is statically linked.
        if(!currentSymbolList) continue;

        ElfXX_Rela *data = reinterpret_cast<ElfXX_Rela *>(
            elf->getCharmap() + s->sh_offset);

        size_t count = s->sh_size / sizeof(*data);
        for(size_t i = 0; i < count; i ++) {
            ElfXX_Rela *r = &data[i];
            auto symbolIndex = ELFXX_R_SYM(r->r_info);
            Symbol *sym = nullptr;
            if(symbolIndex > 0) {
                sym = currentSymbolList->get(symbolIndex);
            }

            address_t address = r->r_offset;
            auto type = ELFXX_R_TYPE(r->r_info);

            if(elf->isObjectFile()) {
                // If this relocation refers to a known section, add that
                // section's virtual address to the relocation address.
                // This is currently only applicable in object files.
                if(std::strncmp(RELA_PREFIX, name, std::strlen(RELA_PREFIX)) == 0) {
                    auto s = elf->findSection(name + std::strlen(RELA_PREFIX));
                    if(s) {
                        address += s->getVirtualAddress();
                    }
                }
            }

            Reloc *reloc = new Reloc(
                address,                                // address
                type,                                   // type
                ELFXX_R_SYM(r->r_info),                 // symbol index
                sym,
                r->r_addend                             // addend
            );

            CLOG0(2, "    reloc at address 0x%08lx, type %d, target [%s]\n",
                reloc->getAddress(), reloc->getType(),
                reloc->getSymbolName().c_str());

            if(!list->add(reloc)) {
                CLOG0(1, "ignoring duplicate relocation for %lx\n",
                      reloc->getAddress());
            }
            else {
                list->makeOrGetSection(name, s)->add(reloc);
            		}
        	}
    }

     // Handle RELR sections
    std::vector<void *> relrSections = elf->findSectionsByType(SHT_RELR);
    
    auto allSections = elf->getSectionList();
    ElfXX_Ehdr *header = (ElfXX_Ehdr *)elf;
    bool is_little_endian =  (header->e_ident[EI_DATA] == ELFDATA2LSB);
    auto addend_size = sizeof(Reloc::rel_addend_t);
    for (void *p : relrSections) {
        ElfXX_Shdr *s = static_cast<ElfXX_Shdr *>(p);
        const char *name = elf->getSHStrtab() + s->sh_name;
        LOG(0, "RELR section [" << name << "]");

        uint64_t *entries = reinterpret_cast<uint64_t *>(elf->getCharmap() + s->sh_offset);
        size_t count = s->sh_size / sizeof(uint64_t);
        uint64_t base_addr = 0;
	uint64_t next = base_addr;
        for (size_t i = 0; i < count; i++) {
            uint64_t entry = entries[i];
            if ((entry & 1) == 0) {
                base_addr = entry;

		for(auto sec : allSections)
		{
			auto start = sec->getVirtualAddress();
			auto end = start + sec->getSize();

			if(base_addr >= start && base_addr <= end)
			{
				char *data = elf->getSectionReadPtr<char *>(sec);
				auto offsetWithinSection = sec->convertVAToOffset(base_addr);
				const unsigned char *data_at_offset = reinterpret_cast<const unsigned char *>(data + offsetWithinSection);
				Reloc::rel_addend_t result = 0;
				if(is_little_endian)
				{
					for(int i=0; i < addend_size; i++)
					{
						result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * i);
						CLOG(0, "%02x ", data_at_offset[i]);
						LOG(0,"Printing bytes");
					}
				}
				else
				{
					/*for(int i = 0; i < addend_size; i++)
					{
						result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * (addend_size - 1 - i));*/
					for(int i = addend_size-1; i>=0; i--)
					{
						result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * i);
						CLOG(0, "%02x ", data_at_offset[i]);
                                                LOG(0,"Printing bytes in big-endian");

					}
				}
				LOG(0,"Printing reloc addend at offset "<<std::hex<<base_addr<<" 0x"<<std::hex<<result);
		                Reloc *reloc = new Reloc(base_addr, R_X86_64_RELATIVE, 0, nullptr, result);
				LOG(0, "Adding RELR reloc at address "<<std::hex<<base_addr);
		                list->add(reloc);
                		list->makeOrGetSection(name, s)->add(reloc);
				next = base_addr + sizeof(uint64_t);
			}
		}
            } 
	    else 
	    {
		   for(size_t i=0; i< sizeof(uint64_t) * 8 -1; i++)
		   {
			if ((entry >> (i+1)) & 1)
			{
				uint64_t offset = next + sizeof(uint64_t) * i;

				for(auto sec : allSections)
		                {
                		        auto start = sec->getVirtualAddress();
		                        auto end = start + sec->getSize();

                		        if(offset >= start && offset <= end)
                        		{
		                                char *data = elf->getSectionReadPtr<char *>(sec);
                		                auto offsetWithinSection = sec->convertVAToOffset(offset);
                                		const unsigned char *data_at_offset = reinterpret_cast<const unsigned char *>(data + offsetWithinSection);
		                                Reloc::rel_addend_t result = 0;
                		                if(is_little_endian)
                                		{
		                                        for(int i=0; i < addend_size; i++)
                		                        {
                                		                result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * i);
                                                		CLOG(0, "%02x ", data_at_offset[i]);
	                                                LOG(0,"Printing bytes");
        		                                }
                        		        }
		                                else
                		                {
                                		/*        for(int i = 0; i < addend_size; i++)
                                        		{
		                                                result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * (addend_size - 1 - i)); */
							for(int i = addend_size-1; i>=0; i--)
		                                        {
                		                                result |= static_cast<Reloc::rel_addend_t>(data_at_offset[i]) << (8 * i);
                		                                CLOG(0, "%02x ", data_at_offset[i]);
                                		                LOG(0,"Printing bytes in big-endian");

                                        		}
                                		}
		                                LOG(0,"Printing reloc addend at "<< offset<<" 0x"<<std::hex<<result);
                		                LOG(0, "Adding RELR reloc at address "<<std::hex<<offset);
						Reloc *reloc = new Reloc(offset, R_X86_64_RELATIVE, 0, nullptr, result);
                		        	list->add(reloc);
                        			list->makeOrGetSection(name, s)->add(reloc);
					}
				}
                    	}
                   }
		   next += sizeof(uint64_t) * (8 * sizeof(uint64_t) - 1);
            }
        }
    }

    return list;
}

RelocSection *RelocList::makeOrGetSection(const std::string &name,
    ElfXX_Shdr *s) {

    auto section = getSection(name);
    if(section) return section;

    if(s->sh_flags & SHF_INFO_LINK) {
        section = new RelocSection(name, s->sh_info);
    }
    else {
        section = new RelocSection(name);
    }
    sectionList[name] = section;
    return section;
}
