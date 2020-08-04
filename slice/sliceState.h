#ifndef __SLICE_STATE_H_
#define __SLICE_STATE_H_

#include <set>
#include <map>
#include "slice.h"
extern "C" {
  #include <Reader.h>
  #include <Taint.h>
  #include <cfg.h>
  #include <cfgAPI.h>
  #include <cfgState.h>
}
#include <cstdint>

using namespace std;

struct SliceState_t {
  struct Action **listOfActions; // Array of actions (can be swapped out for something more efficient)
  //struct blockSlice *head, *tail;     // List of blocks for slice
  uint64_t numActions;
  ReaderState *readerState;
  TaintState *taintState;
  //hashtable *htableslice;
  map <cfgInstruction *, struct blockSlice *> htableslice; 
  uint64_t numMarked;
  uint64_t numVisited;
  set<Action *> marked;
  set<Action *> visited;
  set<Action *> contributing;
  set<struct blockSlice *> removableBlocks;
  set<cfgInstruction *> removableCFGInstructions;
  set<cfgInstruction *> saveableCFGInstructions;
};

#endif    /* __SLICE_STATE_H_ */

