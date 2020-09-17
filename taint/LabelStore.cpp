
#include <set>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <algorithm>
#include <iterator>
#include <cassert>
#include "LabelStore.h"

using namespace std;

struct SetHash {
    inline std::size_t operator()(const set<uint64_t> *t) const {
        uint64_t hash = 0;
        int i = 1;
        for(auto it = t->cbegin(); it != t->cend(); it++) {
            hash += *it * i;
            i++;
        }
        return hash;
    }
};

struct SetEquality {
    inline bool operator()(const set<uint64_t> *t1, const set<uint64_t> *t2) const {
        return *t1 == *t2;
    }
};

struct LabelStoreState_t {
    unordered_map<set<uint64_t> *, uint64_t, SetHash, SetEquality> def;
    unordered_map<uint64_t, set<uint64_t> *> uses;
    uint64_t next;
};

uint64_t getLabelArrSize(LabelStoreState *state) {
    return state->next;
}

uint8_t *getLabelArr(LabelStoreState *state) {
    return new uint8_t[state->next]();
}

void freeUnnecessary(LabelStoreState *state, uint8_t *alive) {
    for(uint64_t i = 1; i < state->next; i++) {
        if(!alive[i]) {
            auto it = state->uses.find(i);
            if(it != state->uses.end() && it->second->size() > 1) {
                set<uint64_t> *curSet = it->second;
                state->uses.erase(i);
                state->def.erase(curSet);
                delete curSet;
            }
        }
    }
}

uint8_t *condenseLabels(LabelStoreState *state, uint8_t *labelArr) {
    vector<uint64_t> labels;

    for(uint64_t i = 1; i < state->next; i++) {
        if(labelArr[i]) {
            labels.push_back(i);
            set<uint64_t> *curSet = state->uses[i];
            for(auto it = curSet->begin(); it != curSet->end(); it++) {
                printf("%llx ", (unsigned long long) *it);
            }
            printf("\n");
        }
    }

    printf("==========\n");

    set<uint64_t> subtract;
    set<uint64_t> condensed;
    while(!labels.empty()) {
        set<uint64_t> curSet;
        uint64_t l1 = labels.back();

        if(subtract.empty()) {
            curSet = *state->uses[l1];
        }
        else {
            set<uint64_t> &s1 = *state->uses[l1];
            set_difference(s1.begin(), s1.end(), subtract.begin(), subtract.end(), inserter(curSet, curSet.begin()));

            if(curSet.empty()) {
                labels.pop_back();
                continue;
            }
        }

        for(uint64_t l2 : labels) {
            set<uint64_t> newSet;

            set<uint64_t> &s2 = *state->uses[l2];

            set_intersection(s2.begin(), s2.end(), curSet.begin(), curSet.end(), inserter(newSet, newSet.begin()));
            if(!newSet.empty()) {
                curSet = newSet;
            }
        }
    
        auto result = state->def.find(&curSet);

        if(result == state->def.end()) {
            set<uint64_t> *newSet = new set<uint64_t>(curSet);
            state->def[newSet] = state->next;
            state->uses[state->next] = newSet;
            condensed.insert(state->next);
            state->next++;
        }
        else {
            condensed.insert(result->second);
        }

        for(uint64_t label : curSet) {
            printf("%llx ", (unsigned long long) label);
        }
        printf("\n\n");

        subtract.insert(curSet.begin(), curSet.end());
    }

    uint8_t *condensedLabels = getLabelArr(state);
    for(auto i = condensed.begin(); i != condensed.end(); i++) {
        condensedLabels[*i] = 1;
    }

    return condensedLabels;
}

LabelStoreState *initLabelStore() {
    LabelStoreState *state = new LabelStoreState();
    state->next = 1;
    return state;
}

void freeLabelStore(LabelStoreState *state) {
    for(uint64_t i = 1; i < state->next; i++) {
        auto it = state->uses.find(i);
        if(it != state->uses.end()) {
            set<uint64_t> *curSet = it->second;
            state->uses.erase(i);
            state->def.erase(curSet);
            delete curSet;
        }
    }
    delete state;
}


uint64_t createNewLabel(LabelStoreState *state) {
    set<uint64_t> *newSet = new set<uint64_t>();
    newSet->insert(state->next);
    state->def[newSet] = state->next;
    state->uses[state->next] = newSet;
    return state->next++;
}

