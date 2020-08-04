#ifndef LABELSTORE_H_
#define LABELSTORE_H_

#ifdef __cplusplus
  extern "C" {
#endif

typedef struct LabelStoreState_t LabelStoreState;

void outputBaseLabels(LabelStoreState *state, uint64_t label);

uint64_t createNewLabel(LabelStoreState *state);

uint64_t combineLabels(LabelStoreState *state, uint64_t label1, uint64_t label2);

LabelStoreState *initLabelStore();

void freeLabelStore(LabelStoreState *state);

void freeUnnecessary(LabelStoreState *state, uint8_t *alive);

uint64_t getLabelArrSize(LabelStoreState *state);

uint8_t *getLabelArr(LabelStoreState *state);

uint8_t *condenseLabels(LabelStoreState *state, uint8_t *labels);

uint64_t applyAndCombineLabels(LabelStoreState *state, uint64_t apply, uint64_t *labels, int size);

void applyLabel(LabelStoreState *state, uint64_t apply, uint64_t *labels, int size);

uint64_t combineAllLabels(LabelStoreState *state, uint64_t init, uint64_t *labels, int size);

uint64_t *returnSubLabels(LabelStoreState *state, uint64_t label);

uint64_t getSize(LabelStoreState *state, uint64_t label);

uint8_t includes(LabelStoreState *state, uint64_t label, uint64_t in);

uint64_t subtract(LabelStoreState *state, uint64_t label, uint64_t from);

uint64_t pop(LabelStoreState *state, uint64_t *label);

uint64_t getFirst(LabelStoreState *state, uint64_t label);

uint64_t getLast(LabelStoreState *state, uint64_t label);

uint64_t intersectLabels(LabelStoreState *state, uint64_t l1, uint64_t l2);

uint8_t hasInRange(LabelStoreState *state, uint64_t label, uint64_t min, uint64_t max);

uint8_t hasSequential(LabelStoreState *state, uint64_t world, uint64_t label);

#ifdef __cplusplus
  }
#endif

#endif


