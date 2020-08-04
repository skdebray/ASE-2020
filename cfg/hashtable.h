#ifndef _HASHTABLE_CFG_H_
#define _HASHTABLE_CFG_H_
/*
* File: hashtable.h
* Author: Jesse Bartels
* Description: Header file for the hashtable used in the cfg tool
*
*/
#include "Reader.h"
#include "cfg.h"

// Struct for our key_value pairs in our buckets. 
// Note we are doing linked hashing
typedef struct key_value{
	uint64_t key;
	void *val;
	struct key_value *next;
} key_val; 

// Struct for our hashtable
typedef struct Hashtable{
	uint64_t size;
	struct key_value **table;
        int ins;
        int col;
        int maxD;
} hashtable;

// Global hashtable used by cfg tool
extern hashtable *htablecfg;

void union_cnt(hashtable *one, hashtable *two);

// Function Name: create_table
// Purpose: Create a hash table of HT_SIZE size.
// Returns: 1(true) if the instruction passed in is a pop instruction, 0 otherwise
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory left for allocation
// Parameters: None
hashtable *create_table();

// Function Name: gen_hash
// Purpose: Generate hash value to produce mapping from bject to slot in table
// Returns: Hash value generated
// Exceptions/Errors from library calls: None
// Parameters: The table and the address from which to generate an index
uint64_t gen_hash(hashtable *table, uint64_t lbaddress);

// Function Name: new_key_val
// Purpose: Create a new key/value pair based on the passed in arguments.
//          Note: key = address, value = pointer to instruction with that address 
// Returns: Key/value pair if alloc succeeds
// Exceptions/Errors from library calls: Alloc can fail if there is not enough memory
// Parameters: The key (the instruction address) and the cfgInstruction to make a new key_val struct with
key_val *new_key_val(uint64_t key, void *val);

// Function Name: put_key_val
// Purpose: Function to put an instruction to the hash tab;e
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The key, table, and cfgInstruction to add to.
void put_key_val(hashtable *table, uint64_t key, void *val);;

// Function Name: get_key_val
// Purpose: Check if a value is in our table and if so then return it
// Returns: The cfgInstruction if it is in the table, null if not
// Exceptions/Errors from library calls: None
// Parameters: The table and key to search for
void *get_key_val( hashtable *table, uint64_t key);

// Function Name: freeCFGTable
// Purpose: Free the memory set aside for the hash table
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The table to free
void freeTable(hashtable *htable);

// Function Name: wipeCFGTable
// Purpose: wipe the hashtable clean
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The table to wipe clean
void wipeTable(hashtable *htable);
#endif
