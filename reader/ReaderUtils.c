/**
 * This file includes all of the functions that may be useful for multiple trace formats to reduce the
 *  amount of duplicated code.
 **/

#include <stdio.h>
#include <stdlib.h>
#include "ReaderUtils.h"

/**
 * Function: throwError
 * Description: Prints the given message and then exits the program 
 * Output: None
 **/
void throwError(const char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}

/**
 * Function: throwWarning
 * Description: Prints the given message
 * Output: None
 **/
void throwWarning(const char *msg) {
    fprintf(stderr, "Warning: %s\n", msg);
}

/*************************************
 * Section Table Parsing
 *************************************/

/**
 * Function: readTraceEntry
 * Description: Creates a buffer for reading the trace from the trace entry in the section table
 * Output: Buf for the trace section
 **/
Buf *readTraceEntry(FILE *trace, SectionEntry * traceEntry) {
    if (traceEntry->type == TRACE_SECTION) {
        return createBuf(trace, traceEntry->offset, traceEntry->size);
    }

    return NULL;
}

/**
 * Function: readDataEntry
 * Description: Creates a buffer for reading the data from the data entry in the section table
 * Output: Buf for the data section
 **/
Buf *readDataEntry(FILE *trace, SectionEntry* dataEntry) {
    if (dataEntry->type == DATA_SECTION) {
        return createBuf(trace, dataEntry->offset, dataEntry->size);
    }
    return NULL;
}

/**
 * Function: readStrTableEntry
 * Description: From information in the string table entry of the section table, the function creates 
 *  and returns a string table
 * Output: The string table as a char *
 **/
char *readStrTableEntry(FILE *trace, SectionEntry *strTableEntry) {
    if (strTableEntry->type != STR_TABLE) {
        return NULL;
    }

    char *strTable = malloc(strTableEntry->size);

    if (strTable == NULL) {
        return NULL;
    }

    if (fseek(trace, strTableEntry->offset, SEEK_SET)) {
        free(strTable);
        return NULL;
    }

    if (fread(strTable, 1, strTableEntry->size, trace) != strTableEntry->size) {
        free(strTable);
        return NULL;
    }

    return strTable;
}

/**
 * Function: readTraceHeaderEntry
 * Description: Populates the given TraceHeader using the information given by the trace header entry in
 *  the section table
 * Side Effects: The TraceHeader is populated
 * Output: 1 if successful, 0 otherwise
 **/
int readTraceHeaderEntry(FILE *trace, SectionEntry *traceHeaderEntry, TraceHeader *traceHeader) {
    if (traceHeaderEntry->type != TRACE_HEADER_SECTION) {
        return 0;
    }

    if (fseek(trace, traceHeaderEntry->offset, SEEK_SET)) {
        return 0;
    }

    return fread(traceHeader, sizeof(TraceHeader), 1, trace);
}

/**
 * Function: readInfoSelHeaderEntry
 * Description: Populates the InfoSelHeader using the information given in the Information Selection entry
 *  of the section table.
 * Side Effects: The InfoSelHeader is populated
 * Output: 1 if successful, 0 otherwise
 **/
int readInfoSelHeaderEntry(FILE *trace, SectionEntry *infoSelHeaderEntry, InfoSelHeader *infoSelHeader) {
    if (infoSelHeaderEntry->type != INFO_SEL_HEADER) {
        return 0;
    }

    if (fseek(trace, infoSelHeaderEntry->offset, SEEK_SET)) {
        return 0;
    }

    return fread(infoSelHeader, sizeof(InfoSelHeader), 1, trace);
}

/**
 * Function: readSegmentLoads
 * Description: Populates the SegmentLoads entry from the trace into segments
 * Side Effects: segments is populated
 * Output: Number of segments loaded
 **/
int readSegmentLoads(FILE *trace, SectionEntry *segmentLoadsEntry, SegmentLoad **segments) {
    if (segmentLoadsEntry->type != SEGMENT_LOADS) {
        return -1;
    }

    if (fseek(trace, segmentLoadsEntry->offset, SEEK_SET)) {
        return -1;
    }

    *segments = malloc(segmentLoadsEntry->size);

    if(segments == NULL) {
        return -1;
    }

    if (fread(*segments, segmentLoadsEntry->size, 1, trace) != 1) {
        free(*segments);
        return -1;
    }
    
    return segmentLoadsEntry->size / sizeof(SegmentLoad);
}

/*****************************
 * parsing binary
 *****************************/

/**
 * Function: parseAddr32
 * Description: Parses a 32 bit address from buf 
 * Output: The 32 bit address as a uint64_t
 **/
uint64_t parseAddr32(uint8_t *buf) {
    return *((uint32_t *) buf);
}

/**
 * Function: parseAddr64
 * Description: Parses a 64 bit address from buf
 * Output: The 64 bit address
 **/
uint64_t parseAddr64(uint8_t *buf) {
    return *((uint64_t *) buf);
}
