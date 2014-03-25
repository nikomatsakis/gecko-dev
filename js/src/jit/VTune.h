/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_VTune_h
#define jit_VTune_h

#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif

class JSScript;

namespace js {
namespace jit {

class MBasicBlock;
class MacroAssembler;
class JitCode;

#ifndef MOZ_VTUNE

class VTune {
  public:
    bool startLBlock(LBlock *block, MacroAssembler &masm) {
        return true;
    }

    bool endLBlock(LBlock *block, MacroAssembler &masm) {
        return true;
    }

    bool startLInstruction(LInstruction *lir, MacroAssembler &masm) {
        return true;
    }

    bool startOutOfLine(MacroAssembler &masm) {
        return true;
    }

    bool registerMethod(JSScript *script, JitCode *code, MacroAssembler &masm) {
        return true;
    }

    static void registerRandomJitCode(JitCode *code, const char *name) {
    }
};

#else

class VTune {
//    struct BlockRecord {
//        MBasicBlock *block;
//        Label start, end;
//        size_t instrStart, instrEnd;
//
//        BlockRecord(MBasicBlock *block, size_t instrStart)
//          : block(block),
//            instrStart(instrStart),
//            instrEnd(instrEnd)
//        {}
//    };
//
//    struct InstructionRecord {
//        jsbytecode *pc;
//        Label start;
//
//        InstructionRecord(jsbytecode *pc)
//          : pc(pc)
//        {}
//    };

    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> funcCounter_;

  public:
    bool startLBlock(LBlock *block, MacroAssembler &masm);
    bool endLBlock(LBlock *block, MacroAssembler &masm);
    bool startLInstruction(LInstruction *lir, MacroAssembler &masm);
    bool startOutOfLine(MacroAssembler &masm);
    bool registerMethod(JSScript *script, JitCode *code, MacroAssembler &masm);

    static void registerRandomJitCode(JitCode *code, const char *name);
};

#endif // MOZ_VTUNE

} // jit
} // js

#endif

