#ifndef __TAINT_STATE_H_
#define __TAINT_STATE_H_

#include "LabelStore.h"
#include <Reader.h>
#include <stdint.h>

//typedef void * MemoryTaint;
//typedef void * RegisterTaint;

typedef ByteMemTaint *MemoryTaint;
typedef ByteRegState *RegisterTaint;

struct TaintState_t {
    MemoryTaint memTaint;
    RegisterTaint regTaint;
    LabelStoreState *labelState;
    ReaderState *readerState;

    uint64_t (*opHandlers[XED_ICLASS_LAST]) (struct TaintState_t *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc);

    void (*freeMemTaint) (MemoryTaint);
    void (*resetMemLabels) (MemoryTaint);
    void (*setAllMemTaint) (MemoryTaint, uint64_t, uint32_t, uint64_t);
    void (*setMemTaint) (MemoryTaint, uint64_t, uint32_t, const uint64_t *);
    void (*getMemTaint) (MemoryTaint, uint64_t, uint32_t, uint64_t *);
    void (*addTaintToMem) (MemoryTaint, LabelStoreState *, uint64_t, uint32_t, uint64_t);
    void (*freeRegTaint) (RegisterTaint);
    void (*setAllRegTaint) (RegisterTaint, LynxReg, uint32_t, uint64_t);
    void (*setRegTaint) (RegisterTaint, LynxReg, uint32_t, const uint64_t *);
    const uint64_t *(*getRegTaint) (RegisterTaint, LynxReg, uint32_t);
    void (*addTaintToReg) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t);
    uint64_t (*getCombinedRegTaint) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t);
};

#endif
