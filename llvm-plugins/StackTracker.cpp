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
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Analysis/LoopInfo.h>

#include <string>
#include <list>
#include <set>
#include <vector>

#include <Utils.h>
#include <metadata.h>

using namespace llvm;

cl::opt<bool> MergedStack ("mergedstack", cl::desc("Merge the static stack allocas before generating metadata"), cl::init(false));
extern cl::opt<bool> UseLargeStack;
extern cl::opt<unsigned long> LargeStackThreshold;

template<class Cont>
class const_reverse_wrapper {
  const Cont& container;

public:
  const_reverse_wrapper(const Cont& cont) : container(cont){ }
  decltype(container.rbegin()) begin() const { return container.rbegin(); }
  decltype(container.rend()) end() const { return container.rend(); }
};

template<class Cont>
class reverse_wrapper {
  Cont& container;

public:
  reverse_wrapper(Cont& cont) : container(cont){ }
  decltype(container.rbegin()) begin() { return container.rbegin(); }
  decltype(container.rend()) end() { return container.rend(); }
};

template<class Cont>
const_reverse_wrapper<Cont> reverse(const Cont& cont) {
  return const_reverse_wrapper<Cont>(cont);
}

template<class Cont>
reverse_wrapper<Cont> reverse(Cont& cont) {
  return reverse_wrapper<Cont>(cont);
}

struct StackTracker : public FunctionPass {
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
    IntegerType *IntMetaTy;
    PointerType *PtrVoidTy;

    StructType *MetaDataTy;

    //declare i64 @metaset_alignment(i64, i64, iM, i64)
    Constant *MetasetFunc;
    //declare i64 @metabaseget(i64)
    Constant *MetabasegetFunc;
    //declare i64 @metaset_fast(i64, i64, iM, i64, i64, i64)
    Constant *MetasetFastFunc;
    Constant *DangSanInitFunc;
    Constant *DangSanFreeFunc;

    StackTracker() : FunctionPass(ID) { initialized = false; }

    void MergeAllocas(std::vector<AllocaInst*> &oldStaticAllocas, std::set<AllocaInst*> &staticAllocas, IRBuilder<> &AllocaBuilder) {
        // Compute maximum alignment among static objects.
        unsigned long maxAlignment = 0;
        for (auto &AI : oldStaticAllocas) {
            Type *Ty = AI->getAllocatedType();
            unsigned align =
                std::max((unsigned)DL->getPrefTypeAlignment(Ty), AI->getAlignment());
            if (align > maxAlignment)
                maxAlignment = align;
        }
        // Compute the total size of static objects (including desired alignment).
        // Traverse in reverse alloca order.
        unsigned long staticOffset = 0; // Current stack top.
        std::vector<unsigned long> offsetVector;
	    const std::vector<llvm::AllocaInst*> &oldStaticAllocasConst = oldStaticAllocas;
        for (auto &AI : oldStaticAllocasConst) {
            Type *Ty = AI->getAllocatedType();
            // Ensure the object is properly aligned.
            unsigned align =
                std::max((unsigned)DL->getPrefTypeAlignment(Ty), AI->getAlignment());
            // Add alignment.
            staticOffset = RoundUpToAlignment(staticOffset, align);
            // Save start address
            offsetVector.push_back(staticOffset);
            // Compute and add size
            auto arraySize = cast<ConstantInt>(AI->getArraySize());
            uint64_t size = DL->getTypeAllocSize(Ty) * arraySize->getZExtValue();
            if (size == 0)
                size = 1; // Don't create zero-sized stack objects.
            staticOffset += size;
        }
        AllocaInst *newAI = AllocaBuilder.CreateAlloca(Int8Ty, ConstantInt::get(IntPtrTy, staticOffset));
        newAI->setAlignment(maxAlignment);
        // Replace static objects with pointer into merged object.
        // Traverse in reverse alloca order.
        int offsetPos = 0;
        for (auto &AI : oldStaticAllocasConst) {
            Value *newObject = AllocaBuilder.CreateGEP(newAI, ConstantInt::get(IntPtrTy, offsetVector[offsetPos]));
            AI->replaceAllUsesWith(AllocaBuilder.CreatePointerCast(newObject, AI->getType()));
            ++offsetPos;
        }
        // Add merged object as the only static alloca.
        staticAllocas.insert(newAI);
    }

