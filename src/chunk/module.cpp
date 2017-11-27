#include <string>
#include <sstream>
#include "module.h"
#include "elf/elfspace.h"
#include "elf/sharedlib.h"
#include "serializer.h"
#include "visitor.h"
#include "log/log.h"

void Module::setElfSpace(ElfSpace *space) {
    elfSpace = space;

    // set name based on ElfSpace
    std::ostringstream stream;
    auto lib = elfSpace->getLibrary();
    if(lib) {
        stream << "module-" << lib->getShortName();
    }
    else {
        stream << "module-main";
    }
    setName(stream.str());
}

void Module::serialize(ChunkSerializerOperations &op,
    ArchiveStreamWriter &writer) {

    writer.writeAnyLength(getName());

    auto pltListID = op.serialize(getPLTList());
    writer.write(pltListID);

    auto functionListID = op.serialize(getFunctionList());
    writer.write(functionListID);
}

bool Module::deserialize(ChunkSerializerOperations &op,
    ArchiveStreamReader &reader) {

    std::string name;
    reader.readAnyLength(name);
    setName(name);

    LOG(1, "trying to parse Module [" << name << "]");

    {
        uint32_t id;
        reader.read(id);
        op.deserialize(op.lookupFlat(id));
        auto pltList = op.lookupAs<PLTList>(id);
        getChildren()->add(pltList);
        setPLTList(pltList);
    }

    {
        uint32_t id;
        reader.read(id);
        op.deserialize(op.lookupFlat(id));
        auto functionList = op.lookupAs<FunctionList>(id);
        getChildren()->add(functionList);
        setFunctionList(functionList);
    }

    return reader.stillGood();
}

void Module::accept(ChunkVisitor *visitor) {
    visitor->visit(this);
}
