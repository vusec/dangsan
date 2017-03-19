/*
 * This file has FreeSentry compiler pass. Modified
 * to work with DangSan backend.
 */
#define DEBUG_TYPE "FreeSentry"

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

#define FSdebug

#define UAFFUNC "inlinedang_registerptr"

#ifdef DANG_DEBUG
#define DEBUG_MSG(err)  err
#else
#define DEBUG_MSG(err)
#endif

STATISTIC(FreeSentryRegptr, "Counts number of register pointer calls that were added before optimization");
STATISTIC(FreeSentryRegptrCall, "Counts number of register pointer calls that were added due to calls");
STATISTIC(FreeSentryRegptrStore, "Counts number of register pointer calls that were added due to stores");
STATISTIC(FreeSentryLoopStat, "Counts number of register pointer calls that were moved due to loop optimization");


static cl::opt<std::string> FSGOFile("fsgout", cl::desc("File to write FreeSentry call analysis too"),
				    cl::init("/tmp/fs-callmodel.raw"), cl::Optional);
static cl::opt<std::string> FSGIFile("fsgin", cl::desc("File to read FreeSentry call model from"),
				    cl::init("/tmp/fs-callmodel.res"), cl::Optional);
static cl::opt<std::string> FSGSFile("fsgsys",  cl::desc("File to read system FreeSentry call model from"),
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
	  if (freecalls.find(funcname) == freecalls.end()) {
		  DEBUG_MSG(errs() << "FSL: can't find function in freecalls, assuming free is called" << "\n");
		return true;
      	  } else {
		bool freecalled = freecalls[funcname];
		// debug
		/*if (freecalled)
		  DEBUG_MSG(errs() << "FSL: free is called" << "\n");
		else
		  DEBUG_MSG(errs() << "FSL: free is not called" << "\n");*/
		return freecalled;
      	  }
	}

    // if address is taken or GetElementPtr is called on this instruction
    bool HATcheck (Instruction * AI, User *U, Loop * L) {
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
	  if (HAT (SI, L))
	    return true;
	}
	else if (PHINode * PN = dyn_cast < PHINode > (U)) {
	  DEBUG_MSG(errs() << "HAT: PHINode " << *PN << "\n");
	  if (VisitedPHIs.insert(PN).second)
	    if (HAT (PN, L))
	      return true;
	}
	else if (GetElementPtrInst * GEP =
		 dyn_cast < GetElementPtrInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: GetElementPtrInst " << *GEP << "\n");
	  if (AI == GEP->getPointerOperand())
	    return true;
	  else if (HAT (GEP, L))
	    return true;

	}
	else if (BitCastInst * BI = dyn_cast < BitCastInst > (U)) {
	  DEBUG_MSG(errs() << "HAT: Bitcast " << *BI << "\n");
	  if (HAT (BI, L))
	    return true;
	}
	return false;
    }


    bool HAT (Instruction * AI, Loop *L) {
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
	if (HATcheck(AI, U, L)) return true;
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


  struct FreeSentry:public FunctionPass, FreeSentryShared {
    static char ID;
      FreeSentry ():FunctionPass (ID) {
      }
      FreeSentry (bool setflag): FunctionPass(ID) {flag = setflag; FreeSentry();}

    //bool flag = false;
    bool flag = true;
    bool initialized = false;
    Module *M;
    const DataLayout *DL; 
    Type* VoidTy;
    IntegerType *IntPtrTy;
    Constant    *RegisterPtrFunc;

    using llvm::Pass::doInitialization;
    //bool doInitialization (Module & Mod) override {
    bool doInitialization (Module *Mod) {

	loadcallgraph(FSGIFile);
	loadcallgraph(FSGSFile);
        M = Mod;
        DL = &(M->getDataLayout());
        if (!DL) {
            report_fatal_error("Data layout required");
        }
        VoidTy = Type::getVoidTy(M->getContext());
        IntPtrTy = DL->getIntPtrType(M->getContext(), 0);
        
        RegisterPtrFunc = M->getOrInsertFunction(UAFFUNC, VoidTy, IntPtrTy,
                                                        IntPtrTy, NULL);
        initialized = true;
      return false;
    }

    using llvm::Pass::doFinalization;
    bool doFinalization (Module & M) override {
        return false;
    }

    bool runOnFunction (Function & F) override {
        
        if (!initialized) {
            doInitialization(F.getParent());
        }

      bool freecalled;
      bool changed = false;
      String funcname = F.getName();

      /* Metalloc : Skip run-time functions */
      if (ISMETADATAFUNC(funcname.c_str())) {
        return false;
      }
            
      freecalled = callsfree(funcname);

      if (!flag) {
	return false;
      }

      DEBUG_MSG(errs() << "FS: entering " << funcname << "\n");

      if (freecalled) {
	DEBUG_MSG(errs() << "FS: This function calls free\n");
      } else {
	DEBUG_MSG(errs() << "FS: This funtion does not call free\n");
      }

      bool prevcall = false;
      for (inst_iterator i = inst_begin (F), e = inst_end (F); i != e; ++i) {
	Instruction *I = &*i;
        if (strstr(F.getName().str().c_str(), "Perl_ck_split") != NULL) {
            I->dump();
        }
        if (isa < CallInst > (I)) {

	  DEBUG_MSG(errs() << "FS: call instruction: " << *I << "\n");
	  CallInst *CI = dyn_cast < CallInst > (I);


	 Function *Func = CI->getCalledFunction ();
	 if (Func) {
	    StringRef funcname = Func->getName ();
	    DEBUG_MSG(errs() << "FS: Function called: " << funcname << "\n");
	    //if (funcname == UAFFUNC) {
	    if (ISMETADATAFUNC(funcname.str().c_str())) {
		continue;
	    }
	  }

	  Value *Callee = CI->getCalledValue();
	  Type *CalleeType = Callee->getType();
	  if (CalleeType->isPointerTy()) {
	     DEBUG_MSG(errs() << "FS: called function returns a pointer\n");
	     prevcall = true;
	  }

	} else if (isa < BitCastInst > (I)) {
           // continue;
	  if (prevcall) {
	    prevcall = false;
	    BitCastInst *BCI = dyn_cast < BitCastInst > (I);
	    if (!freecalled) {
		if (!HAT (BCI, NULL)) {
		  DEBUG_MSG(errs() <<
		    "FS: No address taken of value and no calls to free in function, no need to register this particular pointer\n");
		  continue;
		}
	    }

	    Module *M = F.getParent ();
            const DataLayout *DL = &(M->getDataLayout());
	    Constant *regptr_def = M->getOrInsertFunction (UAFFUNC,
							   Type::getVoidTy (M->getContext ()),
							    DL->getIntPtrType(M->getContext(), 0),
                                                            DL->getIntPtrType(M->getContext(), 0),
							   NULL);
	    Function *regptr = cast < Function > (regptr_def);
	    regptr->setCallingConv (CallingConv::C);


	    std::vector < Value * >Args;
	    DEBUG_MSG(errs() << "FS (call): adding registerptr for" << *I << "\n");
            
            Instruction *insertBeforeInstruction = I;
            BasicBlock::iterator nextIt(I);
            ++nextIt;
            insertBeforeInstruction = &*nextIt;
            IRBuilder<> B(insertBeforeInstruction);
            
            Value *BC = I; 
            Value *BC_operand = I->getOperand(0);
            if (BC_operand->getType()->getTypeID() == Type::DoubleTyID) {
                continue;
            }

            if (!(I->getType()->getScalarType()->isPointerTy())) {
                //errs() << "Not a Pointer Type : " << *I << "\n";
                Value *I_BC = I;
                switch (I->getType()->getTypeID()) {
                    case Type::FloatTyID:
                    case Type::DoubleTyID:
                        continue;
                    case Type::IntegerTyID:
                        break;
                    default:
                        I_BC = B.CreateBitCast(I, B.getInt64Ty());
                        break;
                }
                //if (!(I->getType()->getScalarType()->isIntegerTy()) && ) {
                //    I_BC = B.CreateBitCast(I, B.getInt64Ty()); 
                //}
                BC = B.CreateIntToPtr(I_BC, B.getInt64Ty()->getPointerTo());
            }
            
            //errs() << "Instruction : " << *BC << "\n"; 
	    //Value *Idx[1];
	    //Idx[0] = Constant::getNullValue(Type::getInt64Ty(M->getContext()));
	    //Value *GEP = B.CreateGEP(BC, Idx);
	    Value *GEP = BC;
	    //GEP->insertAfter(I);
	    DEBUG_MSG(errs() << "FS (call): adding GEP:" << *GEP << "\n");
            
	    Value *cast = B.CreatePointerCast (GEP, DL->getIntPtrType(M->getContext(), 0));
            
            /* Metaalloc: Need to read value of the pointer operand */
            Value *load_cast = B.CreateIntToPtr(cast, B.getInt64Ty()->getPointerTo());
            DEBUG_MSG(errs() << "FS (call): Load pointer: " << *load_cast << "\n");
            Value *LI = B.CreateLoad(load_cast);
            DEBUG_MSG(errs() << "FS (call): Load " << *LI << "\n");
	    //cast->insertAfter (GEP);
	    DEBUG_MSG(errs() << "FS (call): adding cast:" << *cast << "\n");
            
            
	    Args.push_back (B.CreatePtrToInt(cast, DL->getIntPtrType(M->getContext(), 0)));
            Args.push_back(B.CreatePtrToInt(LI, DL->getIntPtrType(M->getContext(), 0)));

	    B.CreateCall (regptr, Args, "");
	    //regptr_call->insertAfter (cast);

	    FreeSentryRegptrCall++;
	    FreeSentryRegptr++;
	    changed = true;
	  }

	} else if (isa < StoreInst > (I)) {
        //:  continue;
	  prevcall = false;
	  DEBUG_MSG(errs() << "FS: Store instruction: " << *I << "\n");
	  StoreInst *SI = dyn_cast < StoreInst > (I);
	  DEBUG_MSG(errs() << "FS: Pointer operand: " << *SI->getPointerOperand () << "\n");
	  DEBUG_MSG(errs() << "FS: Pointer type: " << *SI->
	    getPointerOperand ()->getType () << " (is pointer: " << SI->
	    getPointerOperand ()->getType ()->isPointerTy () << ")\n");
	  DEBUG_MSG(errs() << "FS: Value operand: " << *SI->getValueOperand () << "\n");
	  DEBUG_MSG(errs() << "FS: Value type: " << *SI->
	    getValueOperand ()->getType () << " (is pointer: " << SI->
	    getValueOperand ()->getType ()->isPointerTy () << ")\n");

	  Value *valop = SI->getValueOperand ();

	  if (SI->getValueOperand ()->getType ()->isPointerTy ()) {
	    if (isa < GetElementPtrInst > (valop)) {
	      DEBUG_MSG(errs() << "FS: Value is a getelptrinst\n");
	      GetElementPtrInst *GI = dyn_cast < GetElementPtrInst > (valop);
	      DEBUG_MSG(errs() << "FS: Getelptr, pointer: " <<
		*GI->getPointerOperand() << "\n");
	      if (isa < LoadInst > (GI->getPointerOperand ())) {
		LoadInst *LI =
		  dyn_cast < LoadInst > (GI->getPointerOperand ());
		DEBUG_MSG(errs() << "FS: Found loadinst: " <<
		  *LI->getPointerOperand () << "\n");
		if (SI->getPointerOperand () == LI->getPointerOperand ()) {
		  DEBUG_MSG(errs() <<
		    "FS: Pointer loaded, added to and then stored again, ignore\n");
		  continue;
		}
	      }
	    }

	    if (!freecalled) {
	      if (isa < Instruction > (valop)) {
		Instruction *AI = dyn_cast < Instruction > (valop);
		if (!HAT (AI, NULL)) {
		  DEBUG_MSG(errs() <<
		    "FS: No address taken of value and no calls to free in function, no need to register this particular pointer\n");
		  continue;
		}
	      }
	    }

        //if (strstr(F.getName().str().c_str(), "Perl_ck_split") != NULL) {
        //    continue;
        //}
	    //Module *M = F.getParent ();
            //const DataLayout *DL = &(M->getDataLayout());
            //Constant *regptr_def = M->getOrInsertFunction (UAFFUNC,
            //                            Type::getVoidTy (M->getContext ()),
            //                            DL->getIntPtrType(M->getContext(), 0),
            //                            DL->getIntPtrType(M->getContext(), 0),
            //                            NULL);

	    //Function *regptr = cast < Function > (regptr_def);
	    //regptr->setCallingConv (CallingConv::C);
            
            Instruction *insertBeforeInstruction = I;
            BasicBlock::iterator nextIt(I);
            ++nextIt;
            insertBeforeInstruction = &*nextIt;
            IRBuilder<> B(insertBeforeInstruction);
           
            /* This check is required for the GCC */ 
            Value *object_addr = SI->getValueOperand();
            /* TODO: Just to check whether it prevents core dump */
            if (isa<ConstantPointerNull>(object_addr)) {
                continue;
            }
            Value *obj_bound_addr = getBoundedAddr(object_addr); 
	    
            std::vector < Value * >Args;
	    //Value *cast =
	     // B.CreatePointerCast (SI->getPointerOperand (), DL->getIntPtrType(M->getContext(), 0));
           
            //Value *LI = B.CreateLoad(cast);
            //Value *LI = B.CreateLoad(SI->getPointerOperand());
	    DEBUG_MSG(errs() <<
		    "FS: adding registerptr for" << *SI->getPointerOperand() << "\n");

	    Args.push_back (B.CreatePtrToInt(SI->getPointerOperand(), IntPtrTy));
            Args.push_back(B.CreatePtrToInt(obj_bound_addr, IntPtrTy));

            B.CreateCall (RegisterPtrFunc, Args, "");
	    FreeSentryRegptrStore++;
	    FreeSentryRegptr++;

	    changed = true;
	  }
	} else {
	     prevcall = false;
	}
      }
      return changed;
    }

    void getAnalysisUsage (AnalysisUsage & AU) const override {
      AU.setPreservesAll ();
//      AU.addRequiredID(DemoteRegisterToMemoryID);
//      AU.addRequiredID
    }


  };



  struct FreeSentryLoop:public LoopPass, FreeSentryShared {
    static char ID;		// Pass identification, replacement for typeid
      FreeSentryLoop ():LoopPass (ID) {
	//initializeFreeSentryLoopPass(*PassRegistry::getPassRegistry());
      }
      FreeSentryLoop (bool setflag): LoopPass(ID) {flag = setflag; FreeSentryLoop();}

    LoopInfo *LI;
    DominatorTree *DT;
    bool flag = false;


    using llvm::Pass::doInitialization;
    bool doInitialization (Loop * L, LPPassManager & LPM) override {
      DEBUG_MSG(errs() << "FSL: Loop init: << " << FSGIFile << "\n");
	loadcallgraph(FSGIFile);
	loadcallgraph(FSGSFile);
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
		} else if (callsfree(funcname)) {
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
      bool freecall = false;
      bool regptrcall = false;
      bool changed = false;

      BasicBlock *Header = L->getHeader();
      Function *F = Header->getParent();

      /*if (!(F->hasFnAttribute(Attribute::FreeSentry) || flag)) {
	return false;
      }*/

      /*if (F->hasFnAttribute(Attribute::NoFreeSentry))
	return false;
        */
        if (ISMETADATAFUNC(F->getName().str().c_str()))
            return false; 

      DEBUG_MSG(errs() << "FSL: Running on loop\n");

      LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

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

      for (Loop::block_iterator I = L->block_begin (), E = L->block_end ();
	   I != E; ++I) {
	BasicBlock *BB = *I;
	if (LI->getLoopFor (BB) == L) {
	  for (BasicBlock::iterator I = BB->begin (), E = BB->end ();
	       (I != E); ++I) {
	    Instruction *inst = &*I;
	    if (isa < CallInst > (inst)) {
	      CallInst *CI = dyn_cast < CallInst > (inst);
	      Function *F = CI->getCalledFunction ();
	      if (F) {
		StringRef funcname = F->getName ();
		DEBUG_MSG(errs() << "FSL: Function called: " << funcname << "\n");
		if (funcname == UAFFUNC) {
		  if (CastInst * cast =
		      dyn_cast < CastInst > (CI->getArgOperand (0))) {
		    Value *arg = cast->getOperand (0);
		    DEBUG_MSG(errs() << "FSL: Arg: " << *arg << "\n");
		    if (Instruction * AI = dyn_cast < Instruction > (arg)) {
		      if (HAT (AI, L)) {
			DEBUG_MSG(errs() << "FSL: Address taken: " << "\n");
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
		      BasicBlock *EXBB = L->getExitBlock ();

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
                        IRBuilder<> B(insertBeforeInstruction);

			std::vector < Value * >Args;
			Value *newcast = B.CreatePointerCast (arg, DL->getIntPtrType(M->getContext(), 0));
                        Value *LI = B.CreateLoad(newcast);

			Args.push_back (B.CreatePtrToInt(newcast, DL->getIntPtrType(M->getContext(), 0)));
                        Args.push_back(B.CreatePtrToInt(LI, DL->getIntPtrType(M->getContext(), 0)));
                        B.CreateCall (regptr, Args, "");

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
      AU.addRequired <FreeSentry> ();
    }
  };


  typedef std::set < String > FSSet;
  typedef std::ofstream ofstream;

  struct FSGraph:public FunctionPass {
    static char ID;
      FSGraph ():FunctionPass (ID) {}
      FSGraph (bool setflag): FunctionPass(ID) {flag = setflag; FSGraph();}

    bool flag = false;
    FSSet *getOrInsertFunction (const Function * F) {
      FSSet & fcalls = FCallMap[F->getName ()];

      return &fcalls;
    }

    bool doInitialization (Module & M) {
      //String ErrInfo = "";
      String filename = FSGOFile;
        std::error_code EC;
      outfile =
	new raw_fd_ostream (filename.c_str(), EC, sys::fs::F_Append);
      return false;
    }


    void addToCallGraph (Function * F) {
      FSSet *fcalls = getOrInsertFunction (F);

      for (Function::iterator BB = F->begin (), BBE = F->end ();
	   BB != BBE; ++BB)
	for (BasicBlock::iterator II = BB->begin (), IE = BB->end ();
	     II != IE; ++II) {
	  CallSite CS (cast < Value > (II));
	  if (CS) {
	    const Function *Callee = CS.getCalledFunction ();
	    if (!Callee) {
	      DEBUG_MSG(errs() << "FSG: Indirect call");
	    }
	    else if (!Callee->isIntrinsic ()) {
	      fcalls->insert (Callee->getName ());
	    }
	  }
	}
    }

    void dumpFunction (Function * F) {

      FSSet *fcalls = getOrInsertFunction (F);
      *outfile << F->getName () << ": ";
      for (FSSet::iterator I = fcalls->begin (), IE = fcalls->end ();
	   I != IE; ++I) {
	String func = *I;
	*outfile << func << " ";
      }
      *outfile << "\n";
    }


    bool runOnFunction (Function & F) override {

      /*if (!(F.hasFnAttribute(Attribute::FreeSentry) || flag)) {
	return false;
      }

      if (F.hasFnAttribute(Attribute::NoFreeSentry))
	return false;*/
      if (ISMETADATAFUNC(F.getName().str().c_str())) {
        return false;
      }

      DEBUG_MSG(errs() << "FSG: " << F.getName () << "\n");

      addToCallGraph (&F);

      dumpFunction (&F);

      return false;
    }


    mutable StringMap < FSSet > FCallMap;
    raw_fd_ostream *outfile;

    // We don't modify the program, so we preserve all analyses.
    void getAnalysisUsage (AnalysisUsage & AU) const override {
      AU.setPreservesAll ();
    }
  };

}

char FreeSentry::ID = 0;
//char FreeSentryLoop::ID = 0;
char FSGraph::ID = 0;


static RegisterPass <FreeSentry> X("FreeSentry", "UAF Protection");
static RegisterPass <FSGraph> Z("FSGraph", "FreeSentry Call Graph");
//static RegisterPass <FreeSentryLoop> Y("FreeSentryLoop", "UAF Protection Loop optimization");
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
