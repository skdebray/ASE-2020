#ifndef __SLICE_H_
#define __SLICE_H_

#include "sliceState.h"
#include <libgen.h>
#include <xed-interface.h>
#include <xed-iclass-enum.h>
#include <vector>

extern "C" {
  #include <Reader.h>
  #include <Taint.h>
  #include <cfg.h>
  #include <cfgAPI.h>
  #include <cfgState.h>
}
typedef struct SliceState_t SliceState;

struct Action {
  uint64_t position;
  cfgInstruction *instruction;
  InsInfo *curInfo;
  struct blockSlice *bbl;
  uint8_t visited;
  uint8_t marked;
  uint8_t keep;
  vector<Action *> lastDefList;
  // Only present for push/pop
  uint64_t rspVal;
  const uint8_t *regVal;
  LynxReg reg;
  Action *bypass;
  Action *lastDefForRegOnPush;
  uint32_t tid;
};

struct infoTuple {
  InsInfo *storedInfo;
  uint64_t rspVal;
  const uint8_t *regVal;
  LynxReg reg;
  uint32_t tid;
  uint64_t event_pos;  // position of the event in the input trace
};

struct blockSlice {
  uint64_t start, end, id;
  set<uint64_t> actionsSet; 
};

/*
 * struct SlicedriverState: various values used by the slice driver
 */
typedef struct {
  char *trace;
  char *beginFn ;
  uint32_t beginId;
  char *endFn;
  uint32_t endId;
  char *traceFn;
  uint32_t traceId;
  int64_t targetTid;
  vector<uint32_t> includeSrcs;
  bool makeSuperblocks;
  bool compress;
  bool validate;
  bool keepReg;
  bool print_slice;
  char *outfile;
  FILE *outf;
  uint64_t sliceAddr;    // instruction address to slice from
  uint64_t slice_pos;    // instruction position in the trace to slice from
  ReaderState *rState;
  TaintState *backTaint;
  uint64_t numIns;

  /*
   * information for the slice computation -- filled in by build_action_list()
   */
  Action *slice_start_action;
  uint64_t num_actions;
  Action **listOfActions;
  vector<std::pair<cfgInstruction *, infoTuple *>> *insCollection;
} SlicedriverState;


SliceState *compute_slice(SlicedriverState *driver_state);
void startSlice(SliceState *slice, Action *action);
uint8_t isPushIns(xed_iclass_enum_t inst);
uint8_t isPopIns(xed_iclass_enum_t inst);
#endif
