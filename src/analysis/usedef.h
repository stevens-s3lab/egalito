#ifndef EGALITO_ANALYSIS_USEDEF_H
#define EGALITO_ANALYSIS_USEDEF_H

#include <vector>
#include <map>
#include "controlflow.h"
#include "slicingmatch.h"
#include "instr/register.h"

class Function;
class Instruction;
class TreeNode;
class Assembly;

class UDState;


// Must
class DefList {
private:
    typedef std::map<int, TreeNode *> ListType;
    ListType list;
public:
    void set(int reg, TreeNode *tree);
    TreeNode *get(int reg) const;

    ListType::iterator begin() { return list.begin(); }
    ListType::iterator end() { return list.end(); }
    ListType::const_iterator cbegin() const { return list.cbegin(); }
    ListType::const_iterator cend() const { return list.cend(); }
    void dump() const;
};

// May: evaluation must be delayed until all use-defs are determined
class RefList {
private:
    typedef std::map<int, std::vector<UDState*>> ListType;
    ListType list;
public:
    void set(int reg, UDState *origin);
    void add(int reg, UDState *origin);
    bool addIfExist(int reg, UDState *origin);
    void del(int reg);
    void clear();
    const std::vector<UDState *> *get(int reg) const;

    ListType::iterator begin() { return list.begin(); }
    ListType::iterator end() { return list.end(); }
    ListType::const_iterator cbegin() const { return list.cbegin(); }
    ListType::const_iterator cend() const { return list.cend(); }
    size_t getCount() const { return list.size(); }
    void dump() const;
};

class MemOriginList {
private:
    struct MemOrigin {
        TreeNode *place;
        UDState *origin;

        MemOrigin(TreeNode *place, UDState *origin)
            : place(place), origin(origin) {}
    };
    typedef std::vector<MemOrigin> ListType;
    ListType list;

public:
    void set(TreeNode *place, UDState *origin);
    void add(TreeNode *place, UDState *origin);
    void addList(const MemOriginList& other);
    void del(TreeNode *place);
    void clear();

    ListType::iterator begin() { return list.begin(); }
    ListType::iterator end() { return list.end(); }
    ListType::const_iterator cbegin() const { return list.cbegin(); }
    ListType::const_iterator cend() const { return list.cend(); }
    void dump() const;
};

class UDState {
public:
    virtual const ControlFlowNode *getNode() const = 0;
    virtual Instruction *getInstruction() const = 0;

    virtual void addRegDef(int reg, TreeNode *tree) = 0;
    virtual TreeNode *getRegDef(int reg) const = 0;
    virtual const DefList &getRegDefList() const = 0;
    virtual void addRegRef(int reg, UDState *origin) = 0;
    virtual void delRegRef(int reg) = 0;
    virtual const std::vector<UDState *> *getRegRef(int reg) const = 0;
    virtual const RefList& getRegRefList() const = 0;

    virtual void addMemDef(int reg, TreeNode *tree) = 0;
    virtual TreeNode *getMemDef(int reg) const = 0;
    virtual const DefList& getMemDefList() const = 0;
    virtual void addMemRef(int reg, UDState *origin) = 0;
    virtual void delMemRef(int reg) = 0;
    virtual const std::vector<UDState *> *getMemRef(int reg) const = 0;

    virtual void dumpState() const {}
};

class RegState : public UDState {
private:
    ControlFlowNode *node;
    Instruction *instruction;

    DefList regList;
    RefList regRefList;

public:
    RegState(ControlFlowNode *node, Instruction *instruction)
        : UDState(), node(node), instruction(instruction) {}

    virtual const ControlFlowNode *getNode() const { return node; }
    virtual Instruction *getInstruction() const { return instruction; }

    virtual void addRegDef(int reg, TreeNode *tree)
        { regList.set(reg, tree); }
    virtual TreeNode *getRegDef(int reg) const
        { return regList.get(reg); }
    virtual const DefList &getRegDefList() const
        { return regList; }
    virtual void addRegRef(int reg, UDState *origin)
        { regRefList.add(reg, origin); }
    virtual void delRegRef(int reg)
        { regRefList.del(reg); }
    virtual const std::vector<UDState *> *getRegRef(int reg) const
        { return regRefList.get(reg); }
    virtual const RefList& getRegRefList() const
        { return regRefList; }