    Instruction *GetInitPosition(AllocaInst *AI, Instruction *firstNonAlloca, LoopInfo *LI, DominatorTree *DT) {
        std::set<std::pair<const Instruction*, const Value*> > SideEffects;
        SM->AccumulateUnsafeSideEffects(AI, SM->GetStaticAllocaAllocationSize(AI), SideEffects);
        const BasicBlock *dominator = (SideEffects.begin()->first)->getParent();
        for (auto &pair : SideEffects) {
            dominator = DT->findNearestCommonDominator(pair.first->getParent(), dominator);
        }
        while (LI->getLoopFor(dominator)) {
            dominator = DT->getNode(const_cast<BasicBlock*>(dominator))->getIDom()->getBlock();
        }
        if (dominator != firstNonAlloca->getParent()) {
            return const_cast<Instruction*>(&(*dominator->getFirstInsertionPt()));
        } else {
            return firstNonAlloca;
        }
    }

    virtual bool runOnFunction(Function &F) {
        if (!initialized)
            doInitialization(F.getParent());

        if (ISMETADATAFUNC(F.getName().str().c_str()))
            return false;

        SM = new SafetyManager(DL, &getAnalysis<ScalarEvolutionWrapperPass>().getSE());
        LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
        DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

        IRBuilder<> AllocaBuilder(F.getEntryBlock().getFirstInsertionPt());
        std::vector<AllocaInst*> allocasToRemove;
        // Find all safe static allocas in the function
        // Move them to the top of the function
        for (auto &i : F.getEntryBlock()) {
            Instruction *ins = &i;
            AllocaInst *AI = dyn_cast<AllocaInst>(ins);
            if (!AI || !AI->isStaticAlloca())
                continue;
            unsigned long Size = SM->GetStaticAllocaAllocationSize(AI);
            if (SM->IsSafeStackAlloca(AI, Size)) {
                AllocaInst *newAI = AllocaBuilder.CreateAlloca(AI->getAllocatedType(), AI->getArraySize(), AI->getName());
                newAI->setAlignment(AI->getAlignment());
                newAI->setUsedWithInAlloca(AI->isUsedWithInAlloca());
                AI->replaceAllUsesWith(newAI);
                allocasToRemove.push_back(AI);
            }
        }
        std::set<AllocaInst*> staticAllocas;        
        std::vector<AllocaInst*> oldStaticAllocas;
        // Find all non-safe static allocas in the function
        oldStaticAllocas.clear();
        for (auto &i : F.getEntryBlock()) {
            Instruction *ins = &i;
            AllocaInst *AI = dyn_cast<AllocaInst>(ins);
            if (!AI || !AI->isStaticAlloca())
                continue;
            unsigned long Size = SM->GetStaticAllocaAllocationSize(AI);
            if (!SM->IsSafeStackAlloca(AI, Size)) {
                oldStaticAllocas.push_back(AI);
            }
        }
        // Replace old non-safe static allocas with simpler representation
        if (!MergedStack) {
            // Just create new allocas at the top of the function if no merging requested
            for (auto &AI : oldStaticAllocas) {
                AllocaInst *newAI = AllocaBuilder.CreateAlloca(AI->getAllocatedType(), AI->getArraySize(), AI->getName());
                newAI->setAlignment(AI->getAlignment());
                newAI->setUsedWithInAlloca(AI->isUsedWithInAlloca());
                AI->replaceAllUsesWith(newAI);
                staticAllocas.insert(newAI);
            }
        } else if (oldStaticAllocas.size() > 0) {
            // Merge the static allocas into a single alloca for cheaper metadata management
            MergeAllocas(oldStaticAllocas, staticAllocas, AllocaBuilder);
        }
        // Remove old safe static allocas from function
        for (auto &AI : allocasToRemove)
            AI->eraseFromParent();
        // Remove old non-safe static allocas from function
        for (auto &AI : oldStaticAllocas)
            AI->eraseFromParent();

        BasicBlock::iterator It(F.getEntryBlock().getFirstInsertionPt());
        while (isa<AllocaInst>(*It) || isa<DbgInfoIntrinsic>(*It))
            ++It;
        Instruction *firstNonAlloca = &*It;
        
        std::vector<AllocaInst*> interestingAllocas;
        
        for (auto &bb : F) {
            for (auto &i : bb) {
                AllocaInst *AI = dyn_cast<AllocaInst>(&i);
                if (!AI)
                    continue;
                unsigned long Size = SM->GetStaticAllocaAllocationSize(AI);
                if (!SM->IsSafeStackAlloca(AI, Size)) {
                    interestingAllocas.push_back(AI);
                }
            }
        }

        for (auto AI : interestingAllocas) {
            unsigned long Size = SM->GetStaticAllocaAllocationSize(AI);
            // Create metadata if needed and insert it before the regular allocation
            // Performed here not to disturb the IRBuilder
            // metaData is scoped here to be accessible during initialization
            Value *metaData;
            if (DeepMetadata) {
                metaData = new AllocaInst(MetaDataTy, nullptr, "MetaData_" + AI->getName(), AI);
            }
            Instruction *insertBeforeInstruction;
            if (staticAllocas.count(AI) == 0) {
                BasicBlock::iterator nextIt(AI);
                ++nextIt;
                if (nextIt != AI->getParent()->end())
                    insertBeforeInstruction = &*nextIt;
            } else {
                insertBeforeInstruction = GetInitPosition(AI, firstNonAlloca, LI, DT);
            }
            IRBuilder<> B(insertBeforeInstruction);

            if (AI->getAlignment() < (1 << STACKALIGN))
                AI->setAlignment(1 << STACKALIGN);

            // Check if this is a large static allocation or not
            // Move to large-stack if it is
            if (UseLargeStack && staticAllocas.count(AI) != 0) {
                if (Size >= LargeStackThreshold) {
                    if (AI->getAlignment() < (1 << STACKALIGN_LARGE)) {
                        AI->setAlignment(1 << STACKALIGN_LARGE);
                    }
                }
            }

            // Uses metaset
            if (DeepMetadata) {
                // Initialize the metadata object
                Value *metaField;
                for (unsigned long i = 0; i < (DeepMetadataBytes / sizeof(unsigned long)); ++i) {
                    metaField = B.CreateConstGEP2_32(NULL, metaData, 0, i);
                    B.CreateStore(ConstantInt::get(IntPtrTy, 0, 0), metaField, true);
                }
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
        IntMetaTy = Type::getIntNTy(M->getContext(), 8 * MetadataBytes);
        PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

        std::vector<Type *>MetaMembers;
        for (unsigned long i = 0; i < (DeepMetadataBytes / sizeof(unsigned long)); ++i)
            MetaMembers.push_back(IntPtrTy);
        MetaDataTy = StructType::create(M->getContext(), MetaMembers);

        if (!FixedCompression) {
            //declare i64 @metaset_alignment(i64, i64, iM, i64)
            std::string functionName = "metaset_alignment_" + std::to_string(MetadataBytes);
            MetasetFunc = M->getOrInsertFunction(functionName, IntPtrTy,
                IntPtrTy, IntPtrTy, IntMetaTy, IntPtrTy, NULL);
            //declare i64 @metabaseget(i64)
            std::string functionName2 = "metabaseget";
            MetabasegetFunc = M->getOrInsertFunction(functionName2, IntPtrTy, IntPtrTy, NULL);
            //declare i64 @metaset_alignment(i64, i64, iM, i64)
            std::string functionName3 = "metaset_fast_" + std::to_string(MetadataBytes);
            MetasetFastFunc = M->getOrInsertFunction(functionName3, IntPtrTy,
                IntPtrTy, IntPtrTy, IntMetaTy, IntPtrTy, IntPtrTy, IntPtrTy, NULL);
          
            /* DangSan init function */ 
            std::string functionName4 = "dang_init_heapobj";
            DangSanInitFunc = M->getOrInsertFunction(functionName4, VoidTy, IntPtrTy, IntPtrTy,
                                                              NULL);
            /* DangSan Free function */
            std::string functionName5 = "dang_freeptr";
            DangSanFreeFunc = M->getOrInsertFunction(functionName5, VoidTy, IntPtrTy, IntPtrTy,
                                                                            NULL);
        } else {
            //declare i64 @metaset_fixed(i64, i64, iM)
            std::string functionName = "metaset_fixed_" + std::to_string(MetadataBytes);
            MetasetFunc = M->getOrInsertFunction(functionName, IntPtrTy,
                IntPtrTy, IntPtrTy, IntMetaTy, NULL);
        }

        initialized = true;
        
        return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.addRequired<ScalarEvolutionWrapperPass>();
        AU.addRequired<LoopInfoWrapperPass>();
        AU.addRequired<DominatorTreeWrapperPass>();
    }

};

char StackTracker::ID = 0;
static RegisterPass<StackTracker> X("stacktracker", "Stack Tracker Pass", true, false);




