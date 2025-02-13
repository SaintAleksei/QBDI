/*
 * This file is part of QBDI.
 *
 * Copyright 2017 - 2022 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "instrumented.h"
#include "pipes.h"

#include <QBDI.h>
#include "Utility/LogSys.h"

int SAVED_ERRNO = 0;

struct Pipes {
  FILE *ctrlPipe;
  FILE *dataPipe;
};

static QBDI::VMAction step(QBDI::VMInstanceRef vm, QBDI::GPRState *gprState,
                           QBDI::FPRState *fprState, void *data) {
  COMMAND cmd;
  SAVED_ERRNO = errno;
  Pipes *pipes = (Pipes *)data;

  const QBDI::InstAnalysis *instAnalysis = vm->getInstAnalysis(
      QBDI::ANALYSIS_INSTRUCTION | QBDI::ANALYSIS_DISASSEMBLY);
  // Write a new instruction event
  if (writeInstructionEvent(instAnalysis->address, instAnalysis->mnemonic,
                            instAnalysis->disassembly, gprState, fprState,
                            pipes->dataPipe) != 1) {
    // DATA pipe failure, we exit
    QBDI_ERROR("Lost the data pipe, exiting!");
    return QBDI::VMAction::STOP;
  }
  // Read next command
  if (readCommand(&cmd, pipes->ctrlPipe) != 1) {
    // CTRL pipe failure, we exit
    QBDI_ERROR("Lost the control pipe, exiting!");
    return QBDI::VMAction::STOP;
  }

  errno = SAVED_ERRNO;
  if (cmd == COMMAND::CONTINUE) {
    // Signaling the VM to continue the execution
    return QBDI::VMAction::CONTINUE;
  } else if (cmd == COMMAND::STOP) {
    // Signaling the VM to stop the execution
    return QBDI::VMAction::STOP;
  }
  // FATAL
  QBDI_ERROR("Did not recognize command from the control pipe, exiting!");
  return QBDI::VMAction::STOP;
}

static QBDI::VMAction verifyMemoryAccess(QBDI::VMInstanceRef vm,
                                         QBDI::GPRState *gprState,
                                         QBDI::FPRState *fprState, void *data) {
  SAVED_ERRNO = errno;
  Pipes *pipes = (Pipes *)data;

  const QBDI::InstAnalysis *instAnalysis =
      vm->getInstAnalysis(QBDI::ANALYSIS_INSTRUCTION);

  bool mayRead = instAnalysis->mayLoad_LLVM;
  bool mayWrite = instAnalysis->mayStore_LLVM;

  bool doRead = false;
  bool doWrite = false;
  std::vector<QBDI::MemoryAccess> accesses = vm->getInstMemoryAccess();
  for (auto &m : accesses) {
    if ((m.type & QBDI::MEMORY_READ) != 0) {
      doRead = true;
    }
    if ((m.type & QBDI::MEMORY_WRITE) != 0) {
      doWrite = true;
    }
  }

  // llvm API mayRead and mayWrite are incomplete.
  bool bypassRead = false;
  bool bypassWrite = false;

  if (doRead and !mayRead) {
#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
    // all return instructions read the return address.
    bypassRead |= instAnalysis->isReturn;
    const std::set<std::string> shouldReadInsts{
        "ARPL16mr", "BOUNDS16rm", "BOUNDS32rm",    "CMPSB",   "CMPSL",
        "CMPSQ",    "CMPSW",      "LODSB",         "LODSL",   "LODSQ",
        "LODSW",    "LRETIL",     "LRETIQ",        "LRETIW",  "LRETL",
        "LRETQ",    "LRETW",      "MOVDIR64B16",   "MOVSB",   "MOVSL",
        "MOVSQ",    "MOVSW",      "OR32mi8Locked", "RCL16m1", "RCL16mCL",
        "RCL16mi",  "RCL32m1",    "RCL32mCL",      "RCL32mi", "RCL64m1",
        "RCL64mCL", "RCL64mi",    "RCL8m1",        "RCL8mCL", "RCL8mi",
        "RCR16m1",  "RCR16mCL",   "RCR16mi",       "RCR32m1", "RCR32mCL",
        "RCR32mi",  "RCR64m1",    "RCR64mCL",      "RCR64mi", "RCR8m1",
        "RCR8mCL",  "RCR8mi",     "RETIL",         "RETIQ",   "RETIW",
        "RETL",     "RETQ",       "RETW",          "SCASB",   "SCASL",
        "SCASQ",    "SCASW",
    };
    bypassRead |= (shouldReadInsts.count(instAnalysis->mnemonic) == 1);
#endif
  } else if (!doRead and mayRead) {
#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
    const std::set<std::string> noReadInsts{
        "CLDEMOTE",    "CLFLUSH",      "CLFLUSHOPT", "CLWB",
        "FEMMS",       "FXSAVE",       "FXSAVE64",   "INT",
        "INT3",        "LFENCE",       "LOADIWKEY",  "MFENCE",
        "MMX_EMMS",    "MMX_MOVNTQmr", "MOVDIRI32",  "MOVDIRI64",
        "MWAITrr",     "PAUSE",        "PREFETCH",   "PREFETCHNTA",
        "PREFETCHT0",  "PREFETCHT1",   "PREFETCHT2", "PREFETCHW",
        "PREFETCHWT1", "PTWRITE64r",   "PTWRITEr",   "RDFSBASE",
        "RDFSBASE64",  "RDGSBASE",     "RDGSBASE64", "RDPID32",
        "SERIALIZE",   "SFENCE",       "STTILECFG",  "TILERELEASE",
        "TRAP",        "UMONITOR16",   "UMONITOR32", "UMONITOR64",
        "WRFSBASE",    "WRFSBASE64",   "WRGSBASE",   "WRGSBASE64",
        "XSETBV",
    };
    bypassRead |= (noReadInsts.count(instAnalysis->mnemonic) == 1);
#endif
  }

  if (doWrite and !mayWrite) {
#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
    // all call instructions write the return address.
    bypassWrite |= instAnalysis->isCall;
    const std::set<std::string> shouldWriteInsts{
        "CALL16m",     "CALL16m_NT",    "CALL16r",       "CALL16r_NT",
        "CALL32m",     "CALL32m_NT",    "CALL32r",       "CALL32r_NT",
        "CALL64m",     "CALL64m_NT",    "CALL64pcrel32", "CALL64r",
        "CALL64r_NT",  "CALLpcrel16",   "CALLpcrel32",   "ENTER",
        "MOVDIR64B16", "MOVSB",         "MOVSL",         "MOVSQ",
        "MOVSW",       "OR32mi8Locked", "STOSB",         "STOSL",
        "STOSQ",       "STOSW",
    };
    bypassWrite |= (shouldWriteInsts.count(instAnalysis->mnemonic) == 1);
#endif
  } else if (!doWrite and mayWrite) {
#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
    const std::set<std::string> noWriteInsts{
        "CLDEMOTE",    "CLFLUSH",    "CLFLUSHOPT", "CLWB",        "FEMMS",
        "FXRSTOR",     "FXRSTOR64",  "INT",        "INT3",        "LDMXCSR",
        "LDTILECFG",   "LFENCE",     "LOADIWKEY",  "MFENCE",      "MMX_EMMS",
        "MWAITrr",     "PAUSE",      "PREFETCH",   "PREFETCHNTA", "PREFETCHT0",
        "PREFETCHT1",  "PREFETCHT2", "PREFETCHW",  "PREFETCHWT1", "PTWRITE64m",
        "PTWRITE64r",  "PTWRITEm",   "PTWRITEr",   "RDFSBASE",    "RDFSBASE64",
        "RDGSBASE",    "RDGSBASE64", "RDPID32",    "SERIALIZE",   "SFENCE",
        "TILERELEASE", "UMONITOR16", "UMONITOR32", "UMONITOR64",  "VLDMXCSR",
        "WRFSBASE",    "WRFSBASE64", "WRGSBASE",   "WRGSBASE64",  "XRSTOR",
        "XRSTOR64",    "XRSTORS",    "XRSTORS64",  "XSETBV",
    };
    bypassWrite |= (noWriteInsts.count(instAnalysis->mnemonic) == 1);
#endif
  }

  if ((doRead == mayRead || bypassRead) &&
      (doWrite == mayWrite || bypassWrite)) {
    errno = SAVED_ERRNO;
    return QBDI::VMAction::CONTINUE;
  }

  // Write a new instruction event
  if (writeMismatchMemAccessEvent(
          instAnalysis->address, doRead, instAnalysis->mayLoad, doWrite,
          instAnalysis->mayStore, accesses, pipes->dataPipe) != 1) {
    // DATA pipe failure, we exit
    QBDI_ERROR("Lost the data pipe, exiting!");
    return QBDI::VMAction::STOP;
  }
  errno = SAVED_ERRNO;
  // Continue the execution
  return QBDI::VMAction::CONTINUE;
}

#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
static QBDI::VMAction logSyscall(QBDI::VMInstanceRef vm,
                                 QBDI::GPRState *gprState,
                                 QBDI::FPRState *fprState, void *data) {
  Pipes *pipes = (Pipes *)data;
  // We don't have the address, it just need to be different from 0
  writeExecTransferEvent(1, pipes->dataPipe);
  return QBDI::VMAction::CONTINUE;
}
#endif

static QBDI::VMAction logTransfer(QBDI::VMInstanceRef vm,
                                  const QBDI::VMState *state,
                                  QBDI::GPRState *gprState,
                                  QBDI::FPRState *fprState, void *data) {
  Pipes *pipes = (Pipes *)data;
  writeExecTransferEvent(state->basicBlockStart, pipes->dataPipe);
  return QBDI::VMAction::CONTINUE;
}

static QBDI::VMAction saveErrno(QBDI::VMInstanceRef vm,
                                const QBDI::VMState *state,
                                QBDI::GPRState *gprState,
                                QBDI::FPRState *fprState, void *data) {
  SAVED_ERRNO = errno;
  return QBDI::VMAction::CONTINUE;
}

static QBDI::VMAction restoreErrno(QBDI::VMInstanceRef vm,
                                   const QBDI::VMState *state,
                                   QBDI::GPRState *gprState,
                                   QBDI::FPRState *fprState, void *data) {
  errno = SAVED_ERRNO;
  return QBDI::VMAction::CONTINUE;
}

Pipes PIPES = {NULL, NULL};
QBDI::VM *VM;

void cleanup_instrumentation() {
  static bool cleaned_up = false;
  if (cleaned_up == false) {
    writeEvent(EVENT::EXIT, PIPES.dataPipe);
    fclose(PIPES.ctrlPipe);
    fclose(PIPES.dataPipe);
    delete VM;
    cleaned_up = true;
  }
}

void start_instrumented(QBDI::VM *vm, QBDI::rword start, QBDI::rword stop,
                        int ctrlfd, int datafd) {

  VM = vm;
  if (getenv("QBDI_DEBUG") != NULL) {
    QBDI::setLogPriority(QBDI::LogPriority::DEBUG);
  } else {
    QBDI::setLogPriority(QBDI::LogPriority::ERROR);
  }
  // Opening communication FIFO
  PIPES.ctrlPipe = fdopen(ctrlfd, "rb");
  PIPES.dataPipe = fdopen(datafd, "wb");
  if (PIPES.ctrlPipe == nullptr || PIPES.dataPipe == nullptr) {
    QBDI_ERROR("Could not open communication pipes with master, exiting!");
    return;
  }

  vm->addCodeCB(QBDI::PREINST, step, (void *)&PIPES);
#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86) || \
    defined(QBDI_ARCH_AARCH64)
  // memory Access are not supported for ARM now
  vm->recordMemoryAccess(QBDI::MEMORY_READ_WRITE);
  vm->addCodeCB(QBDI::POSTINST, verifyMemoryAccess, (void *)&PIPES);
#endif

#if defined(QBDI_ARCH_X86_64) || defined(QBDI_ARCH_X86)
  vm->addMnemonicCB("syscall", QBDI::POSTINST, logSyscall, (void *)&PIPES);
#endif
  vm->addVMEventCB(QBDI::VMEvent::EXEC_TRANSFER_CALL, logTransfer,
                   (void *)&PIPES);
  vm->addVMEventCB(QBDI::VMEvent::EXEC_TRANSFER_CALL |
                       QBDI::VMEvent::BASIC_BLOCK_ENTRY,
                   restoreErrno, nullptr);
  vm->addVMEventCB(QBDI::VMEvent::EXEC_TRANSFER_RETURN |
                       QBDI::VMEvent::BASIC_BLOCK_EXIT,
                   saveErrno, nullptr);

  vm->run(start, stop);

  cleanup_instrumentation();
}