    virtual void addMemDef(int reg, TreeNode *tree) {}
    virtual TreeNode *getMemDef(int reg) const
        { return nullptr; }
    virtual const DefList &getMemDefList() const
        { static DefList emptyList; return emptyList; }
    virtual void addMemRef(int reg, UDState *origin) {}
    virtual void delMemRef(int reg) {}
    virtual const std::vector<UDState *> *getMemRef(int reg) const
        { return nullptr; }

    virtual void dumpState() const { dumpRegState(); }

protected:
    void dumpRegState() const;
};

class RegMemState : public RegState {
private:
    DefList memList;
    RefList memRefList;

public:
    RegMemState(ControlFlowNode *node, Instruction *instruction)
        : RegState(node, instruction) {}

    virtual void addMemDef(int reg, TreeNode *tree)
        { memList.set(reg, tree); }
    virtual TreeNode *getMemDef(int reg) const
        { return memList.get(reg); }
    virtual const DefList &getMemDefList() const
        { return memList; }
    virtual void addMemRef(int reg, UDState *origin)
        { memRefList.add(reg, origin); }
    virtual void delMemRef(int reg)
        { memRefList.del(reg); }
    virtual const std::vector<UDState *> *getMemRef(int reg) const
        { return memRefList.get(reg); }
    virtual void dumpState() const
        { dumpRegState(); dumpMemState(); }

private:
    void dumpMemState() const;
};

class UDConfiguration {
private:
    int level;
    ControlFlowGraph *cfg;
    bool allEnabled;
    std::map<int, bool> enabled;

public:
    UDConfiguration(int level, ControlFlowGraph *cfg,
        const std::vector<int> &idList = {});

    int getLevel() const { return level; }
    ControlFlowGraph *getCFG() const { return cfg; }
    bool isEnabled(int id) const;
};

class UDWorkingSet {
private:
    std::vector<RefList> nodeExposedRegSetList;
    std::vector<MemOriginList> nodeExposedMemSetList;

    RefList *regSet;
    MemOriginList *memSet;

public:
    UDWorkingSet(ControlFlowGraph *cfg)
        : nodeExposedRegSetList(cfg->getCount()),
          nodeExposedMemSetList(cfg->getCount()),
          regSet(nullptr), memSet(nullptr) {}

    void transitionTo(ControlFlowNode *node);

    void setAsRegSet(int reg, UDState *origin)
        { regSet->set(reg, origin); }
    void addToRegSet(int reg, UDState *origin)
        { regSet->add(reg, origin); }
    const std::vector<UDState *> *getRegSet(int reg) const
        { return regSet->get(reg); }
    const RefList& getExposedRegSet(int id) const
        { return nodeExposedRegSetList[id]; }

    void setAsMemSet(TreeNode *place, UDState *origin)
        { memSet->set(place, origin); }
    void addToMemSet(TreeNode *place, UDState *origin)
        { memSet->add(place, origin); }
    void copyFromMemSetFor(UDState *state, int reg, TreeNode *place);
    const MemOriginList& getExposedMemSet(int id) const
        { return nodeExposedMemSetList[id]; }

    void dumpSet() const;

    virtual UDState *getState(Instruction *instruction)
        { return nullptr; }
};

// only works for fixed size instructions
class UDRegMemWorkingSet : public UDWorkingSet {
private:
    Function *function;
    std::vector<RegMemState> stateList;
public:
    UDRegMemWorkingSet(Function *function, ControlFlowGraph *cfg);
    virtual UDState *getState(Instruction *instruction);
    const std::vector<RegMemState> &getStateList() const
        { return stateList; }
};

class UseDef {
public:
    typedef void (UseDef::*HandlerType)(UDState *state, Assembly *assembly);

private:
    UDConfiguration *config;
    UDWorkingSet *working;

    const static std::map<int, HandlerType> handlers;

public:
    UseDef(UDConfiguration *config, UDWorkingSet *working)
        : config(config), working(working) {}

