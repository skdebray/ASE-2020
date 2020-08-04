#include <inttypes.h>
#include <stdlib.h>
#include "hashtable.h"
#include "cfg.h"
#include "utils.h"

#define HT_SIZE 65537
/*
* File: hashtable.c
* Author: Jesse Bartels
* Description: Simple hash table for use in the cfg. Each key_value pair uses an instruction's address for
* the key (at to generate the hash) and stores a pointer to the instruction as the value.
*/


// Function Name: create_table
// Purpose: Create a hash table of HT_SIZE size.
// Returns: 1(true) if the instruction passed in is a pop instruction, 0 otherwise
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory left for allocation
// Parameters: None
hashtable *create_table(){
  // Create space for our table wrapper (which holds the actual hash table and the size/occupancy of the table)
  hashtable *htable = NULL;
  uint64_t size = HT_SIZE;
  // Allocate space for our table
  htable = alloc(sizeof(hashtable));
  // Obtain chunk of memory for our array of key_values
  htable->table = alloc(size * sizeof(key_val *));
  // Set each key value pair to null
  int i = 0;
  for(i = 0; i < size; i++){
    htable->table[i] = NULL;
  }
  // Set the hashtable size
  htable->size = size;
  // Return pointer to the table
  return htable;
}

// Function Name: gen_hash
// Purpose: Generate hash value to produce mapping from bject to slot in table
// Returns: Hash value generated
// Exceptions/Errors from library calls: None
// Parameters: The table and the address from which to generate an index
uint64_t gen_hash(hashtable *table, uint64_t lbaddress){
        // Generate a hash value based on address
        uint64_t x = lbaddress;
	x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
	x = x ^ (x >> 31);
	return (x % table->size);
}

// Function Name: new_key_val
// Purpose: Create a new key/value pair based on the passed in arguments.
//          Note: key = address, value = pointer to instruction with that address 
// Returns: Key/value pair if alloc succeeds
// Exceptions/Errors from library calls: Alloc can fail if there is not enough memory
// Parameters: The key (the instruction address) and the cfgInstruction to make a new key_val struct with
key_val *new_key_val(uint64_t key, void *value){
  // Allocated space for a new key/value pair
  key_val *new_key_val = alloc(sizeof(key_val));
  // Key is the instruction's address
  new_key_val->key = key;
  // Value is a pointer to the Instruction struct
  // This struct's memory is managed elsewhere
  new_key_val->val = value;
  // Set the next in the linked list to null (since we are chain hashing)
  new_key_val->next = NULL;
  // Return the key value pair
  return new_key_val;
}

// Function Name: put_key_val
// Purpose: Function to put an instruction to the hash tab;e
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The key, table, and cfgInstruction to add to.
void put_key_val(hashtable *htable, uint64_t key, void *val){
  // Find where to put the nstruction in the hash table
  uint64_t where = gen_hash(htable, key);
  // Get a new key/value pair based on the instruction passed in 
  key_val *insert = new_key_val(key, val);
  // If nothing is there then insert immediately
  if (htable->table[where] == NULL){
    htable->table[where] = insert;
    htable->ins++;
    return;
  }
  // Otherwise search to see if its already in the list
  // if so, free and return
  key_val *cur = htable->table[where];
  while(cur != NULL){
    if (cur->val == val){
      free(insert);
      return;
    }
    cur = cur->next;
  }
  // If its not in the list, insert it at the front of the linked list
  insert->next = htable->table[where];
  htable->table[where] = insert;
  htable->ins++;
  htable->col++;
  return;
}

// Function Name: get_key_val
// Purpose: Check if a value is in our table and if so then return it
// Returns: The cfgInstruction if it is in the table, null if not
// Exceptions/Errors from library calls: None
// Parameters: The table and key to search for
void *get_key_val(hashtable *htable, uint64_t key){
  // Grab our index using our hashing function
  uint64_t where = 0;
  where = gen_hash(htable, key);
  // If nothing is there then return null
  if (htable != NULL && htable->table != NULL && htable->table[where] == NULL){
    return NULL;
  }
  // Otherwise search to see if at front of our list
  key_val *cur = htable->table[where];
  if (cur->key == key){
    return cur->val;
  }
  // Dig if we have to
  while(cur->next != NULL){
    cur = cur->next;
    if (cur->key == key){
      return cur->val;
    }
  }
  return NULL;
}

// Function Name: freeCFGTable
// Purpose: Free the memory set aside for the hash table
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The table to free
void freeTable(hashtable *htable){
  uint64_t i;
  int size = htable->size;
  for(i = 0; i<size; i++){
    key_val *cur = htable->table[i];
    key_val *temp = NULL;
    while (cur != NULL){
      temp = cur->next;
      free(cur);
      cur = temp;
    }
  }
  free(htable->table);
  free(htable);
}

// Function Name: wipeCFGTable
// Purpose: Reset the hashtable to empty state
// Returns: Nothing
// Exceptions/Errors from library calls: None
// Parameters: The table to wipe
void wipeTable(hashtable *htable){
  int i = 0;
  for(i = 0; i < htable->size; i++){
    htable->table[i] = NULL;
  }
}

void union_cnt(hashtable *one, hashtable *two){
  int shared = 0;
  int walk;
  for(walk = 0; walk < one->size; walk++){
    key_val *cur = one->table[walk];
    while(cur != NULL){
      if(get_key_val(two, cur->key) != NULL){
        shared++;
      }
      cur = cur->next;
    }
  }
  printf("Shared addr_cnt between phases: %d\n", shared);
}