uint64_t combineAllLabels(LabelStoreState *state, uint64_t init, uint64_t *labels, int size) {
    uint64_t prevLabel = 0;
    uint64_t combined = init;

    int i;
    for(i = 0; i < size; i++) {
        if(labels[i] != combined && labels[i] != prevLabel) {
            prevLabel = labels[i];
            combined = combineLabels(state, combined, labels[i]);
        }   
    }   

    return combined;
}


/*
 * applyLabel() -- write the taint specified by apply to size positions of the taint
 * array labels.
 */
void applyLabel(LabelStoreState *state, uint64_t apply, uint64_t *labels, int size) {
    uint64_t prevLabel = 0;
    uint64_t prevCombination = apply;

    int i;
    for(i = 0; i < size; i++) {
        if(labels[i] != apply) {
            if(labels[i] == prevLabel) {
                labels[i] = prevCombination;
            }
            else {
                prevLabel = labels[i];
                labels[i] = combineLabels(state, apply, labels[i]);
                prevCombination = labels[i];
            }
        }
    }
}


/*
 * applyAndCombineLabels() -- combines the taint label apply with the label at 
 * each index of the array labels, then writes the resulting taint label to
 * that index.  In the resulting taint array each index in the array will still
 * be "marked" with its original label and the specified taint label.  It is
 * essentially equivalent to iterating over each index in the array, combining 
 * the desired label with the label at the current index, then applying that 
 * label to that index.
 */
uint64_t applyAndCombineLabels(LabelStoreState *state, uint64_t apply, uint64_t *labels, int size) {
    uint64_t prevLabel = 0;
    uint64_t prevCombination = apply;
    uint64_t combined = apply;

    int i;
    for(i = 0; i < size; i++) {
        if(labels[i] != apply) {
            if(labels[i] == prevLabel) {
                labels[i] = prevCombination;
            }
            else {
                prevLabel = labels[i];
                if(combined == apply) {
                    labels[i] = combined = combineLabels(state, combined, labels[i]);
                }
                else {
                    combined = combineLabels(state, combined, labels[i]);
                    labels[i] = combineLabels(state, apply, labels[i]);
                }
                prevCombination = labels[i];
            }
        }
    }

    return combined;
}

uint8_t hasSequential(LabelStoreState *state, uint64_t world, uint64_t label) {
    auto worldSet = state->uses.find(world);

    label = intersectLabels(state, world, label);
    auto labelSet = state->uses.find(label);

    if(worldSet == state->uses.end() || labelSet == state->uses.end()) {
        return 0;
    }

    auto worldIt = worldSet->second->begin();
    int last = 0;
    int cur = 0;

    for(auto labelIt = labelSet->second->begin(); labelIt != labelSet->second->end(); labelIt++) {
        for(; worldIt != worldSet->second->end() && *worldIt != *labelIt; worldIt++, cur++);

        if(worldIt == worldSet->second->end()) {
            break;
        }

        if(cur == (last + 1)) {
            return 1;
        }

        last = cur;
    }

    return 0;
}

uint64_t combineLabels(LabelStoreState *state, uint64_t label1, uint64_t label2) {

    if(label1 == 0) {
        return label2;
    }

    if(label2 == 0) {
        return label1;
    }

    if(label1 == label2) {
        return label1;
    }

    auto it1 = state->uses.find(label1);
    auto it2 = state->uses.find(label2);

    if(it1 == state->uses.end()) {
        return label2;
    }

    if(it2 == state->uses.end()) {
        return label1;
    }

    set<uint64_t> *combinedUses = new set<uint64_t>();

    set_union(it1->second->begin(),
	      it1->second->end(),
	      it2->second->begin(),
	      it2->second->end(),
	      inserter(*combinedUses, combinedUses->begin()));

    if(combinedUses->size() == it1->second->size()) {
        delete combinedUses;
        return label1;
    }

    if(combinedUses->size() == it2->second->size()) {
        delete combinedUses;
        return label2;
    }

    auto result = state->def.find(combinedUses);

    if(result == state->def.end()) {
        state->def[combinedUses] = state->next;
        state->uses[state->next] = combinedUses;
        return state->next++;
    }

    delete combinedUses;
    return result->second;
}

uint64_t *returnSubLabels(LabelStoreState *state, uint64_t label) {
    if(label == 0) {
        return NULL;
    }

    auto it = state->uses.find(label);

    assert(it != state->uses.end());

    set<uint64_t> &base = *it->second;
    
    uint64_t *labels = (uint64_t *)malloc(base.size()*sizeof(uint64_t));
    int i = 0;
    for(uint64_t label : base) {
        labels[i] = label;
        i++;
    }

    return(labels);
}

