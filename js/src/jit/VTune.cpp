#include "jit/VTune.h"

#include "jit/LIR.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

#ifdef MOZ_VTUNE

bool
VTune::startLBlock(LBlock *block, MacroAssembler &masm)
{
//    if (!IsVTuneProfilingActive())
//        return;
//
//    BlockRecord r(block, instructions_.length());
//    masm.bind(&r.start);
//    return basicBlocks_.append(r);
    return true;
}

bool
VTune::endLBlock(LBlock *block, MacroAssembler &masm)
{
//    if (!IsVTuneProfilingActive())
//        return;
//
//    basicBlocks_.back().instrEnd = instructions_.length();
//    masm.bind(&basicBlocks_.back().end);
//    return true;
    return true;
}

bool
VTune::startLInstruction(LInstruction *block, MacroAssembler &masm)
{
//    if (!IsVTuneProfilingActive())
//        return;
//
//    MInstruction *mir = block->mir();
//    if (!mir)
//        return true;
//
//    InstrRecord r(mir);
//    masm.bind(&r.start);
//    return instructions_.append(r);
    return true;
}

bool
VTune::startOutOfLine(MacroAssembler &masm)
{
    return true;
}

bool
VTune::registerMethod(JSScript *script, JitCode *code, MacroAssembler &masm)
{
    if (!IsVTuneProfilingActive())
        return true;

    char buffer[256];
    snprintf(buffer, 256, "%s:%d -- Func%d",
             script->filename(), script->lineno(),
             funcCounter_++);
    buffer[255] = 0; // *always* null terminate

    iJIT_Method_Load main;
    memset(&main, 0, sizeof(iJIT_Method_Load));
    main.method_id = iJIT_GetNewMethodID();
    main.method_name = buffer;
    main.method_load_address = code->raw();
    main.method_size = code->instructionsSize();

    iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&main);

    return true;
}

void
VTune::registerRandomJitCode(JitCode *code, const char *name)
{
    if (!IsVTuneProfilingActive())
        return;

    iJIT_Method_Load jmethod;
    memset(&jmethod, 0, sizeof(iJIT_Method_Load));

    jmethod.method_id = iJIT_GetNewMethodID();
    jmethod.method_name = const_cast<char*>(name);
    jmethod.method_load_address = code->raw();
    jmethod.method_size = code->instructionsSize();

    iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
}

#endif
