#include "tracer.h"
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <iomanip>

namespace Tracer
{

static QBDI::VMAction callDetectCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                                   QBDI::FPRState *fprState, void *data);

static QBDI::VMAction mallocCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                               QBDI::FPRState *fprState, void *data);

static QBDI::VMAction freeCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                             QBDI::FPRState *fprState, void *data);

static QBDI::VMAction objModifyCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState, 
                                  QBDI::FPRState *fprState, void *data);

QBDI::VMInstanceRef Graph::vm_ = nullptr;

Graph graphInstance;

MemInstrumentation::MemInstrumentation(QBDI::VMInstanceRef vm,
                                       QBDI::Range<QBDI::rword> range,
                                       void *context) :
    vm_(vm)
{
    id_ = vm_->addMemRangeCB(range.start(), range.end(),
                             QBDI::MEMORY_WRITE,
                             objModifyCB,
                             context);
    assert(id_ != QBDI::INVALID_EVENTID);
} 

MemInstrumentation::~MemInstrumentation()
{
    vm_->deleteInstrumentation(id_); 
}

Object::Object(QBDI::VMInstanceRef vm, 
               QBDI::rword address,
               QBDI::rword size):
    range_(address, address + size),
    inst_(vm, {address, address + size}, static_cast<void *>(this))
{
}

const QBDI::Range<QBDI::rword> &Tracer::Object::range() const
{
    return range_;
}

void Graph::initInstance(QBDI::VMInstanceRef vm)
{
    vm_ = vm;
}

void Graph::saveInstanceDot(const char *fname)
{
    graphInstance.saveDot(fname);
}

void Graph::addObject(QBDI::rword address, QBDI::rword size)
{
    obj_[address] = std::make_unique<Object>(vm_, address, size);
}

void Graph::delObject(QBDI::rword address)
{
    for (auto it = link_.begin(), end = link_.end(); it != end; ++it)
        if (it->fromObj_ == address || it->toObj_ == address)
            link_.erase(it);

    obj_.erase(address);
}

void Graph::createLink(QBDI::rword from, QBDI::rword to)
{
    QBDI::rword fromObj = 0;
    QBDI::rword toObj = 0;

    for (auto it = obj_.begin(), end = obj_.end(); it != end; ++it)
    {
        const QBDI::Range<QBDI::rword> &range = it->second->range();

        if (fromObj == 0 && range.contains(from))
            fromObj = it->first;
        
        if (toObj == 0 && range.contains(to))
            toObj = it->first;

        if (fromObj != 0 && toObj != 0)
        {
            link_.insert({fromObj, toObj, from, to});
            break;
        }
    }
}

void Graph::saveDot(const char *fname)
{
    assert(fname);

    FILE *stream = fopen(fname, "w");
    assert(stream);

    fprintf(stream, "Digraph G{\n");

    for (auto it = obj_.begin(), end = obj_.end(); it != end; ++it)
        fprintf(stream, "\tnode0x%016lx[label=\"0x%016lx\"];\n", 
                        it->first, it->first);

    for (auto it = link_.begin(), end = link_.end(); it != end; ++it)
        fprintf(stream, "\tnode0x%016lx->node0x%016lx;\n", 
                        it->fromObj_, it->toObj_);

    fprintf(stream, "}\n");

    fclose(stream);
}

/*
 * Callbacks
 */

QBDI::VMAction showInstructionCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                                 QBDI::FPRState *fprState, void *data)
{
    const QBDI::InstAnalysis *inst = vm->getInstAnalysis(); 

    std::cout << std::setbase(16) << inst->address << ": "
              << std::setbase(10) << inst->disassembly << std::endl;

    return QBDI::CONTINUE;
}

std::vector<QBDI::InstrRuleDataCBK> callDetectRuleCB(QBDI::VMInstanceRef vm,
                                                     const QBDI::InstAnalysis *inst,
                                                     void *data)
{
    if (inst->isBranch)
        return {{QBDI::POSTINST, callDetectCB, NULL}};

    return {};
} 

struct FuncCxt
{
    std::vector<QBDI::rword> arg;
    uint32_t instId;
};

static QBDI::VMAction callDetectCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                                   QBDI::FPRState *fprState, void *data)
{

    if (gprState->rip == reinterpret_cast<QBDI::rword>(malloc))
    {
        FuncCxt *cxt = new FuncCxt;

        cxt->arg.push_back(gprState->rdi);
        cxt->instId = vm->addCodeAddrCB(*reinterpret_cast<QBDI::rword *>(gprState->rsp),
                                        QBDI::PREINST,
                                        mallocCB,
                                        static_cast<void *>(cxt));
        assert(cxt->instId != QBDI::INVALID_EVENTID);

        return QBDI::BREAK_TO_VM;
    }

    if (gprState->rip == reinterpret_cast<QBDI::rword>(free))
    {
        FuncCxt *cxt = new FuncCxt;
    

        cxt->arg.push_back(gprState->rdi);
        cxt->instId = vm->addCodeAddrCB(*reinterpret_cast<QBDI::rword *>(gprState->rsp),
                                       QBDI::PREINST,
                                       freeCB,
                                       static_cast<void *>(cxt));
        assert(cxt->instId != QBDI::INVALID_EVENTID);

        return QBDI::BREAK_TO_VM;
    }
    
    return QBDI::CONTINUE;
}

static QBDI::VMAction mallocCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                               QBDI::FPRState *fprState, void *data)
{
    assert(data != nullptr);

    FuncCxt *cxt = static_cast<FuncCxt *>(data);

    if (gprState->rax != 0)
        graphInstance.addObject(gprState->rax, cxt->arg[0]);
     
    vm->deleteInstrumentation(cxt->instId);

    delete cxt;

    return QBDI::BREAK_TO_VM;
}

static QBDI::VMAction freeCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                             QBDI::FPRState *fprState, void *data)
{
    assert(data != nullptr);

    FuncCxt *cxt = static_cast<FuncCxt *>(data);

    graphInstance.delObject(cxt->arg[0]);
     
    vm->deleteInstrumentation(cxt->instId);

    delete cxt;

    return QBDI::BREAK_TO_VM;
}

static QBDI::VMAction objModifyCB(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState, 
                                  QBDI::FPRState *fprState, void *data)
{
    assert(data != nullptr);

    Tracer::Object *objptr = static_cast<Tracer::Object *>(data);
    auto memAccess = vm->getInstMemoryAccess(); 

    for (auto it = memAccess.begin(), end = memAccess.end(); it != end; ++it)
    {
        if ((it->type & QBDI::MEMORY_WRITE) == 0 ||
            !objptr->range().contains(it->accessAddress))
            break;

        graphInstance.createLink(it->accessAddress, it->value);
    }

    return QBDI::CONTINUE;
}

}
