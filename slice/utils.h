/*
 * File: utils.h
 * Purpose: prototypes for functions defined in utils.cpp
 */

#ifndef __UTILS_H__
#define __UTILS_H__

void printUsage(char *program);
void parseCommandLine(int argc, char *argv[], SlicedriverState *driver_state);
void print_slice_instrs(SliceState *slice, SlicedriverState *driver_state);

#endif  /* __UTILS_H__ */
