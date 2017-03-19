/*
 * This file has FreeSentry loop optimization.
 */
#define DEBUG_TYPE "FreeSentryLoop"

#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetFolder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PredIteratorCache.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
//#include "llvm/Support/raw_ostream.h"
//#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/ADT/Statistic.h"
#include "FreeSentry.h"

#include <iostream>
#include <fstream>
#include <set>

#include <metadata.h>

using namespace llvm;

#define UAFFUNC "inlinedang_registerptr"

#ifdef DANG_DEBUG
#define DEBUG_MSG(err)  err
#else
#define DEBUG_MSG(err)
#endif

STATISTIC(FreeSentryLoopStat, "Counts number of register pointer calls that were moved due to loop optimization");


static cl::opt<std::string> FSGOFile("Lfsgout", cl::desc("File to write FreeSentry call analysis too"),
				    cl::init("/tmp/fs-callmodel.raw"), cl::Optional);
static cl::opt<std::string> FSGIFile("Lfsgin", cl::desc("File to read FreeSentry call model from"),
				    cl::init("/tmp/fs-callmodel.res"), cl::Optional);
static cl::opt<std::string> FSGSFile("Lfsgsys",  cl::desc("File to read system FreeSentry call model from"),
				    cl::init("/usr/share/freesentry/callmodel.res"), cl::Optional);



namespace {

  typedef std::string String;


  class FreeSentryShared {
    public:
	FreeSentryShared() {
	}

    	void loadcallgraph (String filename) {
      	   std::ifstream infile (filename);
      	   String funcname;
      	   int freecalled;

      	   while (infile >> funcname >> freecalled) {
		freecalls[funcname] = freecalled;
      	   }
        }

	bool callsfree (String funcname) {
            
	  /*if (freecalls.find(funcname) == freecalls.end()) {
		  DEBUG_MSG(errs() << "FSL: can't find function in freecalls, assuming free is called" << "\n");
		return true;
      	  } else {
		bool freecalled = freecalls[funcname];
		// debug
		if (freecalled)
		  DEBUG_MSG(errs() << "FSL: free is called" << "\n");
		else
		  DEBUG_MSG(errs() << "FSL: free is not called" << "\n");
		return freecalled;
      	  }*/
            return false;
	}

    // if address is taken or GetElementPtr is called on this instruction
    bool HATcheck (Instruction * AI, User *U, Loop * L, Instruction *cast) {
	if (StoreInst * SI = dyn_cast < StoreInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: StoreInst " << *SI << "\n");
	  if (AI == SI->getValueOperand ())
	    return true;
	}
	else if (PtrToIntInst * PI = dyn_cast < PtrToIntInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: PtrToIntInst " << *PI << "\n");
	  if (AI == PI->getOperand (0))
	    return true;
	}

	else if (CallInst * CI = dyn_cast < CallInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: CallInst " << *U << "\n");
	  Function *F = CI->getCalledFunction ();
	  if (F) {
	    StringRef funcname = F->getName ();
	    //if (funcname == UAFFUNC) {
	    if (ISMETADATAFUNC(funcname.str().c_str())) {
	      return false;
	    }
	  }
	  return true;
	} else if (InvokeInst *II = dyn_cast < InvokeInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: InvokeInst " << *II << "\n");
            II = NULL; // TODO: Just to make compiler happy
	  return true;
	}
	else if (SelectInst * SI = dyn_cast < SelectInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: SelectInst " << *SI << "\n");
	  if (HAT (SI, cast, L))
	    return true;
	}
	else if (PHINode * PN = dyn_cast < PHINode > (U)) {
	  DEBUG_MSG(errs() << "HAT: PHINode " << *PN << "\n");
	  if (VisitedPHIs.insert(PN).second)
	    if (HAT (PN, cast, L))
	      return true;
	}
	else if (GetElementPtrInst * GEP =
		 dyn_cast < GetElementPtrInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: GetElementPtrInst " << *GEP << "\n");
	  if (AI == GEP->getPointerOperand())
	    return true;
	  else if (HAT (GEP, cast, L))
	    return true;

	}
	else if (BitCastInst * BI = dyn_cast < BitCastInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: Bitcast " << *BI << "\n");
	  if (HAT (BI, cast, L))
	    return true;
	}
	return false;
    }


