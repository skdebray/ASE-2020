
#ifndef __READER_UTILS_H_
#define __READER_UTILS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <TraceFileHeader.h>
#include "Buf.h"

#define MAX_MEM_OP_SIZE 1024

typedef struct {
    // Number of threads used during execution
    uint32_t numThreads;
    // Information about segments loaded from the executable, see TraceFileHeader.h
    SegmentLoad *loadSegments;
    uint32_t numSegments;
} ExInfo;

typedef void * FormatState;

/*****************************
 * Error/Warnings
 *****************************/
void throwError(const char *msg);
void throwWarning(const char *msg);

/*****************************
 * Section Table Parsing
 *****************************/
Buf *readTraceEntry(FILE *trace, SectionEntry *traceEntry);
Buf *readDataEntry(FILE *trace, SectionEntry *dataEntry);
char *readStrTableEntry(FILE *trace, SectionEntry *strTableEntry);
int readTraceHeaderEntry(FILE *trace, SectionEntry *traceHeaderEntry, TraceHeader *traceHeader);
int readInfoSelHeaderEntry(FILE *trace, SectionEntry *infoSelHeaderEntry, InfoSelHeader *infoSelHeader); 
int readSegmentLoads(FILE *trace, SectionEntry *segmentLoadsEntry, SegmentLoad **segments);

/*****************************
 * Parsing Binary
 *****************************/
uint64_t parseAddr32(uint8_t *buf);
uint64_t parseAddr64(uint8_t *buf);

#endif
