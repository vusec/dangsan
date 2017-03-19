/*
 * GlobalTracker.cpp
 *
 *  Created on: Nov 10, 2015
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
#include <metadata.h>

using namespace llvm;

struct GlobalTracker : public ModulePass {
    static char ID;
    bool initialized;
    Module *M;
    const DataLayout *DL;
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
    Constant *DangInitFunc;
    //declare void @initialize_global_metadata()
    Function *GlobalInitFunc;

    GlobalTracker() : ModulePass(ID) { initialized = false; }

    Instruction * getGlobalInitReturn() {
        for (auto &bb : *GlobalInitFunc)
            if (isa<ReturnInst>(bb.getTerminator()))
                return bb.getTerminator();
        return NULL;
    }

    virtual bool runOnModule(Module &Mod) {
        if (!initialized)
            doInitialization(&Mod);

        std::set<Value*> metaDataInserted;
        
        Instruction *globalInitReturn = getGlobalInitReturn();
        if (!globalInitReturn)
            report_fatal_error("error; either initialize_global_metadata "
                "has no return instruction or you forgot to link in "
                "libmetadata.a");

        IRBuilder<> B(globalInitReturn);

        for (auto& global: M->globals()) {
            GlobalValue *G = &global;

            if (metaDataInserted.count(G))
                continue;

            if (G->getName() == "llvm.global_ctors" ||
		G->getName() == "llvm.global_dtors" ||
		G->getName() == "llvm.global.annotations" ||
		G->getName() == "llvm.used") {
                continue;
	    }

            if (G->getAlignment() < 8)
                dyn_cast<GlobalObject>(G)->setAlignment(8);

            unsigned long elementSize = DL->getTypeAllocSize(G->getType()->getPointerElementType());
            Value *size = ConstantInt::get(IntPtrTy, elementSize);

            Value *ptr = B.CreatePtrToInt(G, IntPtrTy);
            // Reset metadata when none is desired
            // Uses metaset
            if (!DeepMetadata) {
                std::vector<Value *> callParams;
                callParams.push_back(ptr);
                callParams.push_back(size);
                callParams.push_back(ConstantInt::get(IntMetaTy, 0));
                if (!FixedCompression) {
                    callParams.push_back(ConstantInt::get(IntPtrTy, GLOBALALIGN));
                }
                //B.CreateCall(MetasetFunc, callParams);
           } else {
                // Create and initialize the metadata object
                GlobalVariable *metaData = new GlobalVariable(*M, MetaDataTy, false, 
                                    GlobalVariable::LinkageTypes::InternalLinkage,
                                    nullptr, "MetaData_" + G->getName());
                metaDataInserted.insert(metaData);
                std::vector<Constant *> globalMembers;
                for (unsigned long i = 0; i < (DeepMetadataBytes / sizeof(unsigned long)); ++i)
                    globalMembers.push_back(ConstantInt::get(IntPtrTy, 0, 0));
                Constant *globalInitializer = ConstantStruct::get(MetaDataTy, globalMembers);
                metaData->setInitializer(globalInitializer);
                // Set desired metadata
                // Uses metaset
                std::vector<Value *> callParams;
                callParams.push_back(ptr);
                callParams.push_back(size);
                callParams.push_back(B.CreatePtrToInt(metaData, IntPtrTy));
                if (!FixedCompression) {
                    callParams.push_back(ConstantInt::get(IntPtrTy, GLOBALALIGN));
                }
                B.CreateCall(MetasetFunc, callParams);
		/* Dang : We have to add call to dang_init_object() to initialize
 		 * this global object.
 		 */
		Value *ptr = B.CreatePtrToInt(G, IntPtrTy);
		std::vector<Value *> initParams;
		initParams.push_back(ptr);
		B.CreateCall(DangInitFunc, initParams);
           }
        }
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
            //declare i64 @metaset(i64, i64, iM, i64)
            std::string functionName = "metaset_alignment_safe_" + std::to_string(MetadataBytes);
            MetasetFunc = M->getOrInsertFunction(functionName, IntPtrTy,
                IntPtrTy, IntPtrTy, IntMetaTy, IntPtrTy, NULL);
        } else {
            //declare i64 @metaset_fixed(i64, i64, iM)
            std::string functionName = "metaset_fixed_" + std::to_string(MetadataBytes);
            MetasetFunc = M->getOrInsertFunction(functionName, IntPtrTy,
                IntPtrTy, IntPtrTy, IntMetaTy, NULL);
        }
        //declare void @initialize_global_metadata()
        GlobalInitFunc = (Function*)M->getOrInsertFunction("initialize_global_metadata", VoidTy, NULL);
	std::string functionName = "dang_init_object";	
	DangInitFunc = M->getOrInsertFunction(functionName, VoidTy, IntPtrTy, NULL);
        initialized = true;
        
        return false;
    }

};

char GlobalTracker::ID = 0;
static RegisterPass<GlobalTracker> X("globaltracker", "Global Tracker Pass", true, false);