    bool HAT (Instruction * AI, Instruction *cast, Loop *L) {
        /* TODO: Remove this 
         * This is returned to remove all store instructions from loop.
         */
        return false;
      for (User * U:AI->users ()) {
	// can't do this, if anyone gets an address that's a problem
	/*   Instruction *UI = cast<Instruction>(U);
	   if (L) {
	      if (L->contains(UI)) {
	    	errs() << "User in loop\n";
	      } else {
		errs() << "User not in loop\n";
		continue;
	      }
	   } */
        if (cast == dyn_cast<Instruction>(U)) {
            continue;
        }
        
	if (HATcheck(AI, U, L, cast)) return true;
      }
      return false;
    }

    inline Value* getBoundedAddr(Value *obj_addr) {
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(obj_addr)) {
            if (1 == GEP->getNumIndices()) {
                if (ConstantInt *C = dyn_cast<ConstantInt>(GEP->getOperand(1)))
                    if (C && C->getValue().isNegative())
                        return GEP->getOperand(0);
                if (Instruction *SubI = dyn_cast<Instruction>(GEP->getOperand(1))) {
                    if (SubI->getOpcode() == Instruction::Sub) {
                        ConstantInt *C = dyn_cast<ConstantInt>(SubI->getOperand(0));
                        if (C && C->isZero())
                            return GEP->getOperand(0);
                    }
                }
            }
        }
        return obj_addr;
    }

