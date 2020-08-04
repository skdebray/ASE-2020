/*
* File: edge.h
* Author: Jesse Bartels
* Description: Header file for all edge related functions
*
*/
#ifndef _EDGE_H_
#define _EDGE_H_

#include "cfg.h"
#include "hashtable.h"
#include "Reader.h"


// Function Name: newEdge(cfgInstruction *prevIns, cfgInstruction *currIns)
// Purpose: Modular function to create edges (so we don't duplicate code)
// Parameters: The current instruction, previous instruction.
// Returns: Pointer to a new edge if malloc is successful
// Exceptions/Errors from library calls: Alloc can crash if no more memory
Edge* newEdge(cfgInstruction *prevIns, cfgInstruction *currIns, cfgState *cfgStructArg);

// Function Name: newEdge(edge_type, Bbl *from, Bbl *to)
// Purpose: Createan edge with the given type
// Parameters: Edge type, block from and block to
// Returns: Pointer to a new edge tuple if malloc is successful
// Exceptions/Errors from library calls: Alloc can throw an error
edgeTuple *addNewEdgeWithType(edge_type type, Bbl *from, Bbl *to, cfgState *cfgStructArg);

void addToEdgePhaseList(Edge *edg, int phaseID);

int edgPartOfPhase(Edge *edg, int phaseID);

// Function: addEdge(Edge **listOfEdges, Edge *newEdge)
// Purpose: Add an edge to a linked list of successor/predecessor edges
// Parameters: The edge and list of edges in which to add the edge in
// returns: Nothing
void addEdge(Edge **listOfEdges, Edge *newEdge);

// Function: peekEdgeType(cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg)
// Purpose: Find out the edge type between two instruction
// Parameters: prev/current ins, cfgState
// returns: edge type that would be formed between two ins
edge_type peekEdgeType(cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg);

// Function Name: createEdge(cfgInstruction *prevIns, cfgInstruction *currIns)
// Purpose: Function that looks at the previous/current instruction in byte form to determine what kind of edge must be
//          created between curIns and prevIns. In all cases prevIns is the instruction that makes the jump/call/return.
//          currIns is the first instruction right after a jump/call/ret.
// Parameters: The current instruction, previous instruction.
// Returns: Nothing, creates edge between the necessary blocks
// Exceptions/Errors from library calls: Alloc will crash if we do not have enough memory
void createEdge(cfgInstruction *prevIns, cfgInstruction *currIns, cfgState *cfgStructArg);

// Function: searchThroughSyccessors(Bbl *from, Bbl *to)
// Purpose: Walk through the successors of the "from" block and determine if the "to" block is a successor
// Parameters: The block whose successor edges we are searching through and the target of the search
// returns: Edge between blocks or NULL if none exists
Edge *searchThroughSuccessors(Bbl *from, Bbl *to);

Edge *searchThroughSuccessorsWithType(edge_type type, Bbl *from, Bbl *to);

Edge *searchThroughSuccessorsForDynamic(Bbl *from, Bbl *to);

// Function: searchThroughPredecessors(Bbl *from, Bbl *to)
// Purpose: Walk through the predecessors of the "from" block and determine if the "to" block is a successor
// Parameters: The block whose predecessors edges we are searching through and the target of the search
// returns: Edge between blocks or NULL if none exists
Edge *searchThroughPredecessors(Bbl *from, Bbl *to);

// Function name: ExistingEdge(Bbl *prevBbl, Bbl *currBbl, cfgInstruction *prevIns, cfgInstruction *currIns)
// Purpose: Find if there is an existing edge between two blocks (look through successor's of block)
// Parameters: The current instruction, previous instruction. and their respective blocks
// Returns: The edge if it exists, null otherwise
Edge* existingEdge(Bbl *prevBbl, Bbl *currBbl, cfgInstruction *prevIns, cfgInstruction *currIns, DisassemblerState *disState);

// Function name: removeEdge(Edge* remEdge);
// Purpose: remove an edge from a CFG. This is useful in merging when we encounter pseudo-blocks
// Parameters: a double pointer to the edge list containing the edge to be removed, and a pointer to the edge to be removed.
// Returns: void.
void removeEdge(Edge** edgeList, Edge* remEdge);

// Function name: freeEdgeList(Edge* remEdge);
// Purpose: remove a list of edges. This is useful when deleting blocks to prevent memory leaks
// Parameters: a pointer to the head of the edge list to be removed.
// Returns: void.
void freeEdgeList(Edge* remEdge);
#endif
