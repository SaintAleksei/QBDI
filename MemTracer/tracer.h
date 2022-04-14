#ifndef SOURCES_CPP_TRACER
#define SOURCES_CPP_TRACER

#include "QBDI.h"
#include <unordered_map>
#include <unordered_set>

namespace Tracer
{

struct MemInstrumentation
{
private:
    QBDI::VMInstanceRef vm_;
    uint32_t id_;

public:
    MemInstrumentation(QBDI::VMInstanceRef vm, QBDI::Range<QBDI::rword> range, void *context);
    MemInstrumentation(const MemInstrumentation &other) = delete;
    MemInstrumentation(MemInstrumentation &&other) = delete;
    
    ~MemInstrumentation();

    MemInstrumentation &operator=(const MemInstrumentation &other) = delete;
    MemInstrumentation &operator=(MemInstrumentation &&other) = delete;
};

struct Object
{
private:
    QBDI::Range<QBDI::rword> range_;
    MemInstrumentation inst_;

public:
    Object(QBDI::VMInstanceRef vm, QBDI::rword address, QBDI::rword size);

    const QBDI::Range<QBDI::rword> &range() const;
};

struct Link
{
    QBDI::rword fromObj_;
    QBDI::rword toObj_;
    QBDI::rword from_;
    QBDI::rword to_; 

    size_t operator()(const Link& toHash) const
    {
        return std::hash<QBDI::rword>{}(fromObj_ + toObj_ + from_ + to_);
    }
        
    bool operator==(const Link& toCompare) const 
    {
        return (fromObj_ == toCompare.fromObj_ &&
                toObj_ == toCompare.toObj_ &&
                from_ == toCompare.from_ &&
                to_ == toCompare.to_);
    }
};

struct Graph
{
private:
    static QBDI::VMInstanceRef vm_;
    std::unordered_map<QBDI::rword, std::unique_ptr<Object>> obj_;
    std::unordered_set<Link, Link> link_;

public:
    static void initInstance(QBDI::VMInstanceRef vm);
    static void saveInstanceDot(const char *fname);

    void addObject(QBDI::rword address, QBDI::rword size);
    void delObject(QBDI::rword address);
    void createLink(QBDI::rword from, QBDI::rword to);
    void saveDot(const char *fname);
};

QBDI::VMAction showInstructionCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState, 
                                 QBDI::FPRState *fprState, void *data);

std::vector<QBDI::InstrRuleDataCBK> callDetectRuleCB(QBDI::VMInstanceRef vm,
                                                     const QBDI::InstAnalysis *inst,
                                                     void *data);

}; /* Tracer */

#endif