    void analyze(const std::vector<std::vector<int>>& order);

private:
    void analyzeGraph(const std::vector<int>& order);
    void fillState(UDState *state);
    bool callIfEnabled(UDState *state, Assembly *assembly);

    void fillImm(UDState *state, Assembly *assembly);
    void fillReg(UDState *state, Assembly *assembly);
    void fillRegToReg(UDState *state, Assembly *assembly);
    void fillMemToReg(UDState *state, Assembly *assembly, size_t width);
    void fillImmToReg(UDState *state, Assembly *assembly);
    void fillRegRegToReg(UDState *state, Assembly *assembly);
    void fillMemImmToReg(UDState *state, Assembly *assembly);
    void fillRegToMem(UDState *state, Assembly *assembly, size_t width);
    void fillRegImmToReg(UDState *state, Assembly *assembly);
    void fillRegRegToMem(UDState *state, Assembly *assembly);
    void fillMemToRegReg(UDState *state, Assembly *assembly);
    void fillRegRegImmToMem(UDState *state, Assembly *assembly);
    void fillMemImmToRegReg(UDState *state, Assembly *assembly);
    void fillCompareImmThenJump(UDState *state, Assembly *assembly);
    void fillCondJump(UDState *state, Assembly *assembly);

    void defReg(UDState *state, int reg, TreeNode *tree);
    void useReg(UDState *state, int reg);
    void defMem(UDState *state, TreeNode *place, int reg);
    void useMem(UDState *state, TreeNode *place, int reg);

    TreeNode *shiftExtend(TreeNode *tree, arm64_shifter type,
        unsigned int value);

    void fillAddOrSub(UDState *state, Assembly *assembly);
    void fillAdr(UDState *state, Assembly *assembly);
    void fillAdrp(UDState *state, Assembly *assembly);
    void fillAnd(UDState *state, Assembly *assembly);
    void fillBl(UDState *state, Assembly *assembly);
    void fillBlr(UDState *state, Assembly *assembly);
    void fillB(UDState *state, Assembly *assembly);
    void fillBr(UDState *state, Assembly *assembly);
    void fillCbz(UDState *state, Assembly *assembly);
    void fillCbnz(UDState *state, Assembly *assembly);
    void fillCmp(UDState *state, Assembly *assembly);
    void fillCsel(UDState *state, Assembly *assembly);
    void fillLdaxr(UDState *state, Assembly *assembly);
    void fillLdp(UDState *state, Assembly *assembly);
    void fillLdr(UDState *state, Assembly *assembly);
    void fillLdrh(UDState *state, Assembly *assembly);
    void fillLdrb(UDState *state, Assembly *assembly);
    void fillLdrsw(UDState *state, Assembly *assembly);
    void fillLdrsh(UDState *state, Assembly *assembly);
    void fillLdrsb(UDState *state, Assembly *assembly);
    void fillLdur(UDState *state, Assembly *assembly);
    void fillLsl(UDState *state, Assembly *assembly);
    void fillMov(UDState *state, Assembly *assembly);
    void fillMrs(UDState *state, Assembly *assembly);
    void fillNop(UDState *state, Assembly *assembly);
    void fillRet(UDState *state, Assembly *assembly);
    void fillStp(UDState *state, Assembly *assembly);
    void fillStr(UDState *state, Assembly *assembly);
    void fillStrb(UDState *state, Assembly *assembly);
    void fillStrh(UDState *state, Assembly *assembly);
    void fillSxtw(UDState *state, Assembly *assembly);
};

class MemLocation {
private:
    TreeNode *reg;
    long int offset;

    typedef TreePatternRecursiveBinary<TreeNodeAddition,
        TreePatternCapture<TreePatternTerminal<TreeNodePhysicalRegister>>,
        TreePatternCapture<TreePatternTerminal<TreeNodeConstant>>
    > MemoryForm;

public:
    MemLocation(TreeNode *tree) : reg(nullptr), offset(0) { extract(tree); }
    bool operator==(const MemLocation& other)
        { return this->reg->equal(other.reg) && this->offset == other.offset; }
private:
    void extract(TreeNode *tree);
};

#endif