    mutable StringMap < int > freecalls;
    SmallPtrSet < const PHINode *, 16 > VisitedPHIs;


  };

  struct FreeSentryLoop:public LoopPass, FreeSentryShared {
    static char ID;		// Pass identification, replacement for typeid
      FreeSentryLoop ():LoopPass (ID) {
	//initializeFreeSentryLoopPass(*PassRegistry::getPassRegistry());
      }
      FreeSentryLoop (bool setflag): LoopPass(ID) {flag = setflag; FreeSentryLoop();}

    LoopInfo *LI;
    DominatorTree *DT;
    TargetLibraryInfo *TLI;
    bool flag = false;
    bool initialized = false;

    using llvm::Pass::doInitialization;
    bool doInitialization (Loop * L, LPPassManager & LPM) override {
      DEBUG_MSG(errs() << "FSL: Loop init: << " << FSGIFile << "\n");
	loadcallgraph(FSGIFile);
	loadcallgraph(FSGSFile);
        initialized = true;
      return false;
    }

    using llvm::Pass::doFinalization;
    bool doFinalization() override {
	return false;
    }

    void loopcallcheck (Loop * L, bool * lcallsfree, bool * lcallsregptr) {
      for (Loop::block_iterator I = L->block_begin (), E = L->block_end ();
	   I != E; ++I) {
	BasicBlock *BB = *I;
	if (LI->getLoopFor (BB) == L) {
	  for (BasicBlock::iterator I = BB->begin (), E = BB->end ();
	       (I != E); ++I) {
	    Instruction *inst = &*I;
	    if (isa < CallInst > (inst)) {
	      DEBUG_MSG(errs() << "FSL: Found call instruction: " << *inst << "\n");
	      CallInst *CI = dyn_cast < CallInst > (inst);
	      Function *F = CI->getCalledFunction ();
	      if (F) {
		StringRef funcname = F->getName ();
		DEBUG_MSG(errs() << "FSL: Function called: " << funcname << "\n");
		if (funcname == UAFFUNC) {
		  *lcallsregptr = true;
		//} else if (callsfree(funcname)) {
		} else if (isFreeCall(CI, TLI)) {
		  *lcallsfree = true;
		}
	     } else {		// indirect call, assume it calls free
		*lcallsfree = true;
	      }
	    }
	  }
	}
      }
    }

    // based on LICM

    bool runOnLoop (Loop * L, LPPassManager & LPM) override {
        if (!initialized) {
            doInitialization(L, LPM);
        }
      bool freecall = false;
      bool regptrcall = false;
      bool changed = false;

      BasicBlock *Header = L->getHeader();
      Function *F = Header->getParent();
      //errs() << "Loop Pass \n"  ;
      /*if (!(F->hasFnAttribute(Attribute::FreeSentry) || flag)) {
	return false;
      }*/

      /*if (F->hasFnAttribute(Attribute::NoFreeSentry))
	return false;
        */
        if (ISMETADATAFUNC(F->getName().str().c_str()))
            return false; 

      DEBUG_MSG(errs() << "FSL: Running on loop: " << F->getName().str().c_str());

      LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
      TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

      for (Loop::iterator LoopItr = L->begin (), LoopItrE = L->end ();
	   LoopItr != LoopItrE; ++LoopItr) {
	Loop *InnerL = *LoopItr;
	DEBUG_MSG(errs() << "FSL: Inner loop:" << *InnerL << "\n");
	loopcallcheck (InnerL, &freecall, &regptrcall);
      }
      loopcallcheck (L, &freecall, &regptrcall);

      if (freecall) {
	DEBUG_MSG(errs() << "FSL: Function in loop calls free\n");
	return false;
      }
      if (!regptrcall) {
	DEBUG_MSG(errs() << "FSL: Regptr is not called in loop\n");
	return false;
      }

      SmallVector < Instruction *, 8 > InstToDelete;

      for (Loop::block_iterator I = L->block_begin(), E = L->block_end ();
	   I != E; ++I) {
	BasicBlock *BB = *I;
	if (LI->getLoopFor (BB) == L) {
	  for (BasicBlock::iterator I = BB->begin (), E = BB->end ();
	       (I != E); ++I) {
	    Instruction *inst = &*I;
	    if (isa < CallInst > (inst)) {
	      CallInst *CI = dyn_cast < CallInst > (inst);
	      Function *F = CI->getCalledFunction();
	      if (F) {
		StringRef funcname = F->getName();
		DEBUG_MSG(errs() << "FSL: Function called: " << funcname << "\n");
		if (funcname == UAFFUNC) {
		  if (CastInst * cast =
		      dyn_cast < CastInst > (CI->getArgOperand (0))) {
		    Value *arg = cast->getOperand (0);
		    DEBUG_MSG(errs() << "FSL: Arg: " << *arg << "\n");
		    if (Instruction * AI = dyn_cast < Instruction > (arg)) {
		      if (HAT (AI, cast, L)) {
			DEBUG_MSG(errs() << "FSL: Address taken: " << *AI << "\n" );
			continue;
		      }

		      if (GetElementPtrInst * GEP =
			  dyn_cast < GetElementPtrInst > (AI)) {
			DEBUG_MSG(errs() << "FSL: GEP instruction:" << GEP << "\n");
                        GEP = NULL; // TODO: This is just to make compiler happy
			continue;
		      }
		      DEBUG_MSG(errs() << "FSL: checking domination\n");
		      // no address taken and not a GEP instruction
		      // move outside of loop
		      BasicBlock *EXBB = L->getExitBlock();

		      SmallVector<BasicBlock*, 8> ExitBlocks;
  		      L->getExitBlocks(ExitBlocks);

		      bool dominates = true;

 		      for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i) {
    			if (!DT->dominates(AI->getParent(), ExitBlocks[i])) {
				DEBUG_MSG(errs() << "Does not dominate exit: " << *arg << "\n");
				dominates = false;
			}
		      }
                    
		      if (dominates && EXBB) {
			Instruction *EXBBI = &EXBB->front ();
			DEBUG_MSG(errs() << "Exit BB: " << *EXBB << "\n");
			DEBUG_MSG(errs() << "Exit instruction: " << *EXBBI << "\n");
			DEBUG_MSG(errs() << "inst: " << *CI << "\n");
			DEBUG_MSG(errs() << "end: " << *BB->getTerminator () << "\n");
			DEBUG_MSG(errs() << "term: " << CI->isTerminator () << "\n");
			Function *F = BB->getParent ();
			Module *M = F->getParent ();
                        const DataLayout *DL = &(M->getDataLayout());  
                        Constant *regptr_def = M->getOrInsertFunction (UAFFUNC,
                                                Type::getVoidTy (M->getContext ()),
                                                DL->getIntPtrType(M->getContext(), 0),
                                                DL->getIntPtrType(M->getContext(), 0),
                                                NULL);

			Function *regptr = dyn_cast < Function > (regptr_def);
			regptr->setCallingConv (CallingConv::C);
                        
                        Instruction *insertBeforeInstruction = EXBBI;
                        //BasicBlock::iterator nextIt(EXBBI);
                        
                        if (isa<PHINode>(EXBBI)) {
                            continue; 
                        }
                        //insertBeforeInstruction = &*nextIt;
                        IRBuilder<> B(insertBeforeInstruction);
			std::vector < Value * >Args;
			Value *newcast = B.CreatePointerCast(arg, DL->getIntPtrType(M->getContext(), 0));
                        Value *load_cast = B.CreateIntToPtr(newcast, B.getInt64Ty()->getPointerTo());
                        Value *LI = B.CreateLoad(load_cast);

			//Args.push_back (B.CreatePtrToInt(newcast, DL->getIntPtrType(M->getContext(), 0)));
                        //Args.push_back(B.CreatePtrToInt(LI, DL->getIntPtrType(M->getContext(), 0)));
                        //Value *arg1 = CI->getOperand(0);
                        //Value *arg2 = CI->getOperand(1);
                        Args.push_back(B.CreatePtrToInt(newcast, DL->getIntPtrType(M->getContext(), 0)));
                        Args.push_back(B.CreatePtrToInt(LI, DL->getIntPtrType(M->getContext(), 0)));
                        /* TODO: remove this 
                         * Below call is commented to remove all store instructions.
                         */
                        //B.CreateCall (regptr, Args, "");
			InstToDelete.push_back (CI);
	    		FreeSentryLoopStat++;
			changed = true;
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }

      while (!InstToDelete.empty ()) {
	Instruction *del = InstToDelete.pop_back_val ();
	DEBUG_MSG(errs() << "FSL: deleting:" << del << "\n");
	del->eraseFromParent ();
	changed = true;
      }

      return changed;
    }

    void getAnalysisUsage (AnalysisUsage & AU) const override {
      AU.setPreservesCFG ();
      AU.addRequired < DominatorTreeWrapperPass > ();
      //AU.addRequired < LoopInfo > ();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequiredID (LoopSimplifyID);
      AU.addPreservedID (LoopSimplifyID);
      AU.addRequiredID (LCSSAID);
      AU.addPreservedID (LCSSAID);
      AU.addRequired <AAResultsWrapperPass> ();
      AU.addPreserved <AAResultsWrapperPass> ();
      AU.addPreserved <ScalarEvolutionWrapperPass> ();
      AU.addRequired <TargetLibraryInfoWrapperPass> ();
      //AU.addRequired <FreeSentry> ();
    }
  };

}

char FreeSentryLoop::ID = 0;

static RegisterPass <FreeSentryLoop> Y("FreeSentryLoop", "UAF Protection Loop optimization");
/*
INITIALIZE_PASS_BEGIN(FreeSentryLoop, "FreeSentryLoop", "UAF Protection Loop optimization", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfo)
INITIALIZE_PASS_DEPENDENCY(LoopSimplify)
INITIALIZE_PASS_DEPENDENCY(LCSSA)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfo)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)
INITIALIZE_PASS_END(FreeSentryLoop, "FreeSentryLoop", "UAF Protection Loop optimization", false, false)

Pass *llvm::createFreeSentry() {
  return new FreeSentry();
}

Pass *llvm::createFreeSentry(bool flag) {
  return new FreeSentry(flag);
}

Pass *llvm::createFreeSentryLoop() {
  return new FreeSentryLoop();
}

Pass *llvm::createFreeSentryLoop(bool flag) {
  return new FreeSentryLoop(flag);
}

Pass *llvm::createFSGraph() {
  return new FSGraph();
}

Pass *llvm::createFSGraph(bool flag) {
  return new FSGraph(flag);
}
*/
