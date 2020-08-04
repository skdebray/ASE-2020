#ifndef __VERIFY_OPS_H_
#define __VERIFY_OPS_H_

#include "Reader.h"
#include "ReaderUtils.h"

void initializeMarks(InsInfo *info);
int findDstReg(LynxReg reg, InsInfo *info);
int findSrcReg(LynxReg reg, InsInfo *info);
int findDstMem(uint64_t addr, uint16_t size, InsInfo *info);
int findSrcMem(uint64_t addr, uint16_t size, InsInfo *info);
void checkMarks(InsInfo *info, uint16_t selections);
int checkVals(const uint8_t *val1, const uint8_t *val2, uint16_t size, const char *msg);

#endif
