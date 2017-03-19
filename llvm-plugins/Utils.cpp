/*
 * Utils.cpp
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
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetLowering.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>

#include <Utils.h>
#include <algorithm>

#define DEBUG_TYPE "safetymanager"

using namespace llvm;

cl::opt<bool> FixedCompression ("METALLOC_FIXEDCOMPRESSION", cl::desc("Enable fixed compression for METADATA"), cl::init(false));
cl::opt<unsigned long> MetadataBytes ("METALLOC_METADATABYTES", cl::desc("Number of METADATA bytes"), cl::init(8),
    cl::values(
        clEnumVal(1, ""),
        clEnumVal(2, ""),
        clEnumVal(4, ""),
        clEnumVal(8, ""),
        clEnumVal(16, ""),
       clEnumValEnd));
cl::opt<bool> DeepMetadata ("METALLOC_DEEPMETADATA", cl::desc("Enable multi-level METADATA"), cl::init(true));
cl::opt<unsigned long> DeepMetadataBytes ("METALLOC_DEEPMETADATABYTES", cl::desc("Number of bytes for second level METADATA"), cl::init(8),
    cl::values(
        clEnumVal(8, ""),
        clEnumVal(16, ""),
        clEnumVal(32, ""),
	clEnumVal(72, ""),
        clEnumVal(128, ""),
        clEnumValEnd));

/// Rewrite an SCEV expression for a memory access address to an expression that
/// represents offset from the given alloca.
///
/// The implementation simply replaces all mentions of the alloca with zero.
class AllocaOffsetRewriter : public SCEVRewriteVisitor<AllocaOffsetRewriter> {
  const Value *AllocaPtr;

public:
  AllocaOffsetRewriter(ScalarEvolution &SE, const Value *AllocaPtr)
      : SCEVRewriteVisitor(SE), AllocaPtr(AllocaPtr) {}

  const SCEV *visitUnknown(const SCEVUnknown *Expr) {
    if (Expr->getValue() == AllocaPtr)
      return SE.getZero(Expr->getType());
    return Expr;
  }
};

bool SafetyManager::DoesContainPointer(const Type *T) {
  if (T->isPtrOrPtrVectorTy())
    return true;
  if (T->isFunctionTy())
    return true;
  if (!T->isAggregateType())
    return false;
  for (const Type *subType : T->subtypes()) {
    if (DoesContainPointer(subType))
      return true;
  }
  return false;
}

unsigned long SafetyManager::GetStaticAllocaAllocationSize(const AllocaInst *AI) {
  unsigned long Size = DL->getTypeAllocSize(AI->getAllocatedType());
  if (AI->isArrayAllocation()) {
    auto C = dyn_cast<ConstantInt>(AI->getArraySize());
    if (!C)
      return 0;
    Size *= C->getZExtValue();
  }
  return Size;
}

unsigned long SafetyManager::GetByvalArgumentSize(const Argument *Arg) {
  if (!Arg->hasByValAttr())
      return 0;
  unsigned long Size = DL->getTypeStoreSize(Arg->getType()->getPointerElementType());
  return Size;
}

bool SafetyManager::IsAccessSafe(Value *Addr, unsigned long AccessSize,
                             const Value *AllocaPtr, unsigned long AllocaSize) {
  AllocaOffsetRewriter Rewriter(*SE, AllocaPtr);
  const SCEV *Expr = Rewriter.visit(SE->getSCEV(Addr));

  unsigned long BitWidth = SE->getTypeSizeInBits(Expr->getType());
  ConstantRange AccessStartRange = SE->getUnsignedRange(Expr);
  ConstantRange SizeRange =
      ConstantRange(APInt(BitWidth, 0), APInt(BitWidth, AccessSize));
  ConstantRange AccessRange = AccessStartRange.add(SizeRange);
  ConstantRange AllocaRange =
      ConstantRange(APInt(BitWidth, 0), APInt(BitWidth, AllocaSize));
  bool Safe = AllocaRange.contains(AccessRange);

  DEBUG(dbgs() << "[SafeStack] "
               << (isa<AllocaInst>(AllocaPtr) ? "Alloca " : "ByValArgument ")
               << *AllocaPtr << "\n"
               << "            Access " << *Addr << "\n"
               << "            SCEV " << *Expr
               << " U: " << SE->getUnsignedRange(Expr)
               << ", S: " << SE->getSignedRange(Expr) << "\n"
               << "            Range " << AccessRange << "\n"
               << "            AllocaRange " << AllocaRange << "\n"
               << "            " << (Safe ? "safe" : "unsafe") << "\n");

  return Safe;
}

bool SafetyManager::IsMemIntrinsicSafe(const MemIntrinsic *MI, const Use &U,
                                   const Value *AllocaPtr,
                                   unsigned long AllocaSize) {
  // All MemIntrinsics have destination address in Arg0 and size in Arg2.
  if (MI->getRawDest() != U) return true;
  const auto *Len = dyn_cast<ConstantInt>(MI->getLength());
  // Non-constant size => unsafe. FIXME: try SCEV getRange.
  if (!Len) return false;
  return IsAccessSafe(U, Len->getZExtValue(), AllocaPtr, AllocaSize);
}

/// Check whether a given allocation must be put on the safe
/// stack or not. The function analyzes all uses of AI and checks whether it is
/// only accessed in a memory safe way (as decided statically).
bool SafetyManager::IsSafeStackAlloca(const Value *AllocaPtr, unsigned long AllocaSize) {
  // Go through all uses of this alloca and check whether all accesses to the
  // allocated object are statically known to be memory safe and, hence, the
  // object can be placed on the safe stack.
  SmallPtrSet<const Value *, 16> Visited;
  SmallVector<const Value *, 8> WorkList;
  WorkList.push_back(AllocaPtr);

  // A DFS search through all uses of the alloca in bitcasts/PHI/GEPs/etc.
  while (!WorkList.empty()) {
    const Value *V = WorkList.pop_back_val();
    for (const Use &UI : V->uses()) {
      auto I = cast<const Instruction>(UI.getUser());
      assert(V == UI.get());

      switch (I->getOpcode()) {
      case Instruction::Load: {
        if (!IsAccessSafe(UI, DL->getTypeStoreSize(I->getType()), AllocaPtr,
                          AllocaSize))
          return false;
        break;
      }
      case Instruction::VAArg:
        // "va-arg" from a pointer is safe.
        break;
      case Instruction::Store: {
        if (V == I->getOperand(0)) {
          // Stored the pointer - conservatively assume it may be unsafe.
          DEBUG(dbgs() << "[SafeStack] Unsafe alloca: " << *AllocaPtr
                       << "\n            store of address: " << *I << "\n");
          return false;
        }

        if (!IsAccessSafe(UI, DL->getTypeStoreSize(I->getOperand(0)->getType()),
                          AllocaPtr, AllocaSize))
          return false;
        break;
      }
      case Instruction::Ret: {
        // Information leak.
        return false;
      }

      case Instruction::Call:
      case Instruction::Invoke: {
        ImmutableCallSite CS(I);

        if (const IntrinsicInst *II = dyn_cast<IntrinsicInst>(I)) {
          if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
              II->getIntrinsicID() == Intrinsic::lifetime_end)
            continue;
        }

        if (const MemIntrinsic *MI = dyn_cast<MemIntrinsic>(I)) {
          if (!IsMemIntrinsicSafe(MI, UI, AllocaPtr, AllocaSize)) {
            DEBUG(dbgs() << "[SafeStack] Unsafe alloca: " << *AllocaPtr
                         << "\n            unsafe memintrinsic: " << *I
                         << "\n");
            return false;
          }
          continue;
        }

        // LLVM 'nocapture' attribute is only set for arguments whose address
        // is not stored, passed around, or used in any other non-trivial way.
        // We assume that passing a pointer to an object as a 'nocapture
        // readnone' argument is safe.
        // FIXME: a more precise solution would require an interprocedural
        // analysis here, which would look at all uses of an argument inside
        // the function being called.
        ImmutableCallSite::arg_iterator B = CS.arg_begin(), E = CS.arg_end();
        for (ImmutableCallSite::arg_iterator A = B; A != E; ++A)
          if (A->get() == V)
            if (!(CS.doesNotCapture(A - B) && (CS.doesNotAccessMemory(A - B) ||
                                               CS.doesNotAccessMemory()))) {
              DEBUG(dbgs() << "[SafeStack] Unsafe alloca: " << *AllocaPtr
                           << "\n            unsafe call: " << *I << "\n");
              return false;
            }
        continue;
      }

      default:
        if (Visited.insert(I).second)
          WorkList.push_back(cast<const Instruction>(I));
      }
    }
  }

  // All uses of the alloca are safe, we can place it on the safe stack.
  return true;
}

void SafetyManager::AccumulateSideEffects(const Value *AllocaPtr, std::set<const Instruction*> &SideEffects) {
  SmallPtrSet<const Value *, 16> Visited;
  SmallVector<const Value *, 8> WorkList;
  WorkList.push_back(AllocaPtr);

  // A DFS search through all uses of the alloca in bitcasts/PHI/GEPs/etc.
  while (!WorkList.empty()) {
    const Value *V = WorkList.pop_back_val();
    for (const Use &UI : V->uses()) {
      Constant *C = dyn_cast<Constant>(UI.getUser());
      if (C) {
        if (Visited.insert(C).second)
          WorkList.push_back(C);
        continue;
      }
      auto I = cast<const Instruction>(UI.getUser());
      assert(V == UI.get());

      switch (I->getOpcode()) {
      case Instruction::Load: {
        SideEffects.insert(I);
        break;
      }
      case Instruction::VAArg:
        // "va-arg" from a pointer is safe.
        break;
      case Instruction::Store: {
        SideEffects.insert(I);
        break;
      }
      case Instruction::Ret: {
        SideEffects.insert(I);
        break;
      }
      case Instruction::Call:
      case Instruction::Invoke: {
        SideEffects.insert(I);
        break;
      }

      default:
        if (Visited.insert(I).second)
          WorkList.push_back(cast<const Instruction>(I));
      }
    }
  }

  return;
}

void SafetyManager::AccumulateUnsafeSideEffects(const Value *AllocaPtr, unsigned long AllocaSize, std::set<std::pair<const llvm::Instruction*, const llvm::Value*> > &UnsafeSideEffects) {
  SmallPtrSet<const Value *, 16> Visited;
  SmallVector<const Value *, 8> WorkList;
  WorkList.push_back(AllocaPtr);

  // A DFS search through all uses of the alloca in bitcasts/PHI/GEPs/etc.
  while (!WorkList.empty()) {
    const Value *V = WorkList.pop_back_val();
    for (const Use &UI : V->uses()) {
      Constant *C = dyn_cast<Constant>(UI.getUser());
      if (C) {
        if (Visited.insert(C).second)
          WorkList.push_back(C);
        continue;
      }
      auto I = cast<const Instruction>(UI.getUser());
      assert(V == UI.get());

      switch (I->getOpcode()) {
      case Instruction::Load: {
        if (!IsAccessSafe(UI, DL->getTypeStoreSize(I->getType()), AllocaPtr,
                          AllocaSize)) {
          UnsafeSideEffects.insert(std::make_pair(I, V));
        }
        break;
      }
      case Instruction::VAArg:
        // "va-arg" from a pointer is safe.
        break;
      case Instruction::Store: {
        if (V == I->getOperand(0)) {
          UnsafeSideEffects.insert(std::make_pair(I, V));
          break;
        }
        if (!IsAccessSafe(UI, DL->getTypeStoreSize(I->getOperand(0)->getType()),
                          AllocaPtr, AllocaSize)) {
          UnsafeSideEffects.insert(std::make_pair(I, V));
        }
        break;
      }
      case Instruction::Ret: {
        UnsafeSideEffects.insert(std::make_pair(I, V));
        break;
      }
      case Instruction::Call:
      case Instruction::Invoke: {
        ImmutableCallSite CS(I);

        if (const MemIntrinsic *MI = dyn_cast<MemIntrinsic>(I)) {
          if (!IsMemIntrinsicSafe(MI, UI, AllocaPtr, AllocaSize)) {
            UnsafeSideEffects.insert(std::make_pair(MI, V));
          }
          break;
        }

        ImmutableCallSite::arg_iterator B = CS.arg_begin(), E = CS.arg_end();
        for (ImmutableCallSite::arg_iterator A = B; A != E; ++A)
          if (A->get() == V)
            if (!(CS.doesNotCapture(A - B) && (CS.doesNotAccessMemory(A - B) ||
                                               CS.doesNotAccessMemory()))) {
              UnsafeSideEffects.insert(std::make_pair(I, V));
            }
        break;
      }

      default:
        if (Visited.insert(I).second)
          WorkList.push_back(cast<const Instruction>(I));
      }
    }
  }

  return;
}

void SafetyManager::AccumulateSafeSideEffects(const Function *F, std::set<const Instruction*> &SafeSideEffects) {
  std::set<const Instruction*> AllSideEffects;
  std::set<std::pair <const Instruction*, const Value *> > UnsafeSideEffects;
  for (const Instruction &I : instructions(F)) {
    if (auto AI = dyn_cast<AllocaInst>(&I)) {
      AccumulateSideEffects(AI, AllSideEffects);
      AccumulateUnsafeSideEffects(AI, GetStaticAllocaAllocationSize(AI), UnsafeSideEffects);
    }
    if (auto CI = dyn_cast<CallInst>(&I)) {
      if (CI->getCalledFunction() && 
            (CI->getCalledFunction()->getName().equals("_Znwm") || CI->getCalledFunction()->getName().equals("_Znam") || CI->getCalledFunction()->getName().equals("malloc")) && 
            isa<ConstantInt>(CI->getArgOperand(0))) {
        AccumulateSideEffects(CI, AllSideEffects);
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(0)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects);
      } else if (CI->getCalledFunction() && 
            (CI->getCalledFunction()->getName().equals("calloc")) && 
            isa<ConstantInt>(CI->getArgOperand(0)) && isa<ConstantInt>(CI->getArgOperand(1))) {
        AccumulateSideEffects(CI, AllSideEffects);
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(0)))->getZExtValue() *
                                (cast<ConstantInt>(CI->getArgOperand(1)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects);
      } else if (CI->getCalledFunction() && 
            CI->getCalledFunction()->getName().equals("realloc") && 
            isa<ConstantInt>(CI->getArgOperand(1))) {
        AccumulateSideEffects(CI, AllSideEffects);
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(1)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects);
      } else if (CI->getCalledFunction() && 
            CI->getCalledFunction()->getName().equals("__errno_location")) {
        AccumulateSideEffects(CI, AllSideEffects);
      }
    }
  }
  std::set<const Instruction*> RelevantUnsafeSideEffects;
  for (auto &pair : UnsafeSideEffects) {
    auto *SI = dyn_cast<StoreInst>(pair.first);
    if (SI && SI->getPointerOperand() == pair.second) {
      RelevantUnsafeSideEffects.insert(SI);
    }
  }
  std::set_difference(AllSideEffects.begin(), AllSideEffects.end(), RelevantUnsafeSideEffects.begin(), RelevantUnsafeSideEffects.end(), 
            std::inserter(SafeSideEffects, SafeSideEffects.end()));
}

void SafetyManager::AccumulateUnsafeSideEffects(const Function *F, std::map<const Value*, std::set<std::pair<const llvm::Instruction*, const llvm::Value*> > > &UnsafeSideEffects) {
  for (const Instruction &I : instructions(F)) {
    if (auto *AI = dyn_cast<AllocaInst>(&I)) {
      AccumulateUnsafeSideEffects(AI, GetStaticAllocaAllocationSize(AI), UnsafeSideEffects[AI]);
    }
    if (auto *CI = dyn_cast<CallInst>(&I)) {
      if (CI->getCalledFunction() && 
            (CI->getCalledFunction()->getName().equals("_Znwm") || CI->getCalledFunction()->getName().equals("_Znam") || CI->getCalledFunction()->getName().equals("malloc")) && 
            isa<ConstantInt>(CI->getArgOperand(0))) {
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(0)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects[CI]);
      } else if (CI->getCalledFunction() && 
            (CI->getCalledFunction()->getName().equals("calloc")) && 
            isa<ConstantInt>(CI->getArgOperand(0)) && isa<ConstantInt>(CI->getArgOperand(1))) {
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(0)))->getZExtValue() *
                                (cast<ConstantInt>(CI->getArgOperand(1)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects[CI]);
      } else if (CI->getCalledFunction() && 
            CI->getCalledFunction()->getName().equals("realloc") && 
            isa<ConstantInt>(CI->getArgOperand(1))) {
        unsigned long Size = (cast<ConstantInt>(CI->getArgOperand(1)))->getZExtValue();
        AccumulateUnsafeSideEffects(CI, Size, UnsafeSideEffects[CI]);
      } else if (CI->getCalledFunction() && 
            CI->getCalledFunction()->getName().equals("__errno_location")) {
        //Skip
      } else if (CI->getType()->isPointerTy()) {
        AccumulateUnsafeSideEffects(CI, 1, UnsafeSideEffects[CI]);
      }
    }
    if (auto *LI = dyn_cast<LoadInst>(&I)) {
      if (LI->getType()->isPointerTy()) {
        AccumulateUnsafeSideEffects(LI, 1, UnsafeSideEffects[LI]);
      }
    }
  }
  for (auto &a : F->args()) {
    const Argument *Arg = dyn_cast<Argument>(&a);
    if (!Arg->hasByValAttr() && Arg->getType()->isPointerTy()) {
      AccumulateUnsafeSideEffects(Arg, 1, UnsafeSideEffects[Arg]);
    }
  }
}

void SafetyManager::AccumulateSafeSideEffects(const Module *M, std::set<const Instruction*> &SafeSideEffects) {
  std::set<const Instruction*> AllSideEffects;
  std::set<std::pair <const Instruction*, const Value *> > UnsafeSideEffects;
  for (auto& global: M->globals()) {
    const GlobalValue *G = &global;
    AccumulateSideEffects(G, AllSideEffects);
    unsigned long Size = DL->getTypeAllocSize(G->getType()->getPointerElementType());
    AccumulateUnsafeSideEffects(G, Size, UnsafeSideEffects);
  }
  std::set<const Instruction*> RelevantUnsafeSideEffects;
  for (auto &pair : UnsafeSideEffects) {
    auto *SI = dyn_cast<StoreInst>(pair.first);
    if (SI && SI->getPointerOperand() == pair.second) {
      RelevantUnsafeSideEffects.insert(SI);
    }
  }
  std::set_difference(AllSideEffects.begin(), AllSideEffects.end(), RelevantUnsafeSideEffects.begin(), RelevantUnsafeSideEffects.end(), 
            std::inserter(SafeSideEffects, SafeSideEffects.end()));
}

void SafetyManager::AccumulateUnsafeSideEffects(const Module *M, std::map<const GlobalValue *, std::set<std::pair<const llvm::Instruction*, const llvm::Value*> > > &UnsafeSideEffects) {
  for (auto& global: M->globals()) {
    const GlobalValue *G = &global;
    unsigned long Size = DL->getTypeAllocSize(G->getType()->getPointerElementType());
    AccumulateUnsafeSideEffects(G, Size, UnsafeSideEffects[G]);
  }
}
