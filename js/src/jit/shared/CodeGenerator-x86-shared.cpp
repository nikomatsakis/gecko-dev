/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/SIMD.h"
#include "builtin/TypedObject.h"
#include "jit/shared/CodeGenerator-x86-shared.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/IonFrames.h"
#include "jit/JitCompartment.h"
#include "jit/RangeAnalysis.h"

#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DoubleSignificandBits;
using mozilla::FloatSignificandBits;
using mozilla::FloorLog2;
using mozilla::NegativeInfinity;
using mozilla::SpecificNaN;
using mozilla::SpecificFloatNaN;

namespace js {
namespace jit {

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorShared(gen, graph, masm)
{
}

bool
CodeGeneratorX86Shared::generatePrologue()
{
    // Note that this automatically sets MacroAssembler::framePushed().
    masm.reserveStack(frameSize());

    return true;
}

bool
CodeGeneratorX86Shared::generateEpilogue()
{
    masm.bind(&returnLabel_);

#if JS_TRACE_LOGGING
    masm.tracelogStop();
#endif

    // Pop the stack we allocated at the start of the function.
    masm.freeStack(frameSize());
    JS_ASSERT(masm.framePushed() == 0);

    masm.ret();
    return true;
}

bool
OutOfLineBailout::accept(CodeGeneratorX86Shared *codegen)
{
    return codegen->visitOutOfLineBailout(this);
}

void
CodeGeneratorX86Shared::emitBranch(Assembler::Condition cond, MBasicBlock *mirTrue,
                                   MBasicBlock *mirFalse, Assembler::NaNCond ifNaN)
{
    if (ifNaN == Assembler::NaN_IsFalse)
        jumpToBlock(mirFalse, Assembler::Parity);
    else if (ifNaN == Assembler::NaN_IsTrue)
        jumpToBlock(mirTrue, Assembler::Parity);

    if (isNextBlock(mirFalse->lir())) {
        jumpToBlock(mirTrue, cond);
    } else {
        jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
        jumpToBlock(mirTrue);
    }
}

bool
CodeGeneratorX86Shared::visitDouble(LDouble *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitFloat32(LFloat32 *ins)
{
    const LDefinition *out = ins->getDef(0);
    masm.loadConstantFloat32(ins->getFloat(), ToFloatRegister(out));
    return true;
}

bool
CodeGeneratorX86Shared::visitTestIAndBranch(LTestIAndBranch *test)
{
    const LAllocation *opd = test->input();

    // Test the operand
    masm.testl(ToRegister(opd), ToRegister(opd));
    emitBranch(Assembler::NonZero, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitTestDAndBranch(LTestDAndBranch *test)
{
    const LAllocation *opd = test->input();

    // ucomisd flags:
    //             Z  P  C
    //            ---------
    //      NaN    1  1  1
    //        >    0  0  0
    //        <    0  0  1
    //        =    1  0  0
    //
    // NaN is falsey, so comparing against 0 and then using the Z flag is
    // enough to determine which branch to take.
    masm.xorpd(ScratchFloatReg, ScratchFloatReg);
    masm.ucomisd(ToFloatRegister(opd), ScratchFloatReg);
    emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitTestFAndBranch(LTestFAndBranch *test)
{
    const LAllocation *opd = test->input();
    // ucomiss flags are the same as doubles; see comment above
    masm.xorps(ScratchFloatReg, ScratchFloatReg);
    masm.ucomiss(ToFloatRegister(opd), ScratchFloatReg);
    emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitBitAndAndBranch(LBitAndAndBranch *baab)
{
    if (baab->right()->isConstant())
        masm.testl(ToRegister(baab->left()), Imm32(ToInt32(baab->right())));
    else
        masm.testl(ToRegister(baab->left()), ToRegister(baab->right()));
    emitBranch(Assembler::NonZero, baab->ifTrue(), baab->ifFalse());
    return true;
}

void
CodeGeneratorX86Shared::emitCompare(MCompare::CompareType type, const LAllocation *left, const LAllocation *right)
{
#ifdef JS_CPU_X64
    if (type == MCompare::Compare_Object) {
        masm.cmpq(ToRegister(left), ToOperand(right));
        return;
    }
#endif

    if (right->isConstant())
        masm.cmpl(ToRegister(left), Imm32(ToInt32(right)));
    else
        masm.cmpl(ToRegister(left), ToOperand(right));
}

bool
CodeGeneratorX86Shared::visitCompare(LCompare *comp)
{
    MCompare *mir = comp->mir();
    emitCompare(mir->compareType(), comp->left(), comp->right());
    masm.emitSet(JSOpToCondition(mir->compareType(), comp->jsop()), ToRegister(comp->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareAndBranch(LCompareAndBranch *comp)
{
    MCompare *mir = comp->cmpMir();
    emitCompare(mir->compareType(), comp->left(), comp->right());
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), comp->jsop());
    emitBranch(cond, comp->ifTrue(), comp->ifFalse());
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareD(LCompareD *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->mir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareDouble(cond, lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareF(LCompareF *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->mir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareFloat(cond, lhs, rhs);
    masm.emitSet(Assembler::ConditionFromDoubleCondition(cond), ToRegister(comp->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitNotI(LNotI *ins)
{
    masm.cmpl(ToRegister(ins->input()), Imm32(0));
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()));
    return true;
}

bool
CodeGeneratorX86Shared::visitNotD(LNotD *ins)
{
    FloatRegister opd = ToFloatRegister(ins->input());

    // Not returns true if the input is a NaN. We don't have to worry about
    // it if we know the input is never NaN though.
    Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
    if (ins->mir()->operandIsNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.xorpd(ScratchFloatReg, ScratchFloatReg);
    masm.compareDouble(Assembler::DoubleEqualOrUnordered, opd, ScratchFloatReg);
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitNotF(LNotF *ins)
{
    FloatRegister opd = ToFloatRegister(ins->input());

    // Not returns true if the input is a NaN. We don't have to worry about
    // it if we know the input is never NaN though.
    Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
    if (ins->mir()->operandIsNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.xorps(ScratchFloatReg, ScratchFloatReg);
    masm.compareFloat(Assembler::DoubleEqualOrUnordered, opd, ScratchFloatReg);
    masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareDAndBranch(LCompareDAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->cmpMir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareDouble(cond, lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse(), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitCompareFAndBranch(LCompareFAndBranch *comp)
{
    FloatRegister lhs = ToFloatRegister(comp->left());
    FloatRegister rhs = ToFloatRegister(comp->right());

    Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->cmpMir()->jsop());

    Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
    if (comp->cmpMir()->operandsAreNeverNaN())
        nanCond = Assembler::NaN_HandledByCond;

    masm.compareFloat(cond, lhs, rhs);
    emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(), comp->ifFalse(), nanCond);
    return true;
}

bool
CodeGeneratorX86Shared::visitAsmJSPassStackArg(LAsmJSPassStackArg *ins)
{
    const MAsmJSPassStackArg *mir = ins->mir();
    Address dst(StackPointer, mir->spOffset());
    if (ins->arg()->isConstant()) {
        masm.storePtr(ImmWord(ToInt32(ins->arg())), dst);
    } else {
        if (ins->arg()->isGeneralReg())
            masm.storePtr(ToRegister(ins->arg()), dst);
        else
            masm.storeDouble(ToFloatRegister(ins->arg()), dst);
    }
    return true;
}

bool
CodeGeneratorX86Shared::generateOutOfLineCode()
{
    if (!CodeGeneratorShared::generateOutOfLineCode())
        return false;

    if (deoptLabel_.used()) {
        // All non-table-based bailouts will go here.
        masm.bind(&deoptLabel_);

        // Push the frame size, so the handler can recover the IonScript.
        masm.push(Imm32(frameSize()));

        IonCode *handler = gen->jitRuntime()->getGenericBailoutHandler();
        masm.jmp(ImmPtr(handler->raw()), Relocation::IONCODE);
    }

    return true;
}

class BailoutJump {
    Assembler::Condition cond_;

  public:
    BailoutJump(Assembler::Condition cond) : cond_(cond)
    { }
#ifdef JS_CPU_X86
    void operator()(MacroAssembler &masm, uint8_t *code) const {
        masm.j(cond_, ImmPtr(code), Relocation::HARDCODED);
    }
#endif
    void operator()(MacroAssembler &masm, Label *label) const {
        masm.j(cond_, label);
    }
};

class BailoutLabel {
    Label *label_;

  public:
    BailoutLabel(Label *label) : label_(label)
    { }
#ifdef JS_CPU_X86
    void operator()(MacroAssembler &masm, uint8_t *code) const {
        masm.retarget(label_, ImmPtr(code), Relocation::HARDCODED);
    }
#endif
    void operator()(MacroAssembler &masm, Label *label) const {
        masm.retarget(label_, label);
    }
};

template <typename T> bool
CodeGeneratorX86Shared::bailout(const T &binder, LSnapshot *snapshot)
{
    CompileInfo &info = snapshot->mir()->block()->info();
    switch (info.executionMode()) {
      case ParallelExecution: {
        // in parallel mode, make no attempt to recover, just signal an error.
        OutOfLineAbortPar *ool = oolAbortPar(ParallelBailoutUnsupported,
                                             snapshot->mir()->block(),
                                             snapshot->mir()->pc());
        binder(masm, ool->entry());
        return true;
      }
      case SequentialExecution:
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    if (!encode(snapshot))
        return false;

    // Though the assembler doesn't track all frame pushes, at least make sure
    // the known value makes sense. We can't use bailout tables if the stack
    // isn't properly aligned to the static frame size.
    JS_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                 frameClass_.frameSize() == masm.framePushed());

#ifdef JS_CPU_X86
    // On x64, bailout tables are pointless, because 16 extra bytes are
    // reserved per external jump, whereas it takes only 10 bytes to encode a
    // a non-table based bailout.
    if (assignBailoutId(snapshot)) {
        binder(masm, deoptTable_->raw() + snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE);
        return true;
    }
#endif

    // We could not use a jump table, either because all bailout IDs were
    // reserved, or a jump table is not optimal for this frame size or
    // platform. Whatever, we will generate a lazy bailout.
    OutOfLineBailout *ool = new(alloc()) OutOfLineBailout(snapshot);
    if (!addOutOfLineCode(ool))
        return false;

    binder(masm, ool->entry());
    return true;
}

bool
CodeGeneratorX86Shared::bailoutIf(Assembler::Condition condition, LSnapshot *snapshot)
{
    return bailout(BailoutJump(condition), snapshot);
}

bool
CodeGeneratorX86Shared::bailoutIf(Assembler::DoubleCondition condition, LSnapshot *snapshot)
{
    JS_ASSERT(Assembler::NaNCondFromDoubleCondition(condition) == Assembler::NaN_HandledByCond);
    return bailoutIf(Assembler::ConditionFromDoubleCondition(condition), snapshot);
}

bool
CodeGeneratorX86Shared::bailoutFrom(Label *label, LSnapshot *snapshot)
{
    JS_ASSERT(label->used() && !label->bound());
    return bailout(BailoutLabel(label), snapshot);
}

bool
CodeGeneratorX86Shared::bailout(LSnapshot *snapshot)
{
    Label label;
    masm.jump(&label);
    return bailoutFrom(&label, snapshot);
}

bool
CodeGeneratorX86Shared::visitOutOfLineBailout(OutOfLineBailout *ool)
{
    masm.push(Imm32(ool->snapshot()->snapshotOffset()));
    masm.jmp(&deoptLabel_);
    return true;
}


bool
CodeGeneratorX86Shared::visitMinMaxD(LMinMaxD *ins)
{
    FloatRegister first = ToFloatRegister(ins->first());
    FloatRegister second = ToFloatRegister(ins->second());
#ifdef DEBUG
    FloatRegister output = ToFloatRegister(ins->output());
    JS_ASSERT(first == output);
#endif

    Label done, nan, minMaxInst;

    // Do a ucomisd to catch equality and NaNs, which both require special
    // handling. If the operands are ordered and inequal, we branch straight to
    // the min/max instruction. If we wanted, we could also branch for less-than
    // or greater-than here instead of using min/max, however these conditions
    // will sometimes be hard on the branch predictor.
    masm.ucomisd(first, second);
    masm.j(Assembler::NotEqual, &minMaxInst);
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN())
        masm.j(Assembler::Parity, &nan);

    // Ordered and equal. The operands are bit-identical unless they are zero
    // and negative zero. These instructions merge the sign bits in that
    // case, and are no-ops otherwise.
    if (ins->mir()->isMax())
        masm.andpd(second, first);
    else
        masm.orpd(second, first);
    masm.jump(&done);

    // x86's min/max are not symmetric; if either operand is a NaN, they return
    // the read-only operand. We need to return a NaN if either operand is a
    // NaN, so we explicitly check for a NaN in the read-write operand.
    if (!ins->mir()->range() || ins->mir()->range()->canBeNaN()) {
        masm.bind(&nan);
        masm.ucomisd(first, first);
        masm.j(Assembler::Parity, &done);
    }

    // When the values are inequal, or second is NaN, x86's min and max will
    // return the value we need.
    masm.bind(&minMaxInst);
    if (ins->mir()->isMax())
        masm.maxsd(second, first);
    else
        masm.minsd(second, first);

    masm.bind(&done);
    return true;
}

bool
CodeGeneratorX86Shared::visitAbsD(LAbsD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));
    // Load a value which is all ones except for the sign bit.
    masm.loadConstantDouble(SpecificNaN(0, DoubleSignificandBits), ScratchFloatReg);
    masm.andpd(ScratchFloatReg, input);
    return true;
}

bool
CodeGeneratorX86Shared::visitAbsF(LAbsF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));
    // Same trick as visitAbsD above.
    masm.loadConstantFloat32(SpecificFloatNaN(0, FloatSignificandBits), ScratchFloatReg);
    masm.andps(ScratchFloatReg, input);
    return true;
}

bool
CodeGeneratorX86Shared::visitSqrtD(LSqrtD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.sqrtsd(input, output);
    return true;
}

bool
CodeGeneratorX86Shared::visitSqrtF(LSqrtF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister output = ToFloatRegister(ins->output());
    masm.sqrtss(input, output);
    return true;
}

bool
CodeGeneratorX86Shared::visitPowHalfD(LPowHalfD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));

    Label done, sqrt;

    if (!ins->mir()->operandIsNeverNegativeInfinity()) {
        // Branch if not -Infinity.
        masm.loadConstantDouble(NegativeInfinity(), ScratchFloatReg);

        Assembler::DoubleCondition cond = Assembler::DoubleNotEqualOrUnordered;
        if (ins->mir()->operandIsNeverNaN())
            cond = Assembler::DoubleNotEqual;
        masm.branchDouble(cond, input, ScratchFloatReg, &sqrt);

        // Math.pow(-Infinity, 0.5) == Infinity.
        masm.xorpd(input, input);
        masm.subsd(ScratchFloatReg, input);
        masm.jump(&done);

        masm.bind(&sqrt);
    }

    if (!ins->mir()->operandIsNeverNegativeZero()) {
        // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5). Adding 0 converts any -0 to 0.
        masm.xorpd(ScratchFloatReg, ScratchFloatReg);
        masm.addsd(ScratchFloatReg, input);
    }

    masm.sqrtsd(input, input);

    masm.bind(&done);
    return true;
}

class OutOfLineUndoALUOperation : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    LInstruction *ins_;

  public:
    OutOfLineUndoALUOperation(LInstruction *ins)
        : ins_(ins)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitOutOfLineUndoALUOperation(this);
    }
    LInstruction *ins() const {
        return ins_;
    }
};

bool
CodeGeneratorX86Shared::visitAddI(LAddI *ins)
{
    if (ins->rhs()->isConstant())
        masm.addl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.addl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot()) {
        if (ins->recoversInput()) {
            OutOfLineUndoALUOperation *ool = new(alloc()) OutOfLineUndoALUOperation(ins);
            if (!addOutOfLineCode(ool))
                return false;
            masm.j(Assembler::Overflow, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Overflow, ins->snapshot()))
                return false;
        }
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitSubI(LSubI *ins)
{
    if (ins->rhs()->isConstant())
        masm.subl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
    else
        masm.subl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));

    if (ins->snapshot()) {
        if (ins->recoversInput()) {
            OutOfLineUndoALUOperation *ool = new(alloc()) OutOfLineUndoALUOperation(ins);
            if (!addOutOfLineCode(ool))
                return false;
            masm.j(Assembler::Overflow, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Overflow, ins->snapshot()))
                return false;
        }
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitOutOfLineUndoALUOperation(OutOfLineUndoALUOperation *ool)
{
    LInstruction *ins = ool->ins();
    Register reg = ToRegister(ins->getDef(0));

    mozilla::DebugOnly<LAllocation *> lhs = ins->getOperand(0);
    LAllocation *rhs = ins->getOperand(1);

    JS_ASSERT(reg == ToRegister(lhs));
    JS_ASSERT_IF(rhs->isGeneralReg(), reg != ToRegister(rhs));

    // Undo the effect of the ALU operation, which was performed on the output
    // register and overflowed. Writing to the output register clobbered an
    // input reg, and the original value of the input needs to be recovered
    // to satisfy the constraint imposed by any RECOVERED_INPUT operands to
    // the bailout snapshot.

    if (rhs->isConstant()) {
        Imm32 constant(ToInt32(rhs));
        if (ins->isAddI())
            masm.subl(constant, reg);
        else
            masm.addl(constant, reg);
    } else {
        if (ins->isAddI())
            masm.subl(ToOperand(rhs), reg);
        else
            masm.addl(ToOperand(rhs), reg);
    }

    return bailout(ool->ins()->snapshot());
}

class MulNegativeZeroCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    LMulI *ins_;

  public:
    MulNegativeZeroCheck(LMulI *ins)
      : ins_(ins)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitMulNegativeZeroCheck(this);
    }
    LMulI *ins() const {
        return ins_;
    }
};

bool
CodeGeneratorX86Shared::visitMulI(LMulI *ins)
{
    const LAllocation *lhs = ins->lhs();
    const LAllocation *rhs = ins->rhs();
    MMul *mul = ins->mir();
    JS_ASSERT_IF(mul->mode() == MMul::Integer, !mul->canBeNegativeZero() && !mul->canOverflow());

    if (rhs->isConstant()) {
        // Bailout on -0.0
        int32_t constant = ToInt32(rhs);
        if (mul->canBeNegativeZero() && constant <= 0) {
            Assembler::Condition bailoutCond = (constant == 0) ? Assembler::Signed : Assembler::Equal;
            masm.testl(ToRegister(lhs), ToRegister(lhs));
            if (!bailoutIf(bailoutCond, ins->snapshot()))
                    return false;
        }

        switch (constant) {
          case -1:
            masm.negl(ToOperand(lhs));
            break;
          case 0:
            masm.xorl(ToOperand(lhs), ToRegister(lhs));
            return true; // escape overflow check;
          case 1:
            // nop
            return true; // escape overflow check;
          case 2:
            masm.addl(ToOperand(lhs), ToRegister(lhs));
            break;
          default:
            if (!mul->canOverflow() && constant > 0) {
                // Use shift if cannot overflow and constant is power of 2
                int32_t shift = FloorLog2(constant);
                if ((1 << shift) == constant) {
                    masm.shll(Imm32(shift), ToRegister(lhs));
                    return true;
                }
            }
            masm.imull(Imm32(ToInt32(rhs)), ToRegister(lhs));
        }

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;
    } else {
        masm.imull(ToOperand(rhs), ToRegister(lhs));

        // Bailout on overflow
        if (mul->canOverflow() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
            return false;

        if (mul->canBeNegativeZero()) {
            // Jump to an OOL path if the result is 0.
            MulNegativeZeroCheck *ool = new(alloc()) MulNegativeZeroCheck(ins);
            if (!addOutOfLineCode(ool))
                return false;

            masm.testl(ToRegister(lhs), ToRegister(lhs));
            masm.j(Assembler::Zero, ool->entry());
            masm.bind(ool->rejoin());
        }
    }

    return true;
}

class ReturnZero : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    Register reg_;

  public:
    explicit ReturnZero(Register reg)
      : reg_(reg)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitReturnZero(this);
    }
    Register reg() const {
        return reg_;
    }
};

bool
CodeGeneratorX86Shared::visitReturnZero(ReturnZero *ool)
{
    masm.mov(ImmWord(0), ool->reg());
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86Shared::visitUDivOrMod(LUDivOrMod *ins)
{
    JS_ASSERT(ToRegister(ins->lhs()) == eax);
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    JS_ASSERT_IF(output == eax, ToRegister(ins->remainder()) == edx);

    ReturnZero *ool = nullptr;

    // Prevent divide by zero.
    if (ins->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (ins->mir()->isTruncated()) {
            if (!ool)
                ool = new(alloc()) ReturnZero(output);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    masm.mov(ImmWord(0), edx);
    masm.udiv(rhs);

    // Unsigned div or mod can return a value that's not a signed int32.
    // If our users aren't expecting that, bail.
    if (!ins->mir()->isTruncated()) {
        masm.testl(output, output);
        if (!bailoutIf(Assembler::Signed, ins->snapshot()))
            return false;
    }

    if (ool) {
        if (!addOutOfLineCode(ool))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitMulNegativeZeroCheck(MulNegativeZeroCheck *ool)
{
    LMulI *ins = ool->ins();
    Register result = ToRegister(ins->output());
    Operand lhsCopy = ToOperand(ins->lhsCopy());
    Operand rhs = ToOperand(ins->rhs());
    JS_ASSERT_IF(lhsCopy.kind() == Operand::REG, lhsCopy.reg() != result.code());

    // Result is -0 if lhs or rhs is negative.
    masm.movl(lhsCopy, result);
    masm.orl(rhs, result);
    if (!bailoutIf(Assembler::Signed, ins->snapshot()))
        return false;

    masm.mov(ImmWord(0), result);
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86Shared::visitDivPowTwoI(LDivPowTwoI *ins)
{
    Register lhs = ToRegister(ins->numerator());
    mozilla::DebugOnly<Register> output = ToRegister(ins->output());
    int32_t shift = ins->shift();

    // We use defineReuseInput so these should always be the same, which is
    // convenient since all of our instructions here are two-address.
    JS_ASSERT(lhs == output);

    if (shift != 0) {
        MDiv *mir = ins->mir();
        if (!mir->isTruncated()) {
            // If the remainder is != 0, bailout since this must be a double.
            masm.testl(lhs, Imm32(UINT32_MAX >> (32 - shift)));
            if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
                return false;
        }

        if (!mir->canBeNegativeDividend()) {
            // Numerator is unsigned, so needs no adjusting. Do the shift.
            masm.sarl(Imm32(shift), lhs);
            return true;
        }

        // Adjust the value so that shifting produces a correctly rounded result
        // when the numerator is negative. See 10-1 "Signed Division by a Known
        // Power of 2" in Henry S. Warren, Jr.'s Hacker's Delight.
        Register lhsCopy = ToRegister(ins->numeratorCopy());
        JS_ASSERT(lhsCopy != lhs);
        if (shift > 1)
            masm.sarl(Imm32(31), lhs);
        masm.shrl(Imm32(32 - shift), lhs);
        masm.addl(lhsCopy, lhs);

        // Do the shift.
        masm.sarl(Imm32(shift), lhs);
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitDivSelfI(LDivSelfI *ins)
{
    Register op = ToRegister(ins->op());
    Register output = ToRegister(ins->output());
    MDiv *mir = ins->mir();

    // If we can't divide by zero, lowering should have just used a constant one.
    JS_ASSERT(mir->canBeDivideByZero());

    masm.testl(op, op);
    if (mir->isTruncated()) {
        masm.emitSet(Assembler::NonZero, output);
    } else {
       if (!bailoutIf(Assembler::Zero, ins->snapshot()))
           return false;
        masm.mov(ImmWord(1), output);
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitDivI(LDivI *ins)
{
    Register remainder = ToRegister(ins->remainder());
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register output = ToRegister(ins->output());

    MDiv *mir = ins->mir();

    JS_ASSERT(remainder == edx);
    JS_ASSERT(lhs == eax);
    JS_ASSERT(output == eax);

    Label done;
    ReturnZero *ool = nullptr;

    // Handle divide by zero.
    if (mir->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (mir->isTruncated()) {
            // Truncated division by zero is zero (Infinity|0 == 0)
            if (!ool)
                ool = new(alloc()) ReturnZero(output);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            JS_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    // Handle an integer overflow exception from -2147483648 / -1.
    if (mir->canBeNegativeOverflow()) {
        Label notmin;
        masm.cmpl(lhs, Imm32(INT32_MIN));
        masm.j(Assembler::NotEqual, &notmin);
        masm.cmpl(rhs, Imm32(-1));
        if (mir->isTruncated()) {
            // (-INT32_MIN)|0 == INT32_MIN and INT32_MIN is already in the
            // output register (lhs == eax).
            masm.j(Assembler::Equal, &done);
        } else {
            JS_ASSERT(mir->fallible());
            if (!bailoutIf(Assembler::Equal, ins->snapshot()))
                return false;
        }
        masm.bind(&notmin);
    }

    // Handle negative 0.
    if (!mir->isTruncated() && mir->canBeNegativeZero()) {
        Label nonzero;
        masm.testl(lhs, lhs);
        masm.j(Assembler::NonZero, &nonzero);
        masm.cmpl(rhs, Imm32(0));
        if (!bailoutIf(Assembler::LessThan, ins->snapshot()))
            return false;
        masm.bind(&nonzero);
    }

    // Sign extend eax into edx to make (edx:eax), since idiv is 64-bit.
    masm.cdq();
    masm.idiv(rhs);

    if (!mir->isTruncated()) {
        // If the remainder is > 0, bailout since this must be a double.
        masm.testl(remainder, remainder);
        if (!bailoutIf(Assembler::NonZero, ins->snapshot()))
            return false;
    }

    masm.bind(&done);

    if (ool) {
        if (!addOutOfLineCode(ool))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitModSelfI(LModSelfI *ins)
{
    Register op = ToRegister(ins->op());
    Register output = ToRegister(ins->output());
    MMod *mir = ins->mir();

    // If we're not fallible, lowering should have just used a constant zero.
    JS_ASSERT(mir->fallible());
    JS_ASSERT(mir->canBeDivideByZero() || (!mir->isUnsigned() && mir->canBeNegativeDividend()));

    masm.testl(op, op);

    // For a negative operand, we need to return negative zero. We can't
    // represent that as an int32, so bail if that happens.
    if (!mir->isUnsigned() && mir->canBeNegativeDividend()) {
        if (!bailoutIf(Assembler::Signed, ins->snapshot()))
             return false;
    }

    // For a zero operand, we need to return NaN. We can't
    // represent that as an int32, so bail if that happens.
    if (mir->canBeDivideByZero()) {
        if (!bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
    }

    // For any other value, return 0.
    masm.mov(ImmWord(0), output);

    return true;
}

bool
CodeGeneratorX86Shared::visitModPowTwoI(LModPowTwoI *ins)
{
    Register lhs = ToRegister(ins->getOperand(0));
    int32_t shift = ins->shift();

    Label negative;

    if (ins->mir()->canBeNegativeDividend()) {
        // Switch based on sign of the lhs.
        // Positive numbers are just a bitmask
        masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
    }

    masm.andl(Imm32((1 << shift) - 1), lhs);

    if (ins->mir()->canBeNegativeDividend()) {
        Label done;
        masm.jump(&done);

        // Negative numbers need a negate, bitmask, negate
        masm.bind(&negative);
        // visitModI has an overflow check here to catch INT_MIN % -1, but
        // here the rhs is a power of 2, and cannot be -1, so the check is not generated.
        masm.negl(lhs);
        masm.andl(Imm32((1 << shift) - 1), lhs);
        masm.negl(lhs);
        if (!ins->mir()->isTruncated() && !bailoutIf(Assembler::Zero, ins->snapshot()))
            return false;
        masm.bind(&done);
    }
    return true;

}

class ModOverflowCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    Label done_;
    LModI *ins_;
    Register rhs_;

  public:
    explicit ModOverflowCheck(LModI *ins, Register rhs)
      : ins_(ins), rhs_(rhs)
    { }

    virtual bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitModOverflowCheck(this);
    }
    Label *done() {
        return &done_;
    }
    LModI *ins() const {
        return ins_;
    }
    Register rhs() const {
        return rhs_;
    }
};

bool
CodeGeneratorX86Shared::visitModOverflowCheck(ModOverflowCheck *ool)
{
    masm.cmpl(ool->rhs(), Imm32(-1));
    if (ool->ins()->mir()->isTruncated()) {
        masm.j(Assembler::NotEqual, ool->rejoin());
        masm.mov(ImmWord(0), edx);
        masm.jmp(ool->done());
    } else {
        if (!bailoutIf(Assembler::Equal, ool->ins()->snapshot()))
            return false;
       masm.jmp(ool->rejoin());
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitModI(LModI *ins)
{
    Register remainder = ToRegister(ins->remainder());
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());

    // Required to use idiv.
    JS_ASSERT(lhs == eax);
    JS_ASSERT(remainder == edx);
    JS_ASSERT(ToRegister(ins->getTemp(0)) == eax);

    Label done;
    ReturnZero *ool = nullptr;
    ModOverflowCheck *overflow = nullptr;

    // Prevent divide by zero.
    if (ins->mir()->canBeDivideByZero()) {
        masm.testl(rhs, rhs);
        if (ins->mir()->isTruncated()) {
            if (!ool)
                ool = new(alloc()) ReturnZero(edx);
            masm.j(Assembler::Zero, ool->entry());
        } else {
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    Label negative;

    // Switch based on sign of the lhs.
    if (ins->mir()->canBeNegativeDividend())
        masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);

    // If lhs >= 0 then remainder = lhs % rhs. The remainder must be positive.
    {
        // Check if rhs is a power-of-two.
        if (ins->mir()->canBePowerOfTwoDivisor()) {
            JS_ASSERT(rhs != remainder);

            // Rhs y is a power-of-two if (y & (y-1)) == 0. Note that if
            // y is any negative number other than INT32_MIN, both y and
            // y-1 will have the sign bit set so these are never optimized
            // as powers-of-two. If y is INT32_MIN, y-1 will be INT32_MAX
            // and because lhs >= 0 at this point, lhs & INT32_MAX returns
            // the correct value.
            Label notPowerOfTwo;
            masm.mov(rhs, remainder);
            masm.subl(Imm32(1), remainder);
            masm.branchTest32(Assembler::NonZero, remainder, rhs, &notPowerOfTwo);
            {
                masm.andl(lhs, remainder);
                masm.jmp(&done);
            }
            masm.bind(&notPowerOfTwo);
        }

        // Since lhs >= 0, the sign-extension will be 0
        masm.mov(ImmWord(0), edx);
        masm.idiv(rhs);
    }

    // Otherwise, we have to beware of two special cases:
    if (ins->mir()->canBeNegativeDividend()) {
        masm.jump(&done);

        masm.bind(&negative);

        // Prevent an integer overflow exception from -2147483648 % -1
        Label notmin;
        masm.cmpl(lhs, Imm32(INT32_MIN));
        overflow = new(alloc()) ModOverflowCheck(ins, rhs);
        masm.j(Assembler::Equal, overflow->entry());
        masm.bind(overflow->rejoin());
        masm.cdq();
        masm.idiv(rhs);

        if (!ins->mir()->isTruncated()) {
            // A remainder of 0 means that the rval must be -0, which is a double.
            masm.testl(remainder, remainder);
            if (!bailoutIf(Assembler::Zero, ins->snapshot()))
                return false;
        }
    }

    masm.bind(&done);

    if (overflow) {
        if (!addOutOfLineCode(overflow))
            return false;
        masm.bind(overflow->done());
    }

    if (ool) {
        if (!addOutOfLineCode(ool))
            return false;
        masm.bind(ool->rejoin());
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitBitNotI(LBitNotI *ins)
{
    const LAllocation *input = ins->getOperand(0);
    JS_ASSERT(!input->isConstant());

    masm.notl(ToOperand(input));
    return true;
}

bool
CodeGeneratorX86Shared::visitBitOpI(LBitOpI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    switch (ins->bitop()) {
        case JSOP_BITOR:
            if (rhs->isConstant())
                masm.orl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.orl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITXOR:
            if (rhs->isConstant())
                masm.xorl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.xorl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITAND:
            if (rhs->isConstant())
                masm.andl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.andl(ToOperand(rhs), ToRegister(lhs));
            break;
        default:
            MOZ_ASSUME_UNREACHABLE("unexpected binary opcode");
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitShiftI(LShiftI *ins)
{
    Register lhs = ToRegister(ins->lhs());
    const LAllocation *rhs = ins->rhs();

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        switch (ins->bitop()) {
          case JSOP_LSH:
            if (shift)
                masm.shll(Imm32(shift), lhs);
            break;
          case JSOP_RSH:
            if (shift)
                masm.sarl(Imm32(shift), lhs);
            break;
          case JSOP_URSH:
            if (shift) {
                masm.shrl(Imm32(shift), lhs);
            } else if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                masm.testl(lhs, lhs);
                if (!bailoutIf(Assembler::Signed, ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    } else {
        JS_ASSERT(ToRegister(rhs) == ecx);
        switch (ins->bitop()) {
          case JSOP_LSH:
            masm.shll_cl(lhs);
            break;
          case JSOP_RSH:
            masm.sarl_cl(lhs);
            break;
          case JSOP_URSH:
            masm.shrl_cl(lhs);
            if (ins->mir()->toUrsh()->fallible()) {
                // x >>> 0 can overflow.
                masm.testl(lhs, lhs);
                if (!bailoutIf(Assembler::Signed, ins->snapshot()))
                    return false;
            }
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("Unexpected shift op");
        }
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitUrshD(LUrshD *ins)
{
    Register lhs = ToRegister(ins->lhs());
    JS_ASSERT(ToRegister(ins->temp()) == lhs);

    const LAllocation *rhs = ins->rhs();
    FloatRegister out = ToFloatRegister(ins->output());

    if (rhs->isConstant()) {
        int32_t shift = ToInt32(rhs) & 0x1F;
        if (shift)
            masm.shrl(Imm32(shift), lhs);
    } else {
        JS_ASSERT(ToRegister(rhs) == ecx);
        masm.shrl_cl(lhs);
    }

    masm.convertUInt32ToDouble(lhs, out);
    return true;
}

MoveOperand
CodeGeneratorX86Shared::toMoveOperand(const LAllocation *a) const
{
    if (a->isGeneralReg())
        return MoveOperand(ToRegister(a));
    if (a->isFloatReg())
        return MoveOperand(ToFloatRegister(a));
    return MoveOperand(StackPointer, ToStackOffset(a));
}

class OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorX86Shared>
{
    MTableSwitch *mir_;
    CodeLabel jumpLabel_;

    bool accept(CodeGeneratorX86Shared *codegen) {
        return codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(MTableSwitch *mir)
      : mir_(mir)
    {}

    MTableSwitch *mir() const {
        return mir_;
    }

    CodeLabel *jumpLabel() {
        return &jumpLabel_;
    }
};

bool
CodeGeneratorX86Shared::visitOutOfLineTableSwitch(OutOfLineTableSwitch *ool)
{
    MTableSwitch *mir = ool->mir();

    masm.align(sizeof(void*));
    masm.bind(ool->jumpLabel()->src());
    if (!masm.addCodeLabel(*ool->jumpLabel()))
        return false;

    for (size_t i = 0; i < mir->numCases(); i++) {
        LBlock *caseblock = mir->getCase(i)->lir();
        Label *caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl;
        masm.writeCodePointer(cl.dest());
        cl.src()->bind(caseoffset);
        if (!masm.addCodeLabel(cl))
            return false;
    }

    return true;
}

bool
CodeGeneratorX86Shared::emitTableSwitchDispatch(MTableSwitch *mir, const Register &index,
                                                const Register &base)
{
    Label *defaultcase = mir->getDefault()->lir()->label();

    // Lower value with low value
    if (mir->low() != 0)
        masm.subl(Imm32(mir->low()), index);

    // Jump to default case if input is out of range
    int32_t cases = mir->numCases();
    masm.cmpl(index, Imm32(cases));
    masm.j(AssemblerX86Shared::AboveOrEqual, defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first
    // generate the case entries (we don't yet know their offsets in the
    // instruction stream).
    OutOfLineTableSwitch *ool = new(alloc()) OutOfLineTableSwitch(mir);
    if (!addOutOfLineCode(ool))
        return false;

    // Compute the position where a pointer to the right case stands.
    masm.mov(ool->jumpLabel()->dest(), base);
    Operand pointer = Operand(base, index, ScalePointer);

    // Jump to the right case
    masm.jmp(pointer);

    return true;
}

bool
CodeGeneratorX86Shared::visitMathD(LMathD *math)
{
    FloatRegister lhs = ToFloatRegister(math->lhs());
    Operand rhs = ToOperand(math->rhs());

    JS_ASSERT(ToFloatRegister(math->output()) == lhs);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.addsd(rhs, lhs);
        break;
      case JSOP_SUB:
        masm.subsd(rhs, lhs);
        break;
      case JSOP_MUL:
        masm.mulsd(rhs, lhs);
        break;
      case JSOP_DIV:
        masm.divsd(rhs, lhs);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitMathF(LMathF *math)
{
    FloatRegister lhs = ToFloatRegister(math->lhs());
    Operand rhs = ToOperand(math->rhs());

    JS_ASSERT(ToFloatRegister(math->output()) == lhs);

    switch (math->jsop()) {
      case JSOP_ADD:
        masm.addss(rhs, lhs);
        break;
      case JSOP_SUB:
        masm.subss(rhs, lhs);
        break;
      case JSOP_MUL:
        masm.mulss(rhs, lhs);
        break;
      case JSOP_DIV:
        masm.divss(rhs, lhs);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("unexpected opcode");
        return false;
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitFloor(LFloor *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    if (AssemblerX86Shared::HasSSE41()) {
        // Bail on negative-zero.
        Assembler::Condition bailCond = masm.testNegativeZero(input, output);
        if (!bailoutIf(bailCond, lir->snapshot()))
            return false;

        // Round toward -Infinity.
        masm.roundsd(input, scratch, JSC::X86Assembler::RoundDown);

        masm.cvttsd2si(scratch, output);
        masm.cmp32(output, Imm32(INT_MIN));
        if (!bailoutIf(Assembler::Equal, lir->snapshot()))
            return false;
    } else {
        Label negative, end;

        // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
        masm.xorpd(scratch, scratch);
        masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &negative);

        // Bail on negative-zero.
        Assembler::Condition bailCond = masm.testNegativeZero(input, output);
        if (!bailoutIf(bailCond, lir->snapshot()))
            return false;

        // Input is non-negative, so truncation correctly rounds.
        masm.cvttsd2si(input, output);
        masm.cmp32(output, Imm32(INT_MIN));
        if (!bailoutIf(Assembler::Equal, lir->snapshot()))
            return false;

        masm.jump(&end);

        // Input is negative, but isn't -0.
        // Negative values go on a comparatively expensive path, since no
        // native rounding mode matches JS semantics. Still better than callVM.
        masm.bind(&negative);
        {
            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            masm.cvttsd2si(input, output);
            masm.cmp32(output, Imm32(INT_MIN));
            if (!bailoutIf(Assembler::Equal, lir->snapshot()))
                return false;

            // Test whether the input double was integer-valued.
            masm.convertInt32ToDouble(output, scratch);
            masm.branchDouble(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }

        masm.bind(&end);
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitFloorF(LFloorF *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    if (AssemblerX86Shared::HasSSE41()) {
        // Bail on negative-zero.
        Assembler::Condition bailCond = masm.testNegativeZeroFloat32(input, output);
        if (!bailoutIf(bailCond, lir->snapshot()))
            return false;

        // Round toward -Infinity.
        masm.roundss(input, scratch, JSC::X86Assembler::RoundDown);

        masm.cvttss2si(scratch, output);
        masm.cmp32(output, Imm32(INT_MIN));
        if (!bailoutIf(Assembler::Equal, lir->snapshot()))
            return false;
    } else {
        Label negative, end;

        // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
        masm.xorps(scratch, scratch);
        masm.branchFloat(Assembler::DoubleLessThan, input, scratch, &negative);

        // Bail on negative-zero.
        Assembler::Condition bailCond = masm.testNegativeZeroFloat32(input, output);
        if (!bailoutIf(bailCond, lir->snapshot()))
            return false;

        // Input is non-negative, so truncation correctly rounds.
        masm.cvttss2si(input, output);
        masm.cmp32(output, Imm32(INT_MIN));
        if (!bailoutIf(Assembler::Equal, lir->snapshot()))
            return false;

        masm.jump(&end);

        // Input is negative, but isn't -0.
        // Negative values go on a comparatively expensive path, since no
        // native rounding mode matches JS semantics. Still better than callVM.
        masm.bind(&negative);
        {
            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            masm.cvttss2si(input, output);
            masm.cmp32(output, Imm32(INT_MIN));
            if (!bailoutIf(Assembler::Equal, lir->snapshot()))
                return false;

            // Test whether the input double was integer-valued.
            masm.convertInt32ToFloat32(output, scratch);
            masm.branchFloat(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }

        masm.bind(&end);
    }
    return true;
}

bool
CodeGeneratorX86Shared::visitRound(LRound *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister temp = ToFloatRegister(lir->temp());
    FloatRegister scratch = ScratchFloatReg;
    Register output = ToRegister(lir->output());

    Label negative, end;

    // Load 0.5 in the temp register.
    masm.loadConstantDouble(0.5, temp);

    // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
    masm.xorpd(scratch, scratch);
    masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &negative);

    // Bail on negative-zero.
    Assembler::Condition bailCond = masm.testNegativeZero(input, output);
    if (!bailoutIf(bailCond, lir->snapshot()))
        return false;

    // Input is non-negative. Add 0.5 and truncate, rounding down. Note that we
    // have to add the input to the temp register (which contains 0.5) because
    // we're not allowed to modify the input register.
    masm.addsd(input, temp);

    masm.cvttsd2si(temp, output);
    masm.cmp32(output, Imm32(INT_MIN));
    if (!bailoutIf(Assembler::Equal, lir->snapshot()))
        return false;

    masm.jump(&end);


    // Input is negative, but isn't -0.
    masm.bind(&negative);

    if (AssemblerX86Shared::HasSSE41()) {
        // Add 0.5 and round toward -Infinity. The result is stored in the temp
        // register (currently contains 0.5).
        masm.addsd(input, temp);
        masm.roundsd(temp, scratch, JSC::X86Assembler::RoundDown);

        // Truncate.
        masm.cvttsd2si(scratch, output);
        masm.cmp32(output, Imm32(INT_MIN));
        if (!bailoutIf(Assembler::Equal, lir->snapshot()))
            return false;

        // If the result is positive zero, then the actual result is -0. Bail.
        // Otherwise, the truncation will have produced the correct negative integer.
        masm.testl(output, output);
        if (!bailoutIf(Assembler::Zero, lir->snapshot()))
            return false;

    } else {
        masm.addsd(input, temp);

        // Round toward -Infinity without the benefit of ROUNDSD.
        {
            // If input + 0.5 >= 0, input is a negative number >= -0.5 and the result is -0.
            masm.compareDouble(Assembler::DoubleGreaterThanOrEqual, temp, scratch);
            if (!bailoutIf(Assembler::DoubleGreaterThanOrEqual, lir->snapshot()))
                return false;

            // Truncate and round toward zero.
            // This is off-by-one for everything but integer-valued inputs.
            masm.cvttsd2si(temp, output);
            masm.cmp32(output, Imm32(INT_MIN));
            if (!bailoutIf(Assembler::Equal, lir->snapshot()))
                return false;

            // Test whether the truncated double was integer-valued.
            masm.convertInt32ToDouble(output, scratch);
            masm.branchDouble(Assembler::DoubleEqualOrUnordered, temp, scratch, &end);

            // Input is not integer-valued, so we rounded off-by-one in the
            // wrong direction. Correct by subtraction.
            masm.subl(Imm32(1), output);
            // Cannot overflow: output was already checked against INT_MIN.
        }
    }

    masm.bind(&end);
    return true;
}

bool
CodeGeneratorX86Shared::visitGuardShape(LGuardShape *guard)
{
    Register obj = ToRegister(guard->input());
    masm.cmpPtr(Operand(obj, JSObject::offsetOfShape()), ImmGCPtr(guard->mir()->shape()));

    return bailoutIf(Assembler::NotEqual, guard->snapshot());
}

bool
CodeGeneratorX86Shared::visitGuardObjectType(LGuardObjectType *guard)
{
    Register obj = ToRegister(guard->input());
    masm.cmpPtr(Operand(obj, JSObject::offsetOfType()), ImmGCPtr(guard->mir()->typeObject()));

    Assembler::Condition cond =
        guard->mir()->bailOnEquality() ? Assembler::Equal : Assembler::NotEqual;
    return bailoutIf(cond, guard->snapshot());
}

bool
CodeGeneratorX86Shared::visitGuardClass(LGuardClass *guard)
{
    Register obj = ToRegister(guard->input());
    Register tmp = ToRegister(guard->tempInt());

    masm.loadPtr(Address(obj, JSObject::offsetOfType()), tmp);
    masm.cmpPtr(Operand(tmp, types::TypeObject::offsetOfClasp()), ImmPtr(guard->mir()->getClass()));
    if (!bailoutIf(Assembler::NotEqual, guard->snapshot()))
        return false;
    return true;
}

bool
CodeGeneratorX86Shared::visitEffectiveAddress(LEffectiveAddress *ins)
{
    const MEffectiveAddress *mir = ins->mir();
    Register base = ToRegister(ins->base());
    Register index = ToRegister(ins->index());
    Register output = ToRegister(ins->output());
    masm.leal(Operand(base, index, mir->scale(), mir->displacement()), output);
    return true;
}

Operand
CodeGeneratorX86Shared::createArrayElementOperand(Register elements, const LAllocation *index)
{
    if (index->isConstant())
        return Operand(elements, ToInt32(index) * sizeof(js::Value));

    return Operand(elements, ToRegister(index), TimesEight);
}
bool
CodeGeneratorX86Shared::generateInvalidateEpilogue()
{
    // Ensure that there is enough space in the buffer for the OsiPoint
    // patching to occur. Otherwise, we could overwrite the invalidation
    // epilogue.
    for (size_t i = 0; i < sizeof(void *); i+= Assembler::nopSize())
        masm.nop();

    masm.bind(&invalidate_);

    // Push the Ion script onto the stack (when we determine what that pointer is).
    invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));
    IonCode *thunk = gen->jitRuntime()->getInvalidationThunk();

    masm.call(thunk);

    // We should never reach this point in JIT code -- the invalidation thunk should
    // pop the invalidated JS frame and return directly to its caller.
    masm.assumeUnreachable("Should have returned directly to its caller instead of here.");
    return true;
}

bool
CodeGeneratorX86Shared::visitNegI(LNegI *ins)
{
    Register input = ToRegister(ins->input());
    JS_ASSERT(input == ToRegister(ins->output()));

    masm.neg32(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitNegD(LNegD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));

    masm.negateDouble(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitNegF(LNegF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(input == ToFloatRegister(ins->output()));

    masm.negateFloat(input);
    return true;
}

bool
CodeGeneratorX86Shared::visitSIMDNullaryFunction(LSIMDNullaryFunction *lir)
{
    JS_ASSERT(IsX4Type(lir->mir()->type()));

    FloatRegister output = ToFloatRegister(lir->output());
    switch (lir->mir()->id()) {
      case MSIMDNullaryFunction::Float32x4Zero:
      case MSIMDNullaryFunction::Int32x4Zero:
        masm.xorps(output, output);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD nullary operation.");
        break;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitSIMDUnaryFunction(LSIMDUnaryFunction *lir)
{
    switch (lir->mir()->id()) {
      case MSIMDUnaryFunction::Float32x4Abs:
      case MSIMDUnaryFunction::Float32x4BitsToInt32x4:
      case MSIMDUnaryFunction::Float32x4Neg:
      case MSIMDUnaryFunction::Float32x4Reciprocal:
      case MSIMDUnaryFunction::Float32x4ReciprocalSqrt:
      case MSIMDUnaryFunction::Float32x4Splat:
      case MSIMDUnaryFunction::Float32x4Sqrt:
      case MSIMDUnaryFunction::Float32x4ToInt32x4:
      case MSIMDUnaryFunction::Int32x4BitsToFloat32x4:
      case MSIMDUnaryFunction::Int32x4Neg:
      case MSIMDUnaryFunction::Int32x4Not:
      case MSIMDUnaryFunction::Int32x4ToFloat32x4: {
        FloatRegister output = ToFloatRegister(lir->output());
        FloatRegister input = ToFloatRegister(lir->getOperand(0));
        JS_ASSERT(input == output);

        if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4Abs)
            masm.absps(input);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4BitsToInt32x4)
            return true;
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4Neg)
            masm.negps(input);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4Reciprocal)
            masm.rcpps(input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4ReciprocalSqrt)
            masm.rsqrtps(input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4Splat)
            masm.shufps(0x0, input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4Sqrt)
            masm.sqrtps(input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4ToInt32x4)
            masm.cvtps2dq(input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4BitsToFloat32x4)
            return true;
        else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4Neg)
            masm.pnegd(input);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4Not)
            masm.pnotd(input);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4ToFloat32x4)
            masm.cvtdq2ps(input, output);
        else
            MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");

        return true;
      }

      case MSIMDUnaryFunction::Float32x4GetSignMask:
      case MSIMDUnaryFunction::Int32x4GetSignMask: {
        Register output = ToRegister(lir->output());
        FloatRegister input = ToFloatRegister(lir->getOperand(0));
        masm.movmskps(input, output);
        return true;
      }

      // TODO(haitao): Do we care about the high 96 bits?
      case MSIMDUnaryFunction::Float32x4GetX:
      case MSIMDUnaryFunction::Float32x4GetY:
      case MSIMDUnaryFunction::Float32x4GetZ:
      case MSIMDUnaryFunction::Float32x4GetW: {
        FloatRegister output = ToFloatRegister(lir->output());
        FloatRegister input = ToFloatRegister(lir->getOperand(0));

        if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4GetX) {
            if (output != input) masm.movaps(input, output);
        }
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4GetY)
            masm.pshufd(1, input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4GetZ)
            masm.pshufd(2, input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Float32x4GetW)
            masm.pshufd(3, input, output);
        else
            MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");

        return true;
      }

      // TODO(haitao): Figure out how to return true and false?
      case MSIMDUnaryFunction::Int32x4GetFlagX:
      case MSIMDUnaryFunction::Int32x4GetFlagY:
      case MSIMDUnaryFunction::Int32x4GetFlagZ:
      case MSIMDUnaryFunction::Int32x4GetFlagW: {
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");
        return true;
      }

      case MSIMDUnaryFunction::Int32x4GetX:
      case MSIMDUnaryFunction::Int32x4GetY:
      case MSIMDUnaryFunction::Int32x4GetZ:
      case MSIMDUnaryFunction::Int32x4GetW: {
        Register output = ToRegister(lir->output());
        FloatRegister input = ToFloatRegister(lir->getOperand(0));

        if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4GetX)
            masm.movd(input, output);
        else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4GetY) {
            masm.pshufd(1, input, ScratchFloatReg);
            masm.movd(ScratchFloatReg, output);
        } else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4GetZ) {
            masm.pshufd(2, input, ScratchFloatReg);
            masm.movd(ScratchFloatReg, output);
        } else if (lir->mir()->id() == MSIMDUnaryFunction::Int32x4GetW) {
            masm.pshufd(3, input, ScratchFloatReg);
            masm.movd(ScratchFloatReg, output);
        } else
            MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");

        return true;
      }

      case MSIMDUnaryFunction::Int32x4Splat: {
        FloatRegister output = ToFloatRegister(lir->output());
        Register input = ToRegister(lir->getOperand(0));
        masm.movd(input, output);
        masm.shufps(0x0, output, output);
        return true;
      }

      default:
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");
        break;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitSIMDBinaryFunction(LSIMDBinaryFunction *lir)
{
    JS_ASSERT(IsX4Type(lir->mir()->type()));

    FloatRegister output = ToFloatRegister(lir->output());
    switch (lir->mir()->id()) {
      case MSIMDBinaryFunction::Float32x4Add:
      case MSIMDBinaryFunction::Float32x4Div:
      case MSIMDBinaryFunction::Float32x4Equal:
      case MSIMDBinaryFunction::Float32x4GreaterThan:
      case MSIMDBinaryFunction::Float32x4GreaterThanOrEqual:
      case MSIMDBinaryFunction::Float32x4LessThan:
      case MSIMDBinaryFunction::Float32x4LessThanOrEqual:
      case MSIMDBinaryFunction::Float32x4NotEqual:
      case MSIMDBinaryFunction::Float32x4Max:
      case MSIMDBinaryFunction::Float32x4Min:
      case MSIMDBinaryFunction::Float32x4Mul:
      case MSIMDBinaryFunction::Float32x4Scale:
      case MSIMDBinaryFunction::Float32x4Sub:
      case MSIMDBinaryFunction::Int32x4Add:
      case MSIMDBinaryFunction::Int32x4And:
      case MSIMDBinaryFunction::Int32x4Mul:
      case MSIMDBinaryFunction::Int32x4Or:
      case MSIMDBinaryFunction::Int32x4Sub:
      case MSIMDBinaryFunction::Int32x4Xor: {
        FloatRegister lhs = ToFloatRegister(lir->getOperand(0));
        FloatRegister rhs = ToFloatRegister(lir->getOperand(1));
        JS_ASSERT(lhs == output);

        if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Add)
            masm.addps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Div)
            masm.divps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Equal)
            masm.cmpps(0x0, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4GreaterThan)
            masm.cmpps(0x6, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4GreaterThanOrEqual)
            masm.cmpps(0x5, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4LessThan)
            masm.cmpps(0x1, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4LessThanOrEqual)
            masm.cmpps(0x2, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4NotEqual)
            masm.cmpps(0x4, rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Max)
            masm.maxps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Min)
            masm.minps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Mul)
            masm.mulps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Scale) {
            masm.movaps(rhs, ScratchFloatReg);
            masm.shufps(0x0, ScratchFloatReg, ScratchFloatReg);
            masm.mulps(ScratchFloatReg, lhs);
        } else if (lir->mir()->id() == MSIMDBinaryFunction::Float32x4Sub)
            masm.subps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4Add)
            masm.paddd(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4And)
            masm.andps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4Mul) {
            MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");
        } else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4Or)
            masm.orps(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4Sub)
            masm.psubd(rhs, lhs);
        else if (lir->mir()->id() == MSIMDBinaryFunction::Int32x4Xor)
            masm.xorps(rhs, lhs);
        else
            MOZ_ASSUME_UNREACHABLE("Unsupported SIMD unary operation.");

        return true;
      }
      case MSIMDBinaryFunction::Float32x4WithX:
      case MSIMDBinaryFunction::Float32x4WithY:
      case MSIMDBinaryFunction::Float32x4WithZ:
      case MSIMDBinaryFunction::Float32x4WithW:
      case MSIMDBinaryFunction::Int32x4WithFlagX:
      case MSIMDBinaryFunction::Int32x4WithFlagY:
      case MSIMDBinaryFunction::Int32x4WithFlagZ:
      case MSIMDBinaryFunction::Int32x4WithFlagW:
      case MSIMDBinaryFunction::Int32x4WithX:
      case MSIMDBinaryFunction::Int32x4WithY:
      case MSIMDBinaryFunction::Int32x4WithZ:
      case MSIMDBinaryFunction::Int32x4WithW: {
        return true;
      }
      case MSIMDBinaryFunction::Float32x4Shuffle:
      case MSIMDBinaryFunction::Int32x4Shuffle: {
        return true;
      }
      default:
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD binary operation.");
        break;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitSIMDTernaryFunction(LSIMDTernaryFunction *lir)
{
    JS_ASSERT(IsX4Type(lir->mir()->type()));

    switch (lir->mir()->id()) {
      case MSIMDTernaryFunction::Float32x4Clamp:
        return true;
      case MSIMDTernaryFunction::Float32x4ShuffleMix:
      case MSIMDTernaryFunction::Int32x4ShuffleMix:
        return true;
      case MSIMDTernaryFunction::Int32x4Select:
        return true;
      default:
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD ternary operation.");
        break;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitSIMDQuarternaryFunction(LSIMDQuarternaryFunction *lir)
{
    JS_ASSERT(IsX4Type(lir->mir()->type()));

    FloatRegister output = ToFloatRegister(lir->output());
    switch (lir->mir()->id()) {
      case MSIMDQuarternaryFunction::Float32x4Construct: {
        FloatRegister first  = ToFloatRegister(lir->getOperand(0));
        FloatRegister second = ToFloatRegister(lir->getOperand(1));
        FloatRegister third  = ToFloatRegister(lir->getOperand(2));
        FloatRegister fourth = ToFloatRegister(lir->getOperand(3));

        masm.reserveStack(4 * sizeof(float));
        masm.movss(first,  Address(StackPointer, 0));
        masm.movss(second, Address(StackPointer, 1 * sizeof(float)));
        masm.movss(third,  Address(StackPointer, 2 * sizeof(float)));
        masm.movss(fourth, Address(StackPointer, 3 * sizeof(float)));
        masm.movups(Address(StackPointer, 0), output);
        masm.freeStack(4 * sizeof(float));

        return true;
      }
      case MSIMDQuarternaryFunction::Int32x4Construct: {
        Register first  = ToRegister(lir->getOperand(0));
        Register second = ToRegister(lir->getOperand(1));
        Register third  = ToRegister(lir->getOperand(2));
        Register fourth = ToRegister(lir->getOperand(3));

        masm.reserveStack(4 * sizeof(int32_t));
        masm.move32(first,  Operand(StackPointer, 0));
        masm.move32(second, Operand(StackPointer, 1 * sizeof(int32_t)));
        masm.move32(third,  Operand(StackPointer, 2 * sizeof(int32_t)));
        masm.move32(fourth, Operand(StackPointer, 3 * sizeof(int32_t)));
        masm.movups(Address(StackPointer, 0), output);
        masm.freeStack(4 * sizeof(int32_t));

        return true;
      }

      case MSIMDQuarternaryFunction::Int32x4Bool:
        return true;
      default:
        MOZ_ASSUME_UNREACHABLE("Unsupported SIMD quarternary operation.");
        break;
    }

    return true;
}

bool
CodeGeneratorX86Shared::visitToX4(LToX4 *lir)
{
    FloatRegister resultReg = ToFloatRegister(lir->output());

    if (lir->mir()->getOperand(0)->type() != MIRType_Object) {
        // The box instruction for MIRType_Undefined.
        masm.xorps(resultReg, resultReg);
        return true;
    }

    // TODO(haitao): check the typedobject and bailout if type does not match.
    Register inputReg = ToRegister(lir->getOperand(0));
    Register tempReg = ToRegister(lir->getTemp(0));

    masm.loadPtr(Address(inputReg, TypedObject::dataOffset()), tempReg);
    masm.loadSIMD128(Address(tempReg, 0), resultReg);

    return true;
}

typedef TypedObject *(*CreateZeroedSIMDWrapperFn)(JSContext *);
static const VMFunction CreateZeroedFloat32x4WrapperInfo =
    FunctionInfo<CreateZeroedSIMDWrapperFn>(
        CreateZeroedSIMDWrapper<Float32x4>);
static const VMFunction CreateZeroedInt32x4WrapperInfo =
    FunctionInfo<CreateZeroedSIMDWrapperFn>(
        CreateZeroedSIMDWrapper<Int32x4>);

bool
CodeGeneratorX86Shared::visitToX4TypedObject(LToX4TypedObject *lir)
{
    // Not yet made safe for par exec:
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);

    Register typedObjectReg = ToRegister(lir->output());
    FloatRegister inputReg = ToFloatRegister(lir->getOperand(0));
    Register tempReg = ToRegister(lir->getTemp(0));

    saveLive(lir);

    switch (lir->mir()->input()->type()) {
      case MIRType_float32x4:
        if (!callVM(CreateZeroedFloat32x4WrapperInfo, lir))
            return false;
        break;

      case MIRType_int32x4:
        if (!callVM(CreateZeroedInt32x4WrapperInfo, lir))
            return false;
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Unknown SIMD MIR type");
        break;
    }

    if (ReturnReg != typedObjectReg)
        masm.movePtr(ReturnReg, typedObjectReg);

    restoreLive(lir);

    masm.loadPtr(Address(typedObjectReg, TypedObject::dataOffset()), tempReg);
    masm.storeSIMD128(inputReg, Address(tempReg, 0));

    return true;
}

} // namespace jit
} // namespace js