void outputBaseLabels(LabelStoreState *state, uint64_t label) {
    if(label == 0) {
        printf("0\n");
        return;
    }

    auto it = state->uses.find(label);

    assert(it != state->uses.end());

    set<uint64_t> &base = *it->second;
    //printf("(%llu)", label);

    for(uint64_t label : base) {
        printf("%llx ", (unsigned long long) label);
    }
    printf("\n");
}

uint64_t getSize(LabelStoreState *state, uint64_t label) {
    auto result = state->uses.find(label);

    if(result != state->uses.end()) {
        return result->second->size();
    }

    return 0;
}

uint8_t includes(LabelStoreState *state, uint64_t label, uint64_t in) {
    auto labelIt = state->uses.find(label);
    auto inIt = state->uses.find(in);

    assert(labelIt != state->uses.end());
    assert(inIt != state->uses.end());

    return includes(inIt->second->begin(), inIt->second->end(), labelIt->second->begin(), labelIt->second->end());    
}

uint64_t subtract(LabelStoreState *state, uint64_t label, uint64_t from) {
    if(label == from) {
        return 0;
    }

    auto labelIt = state->uses.find(label);
    if(labelIt == state->uses.end()) {
        return from;
    }
    
    auto fromIt = state->uses.find(from);
    if(fromIt == state->uses.end()) {
        return 0;
    }
    
    set<uint64_t> *diffSet = new set<uint64_t>();
    set_difference(fromIt->second->begin(), fromIt->second->end(), labelIt->second->begin(), labelIt->second->end(), inserter(*diffSet, diffSet->begin()));

    if(diffSet->empty()) {
        return 0;
    }

    auto diffIt = state->def.find(diffSet);

    if(diffIt != state->def.end()) {
        delete diffSet;
        return diffIt->second;
    }

    state->def[diffSet] = state->next;
    state->uses[state->next] = diffSet;
    return state->next++;
}

uint64_t pop(LabelStoreState *state, uint64_t *label) {
    if(*label == 0) {
        return 0;
    }

    auto labelIt = state->uses.find(*label);

    assert(labelIt != state->uses.end());

    if(labelIt->second->size() == 1) {
        uint64_t tmp = *label;
        *label = 0;
        return tmp;
    }

    set<uint64_t> *newSet = new set<uint64_t>(++(labelIt->second->rbegin()), labelIt->second->rend());

    auto it = state->def.find(newSet);

    if(it != state->def.end()) {
        delete newSet;
        *label = it->second;

        return *labelIt->second->rbegin();
    }

    state->def[newSet] = state->next;
    state->uses[state->next] = newSet;
    *label = state->next++;

    return *labelIt->second->rbegin();
}

uint64_t getFirst(LabelStoreState *state, uint64_t label) {
    if(label == 0) {
        return 0;
    }

    auto labelIt = state->uses.find(label);

    if(labelIt == state->uses.end()) {
        return 0;
    }

    return *labelIt->second->begin();
}

uint64_t getLast(LabelStoreState *state, uint64_t label) {
    if(label == 0) {
        return 0;
    }

    auto labelIt = state->uses.find(label);

    if(labelIt == state->uses.end()) {
        return 0;
    }

    return *labelIt->second->rbegin();
}

uint64_t intersectLabels(LabelStoreState *state, uint64_t l1, uint64_t l2) {
    if(l1 == 0 || l2 == 0) {
        return 0;
    }

    auto l1It = state->uses.find(l1);
    auto l2It = state->uses.find(l2);

    if(l1It == state->uses.end() || l2It == state->uses.end()) {
        return 0;
    }

    set<uint64_t> intersect;

    set_intersection(l1It->second->begin(), l1It->second->end(), l2It->second->begin(), l2It->second->end(), inserter(intersect, intersect.begin()));

    if(intersect.empty()) {
        return 0;
    }

    auto result = state->def.find(&intersect);

    if(result == state->def.end()) {
        set<uint64_t> *newSet = new set<uint64_t>(intersect);

        state->def[newSet] = state->next;
        state->uses[state->next] = newSet;
        return state->next++;
    }

    return result->second;
}

uint8_t hasInRange(LabelStoreState *state, uint64_t label, uint64_t min, uint64_t max) {
    if(label == 0) {
        return 0;
    }

    auto it = state->uses.find(label);

    auto low = it->second->lower_bound(min);

    return low != it->second->end() && *low <= max;
}

