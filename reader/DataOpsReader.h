/*
* File: TrdAscii.h
* Author: David Raicher
* Description: This file contains all of the backend created for the Ascii format of the trace reader.
* Note that most of the outward facing functions from this file are desposited in function pointers inside
* of Trd.c.  Future formats will likely want to follow the same process.
*/

#ifndef _TRD_INFO_SEL_H_
#define _TRD_INFO_SEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <LynxReg.h>
#include <TraceFileHeader.h>
#include "ReaderUtils.h"
#include "Reader.h"
#include "ShadowRegisters.h"
#include "ShadowMemory.h"
#include "XedDisassembler.h"

typedef struct DataOpsState_t DataOpsState;

DataOpsState *initDataOps(FileHeader *fileHeader, SectionEntry *sectionTable, FILE *traceFile, ExInfo *exInfo);

void setupDataOpsInitState(DataOpsState *state, MemState *memState, RegState *regState);

uint32_t readDataOpsEvent(DataOpsState *state, MemState *memState, RegState *regState, DisassemblerState *disState, ReaderEvent *nextEvent, uint64_t curId, InsInfo *curInfo);

const char *fetchDataOpsStr(DataOpsState *state, uint32_t id);

uint32_t findDataOpsStr(DataOpsState *state, const char *str);

uint8_t hasDataOpsField(DataOpsState *s, uint32_t query);

void freeDataOps(DataOpsState *state);

uint32_t dataOpsStrTableSize(DataOpsState *state);

#endif
