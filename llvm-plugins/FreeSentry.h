
#ifndef _FREESENTRY_H_
#define _FREESENTRY_H_

using namespace llvm;

// Namespace
namespace llvm {
        Pass *createFreeSentry ();
        Pass *createFreeSentry (bool flag);
        Pass *createFreeSentryLoop ();
        Pass *createFreeSentryLoop (bool flag);
        Pass *createFSGraph ();
        Pass *createFSGraph (bool flag);
}
#endif
