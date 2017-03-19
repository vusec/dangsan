/*
 * StackTracker.cpp
 *
 *  Created on: Oct 22, 2015
 *      Author: haller
 */

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetLowering.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <string>
#include <list>
#include <set>
#include <vector>

#include <Utils.h>

using namespace llvm;

struct ByvalHandler : public FunctionPass {
    static char ID;
    bool initialized;
    Module *M;
    const DataLayout *DL;
    SafetyManager *SM;
    Type* VoidTy;
    IntegerType *Int1Ty;
    IntegerType *Int8Ty;
    IntegerType *Int32Ty;
    IntegerType *IntPtrTy;
    PointerType *PtrVoidTy;

    //declare void @llvm.memcpy.p0i8.p0i8.i64(i8 *, i8 *, i64, i32, i1)
    Function *MemcpyFunc;

    ByvalHandler() : FunctionPass(ID) { initialized = false; }

    virtual bool runOnFunction(Function &F) {
        if (!initialized)
            doInitialization(F.getParent());

        SM = new SafetyManager(DL, &getAnalysis<ScalarEvolutionWrapperPass>().getSE());
                      
        for (auto &a : F.args()) {
            Argument *Arg = dyn_cast<Argument>(&a);
            unsigned long Size = SM->GetByvalArgumentSize(Arg);
            if (Arg->hasByValAttr() && !SM->IsSafeStackAlloca(Arg, Size)) {
                IRBuilder<> B(F.getEntryBlock().getFirstInsertionPt());
                Value *NewAlloca = B.CreateAlloca(Arg->getType()->getPointerElementType());
                Arg->replaceAllUsesWith(NewAlloca);
                Value *Src = B.CreatePointerCast(Arg, PtrVoidTy);
                Value *Dst = B.CreatePointerCast(NewAlloca, PtrVoidTy);
                std::vector<Value *> callParams;
                callParams.push_back(Dst);
                callParams.push_back(Src);
                callParams.push_back(ConstantInt::get(IntPtrTy, Size));
                callParams.push_back(ConstantInt::get(Int32Ty, 1));
                callParams.push_back(ConstantInt::get(Int1Ty, 0));
                B.CreateCall(MemcpyFunc, callParams);
            }
        }

        delete SM;

        return false;
    }

    bool doInitialization(Module *Mod) {
        M = Mod;
        
        DL = &(M->getDataLayout());
        if (!DL)
            report_fatal_error("Data layout required");
    
	    // Type definitions
        VoidTy = Type::getVoidTy(M->getContext());
        Int1Ty = Type::getInt1Ty(M->getContext());
        Int8Ty = Type::getInt8Ty(M->getContext());
        Int32Ty = Type::getInt32Ty(M->getContext());
        IntPtrTy = DL->getIntPtrType(M->getContext(), 0);
        PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

        //declare void @llvm.memcpy.p0i8.p0i8.i64(i8 *, i8 *, i64, i32, i1)
        Type *Tys[] = { PtrVoidTy, PtrVoidTy, IntPtrTy };
        MemcpyFunc = Intrinsic::getDeclaration(M, Intrinsic::memcpy, Tys);

        initialized = true;
        
        return false;
    }
    
    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.addRequired<ScalarEvolutionWrapperPass>();
    }

};

char ByvalHandler::ID = 0;
static RegisterPass<ByvalHandler> X("byvalhandler", "Byval Handler Pass", true, false);




