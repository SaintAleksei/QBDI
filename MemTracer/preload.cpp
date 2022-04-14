#include "QBDIPreload.h"
#include "QBDI.h"
#include "tracer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Шаблон для QBDIPreload, всё интересное в qbdipreload_on_run
 */

extern "C" 
{

QBDIPRELOAD_INIT;

int qbdipreload_on_start(void *main)
{
    return QBDIPRELOAD_NOT_HANDLED;
}

int qbdipreload_on_premain(void *gprCtx, void *fpuCtx)
{
    return QBDIPRELOAD_NOT_HANDLED;
}

int qbdipreload_on_main(int argc, char **argv)
{
    return QBDIPRELOAD_NOT_HANDLED;
} 

int qbdipreload_on_run(QBDI::VMInstanceRef vm, QBDI::rword start, QBDI::rword stop)
{
    Tracer::Graph::initInstance(vm);

    vm->addInstrumentedModuleFromAddr(start);

    vm->addInstrRule(Tracer::callDetectRuleCB, QBDI::ANALYSIS_INSTRUCTION, nullptr);

    vm->run(start, stop);

    Tracer::Graph::saveInstanceDot("traced.dot");

    return QBDIPRELOAD_NO_ERROR;
}

int qbdipreload_on_exit(int status)
{ 
    return QBDIPRELOAD_NOT_HANDLED;
}

}
