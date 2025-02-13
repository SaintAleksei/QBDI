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
#ifndef INSTMETADATA_H
#define INSTMETADATA_H

#include "llvm/MC/MCInst.h"

#include "Utility/InstAnalysis_prive.h"

#include "QBDI/State.h"

namespace QBDI {

class InstMetadata {
public:
  llvm::MCInst inst;
  rword address;
  uint32_t instSize;
  uint32_t patchSize;
  CPUMode cpuMode;
  bool modifyPC;
  bool merge;
  uint8_t execblockFlags;
  mutable InstAnalysisPtr analysis;

  InstMetadata(const llvm::MCInst &inst, rword address = 0,
               uint32_t instSize = 0, uint32_t patchSize = 0,
               CPUMode cpuMode = CPUMode::DEFAULT, bool modifyPC = false,
               bool merge = false, uint8_t execblockFlags = 0,
               InstAnalysisPtr analysis = nullptr)
      : inst(inst), address(address), instSize(instSize), patchSize(patchSize),
        cpuMode(cpuMode), modifyPC(modifyPC), merge(merge),
        execblockFlags(execblockFlags), analysis(std::move(analysis)) {}

  inline rword endAddress() const { return address + instSize; }

  inline InstMetadata lightCopy() const {
    return {inst,     address, instSize,       patchSize, cpuMode,
            modifyPC, merge,   execblockFlags, nullptr};
  }
};

} // namespace QBDI

#endif // INSTMETADATA_H
