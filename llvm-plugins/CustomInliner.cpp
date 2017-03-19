/*
 * CustomInliner.cpp
 *
 *  Created on: Nov 10, 2015
 *      Author: haller
 */

#include <llvm/Transforms/IPO/InlinerPass.h>
#include <llvm/Analysis/InlineCost.h>

#include <metadata.h>

using namespace llvm;

struct CustomInliner : public Inliner {
    static char ID;

    CustomInliner() : Inliner(ID) {}

    InlineCost getInlineCost(CallSite CS) {
        Function *Callee = CS.getCalledFunction();
	const char *func_name = Callee->getName().str().c_str();
        if (Callee && ISMETADATAFUNC(func_name) && strncmp(func_name, "dang_", 5)) {
            return InlineCost::getAlways();
	}

        return InlineCost::getNever();
    }


};

char CustomInliner::ID = 0;
static RegisterPass<CustomInliner> X("custominline", "Custom Inliner Pass", true, false);




