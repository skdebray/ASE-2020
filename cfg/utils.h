/*
 * utils.h -- prototypes for function defined in utils.c
 */
#ifndef _UTILS_H_
#define _UTILS_H_

#include "cfg.h"
#include "Reader.h"
#include "cfgState.h"

void fetchInsInfoNoReaderState(DisassemblerState *disState, cfgInstruction *cfgIns, cfgInsInfo *info);

/*
 * report an error message on stderr
 */
void errmsg(const char *fmt, ...);

/*
 * alloc(n) -- allocate a memory block of n bytes and return a pointer to the block.
 */
void *alloc(int n);



#endif  /* _UTILS_H_ */
