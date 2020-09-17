/// @file xed-operand-ctype-enum.c

// This file was automatically generated.
// Do not edit this file.

#include <string.h>
#include <assert.h>
#include "xed-operand-ctype-enum.h"

typedef struct {
    const char* name;
    xed_operand_ctype_enum_t value;
} name_table_xed_operand_ctype_enum_t;
static const name_table_xed_operand_ctype_enum_t name_array_xed_operand_ctype_enum_t[] = {
{"INVALID", XED_OPERAND_CTYPE_INVALID},
{"XED_BITS_T", XED_OPERAND_CTYPE_XED_BITS_T},
{"XED_CHIP_ENUM_T", XED_OPERAND_CTYPE_XED_CHIP_ENUM_T},
{"XED_ERROR_ENUM_T", XED_OPERAND_CTYPE_XED_ERROR_ENUM_T},
{"XED_ICLASS_ENUM_T", XED_OPERAND_CTYPE_XED_ICLASS_ENUM_T},
{"XED_INT64_T", XED_OPERAND_CTYPE_XED_INT64_T},
{"XED_REG_ENUM_T", XED_OPERAND_CTYPE_XED_REG_ENUM_T},
{"XED_UINT16_T", XED_OPERAND_CTYPE_XED_UINT16_T},
{"XED_UINT64_T", XED_OPERAND_CTYPE_XED_UINT64_T},
{"XED_UINT8_T", XED_OPERAND_CTYPE_XED_UINT8_T},
{"LAST", XED_OPERAND_CTYPE_LAST},
{0, XED_OPERAND_CTYPE_LAST},
};

        
xed_operand_ctype_enum_t str2xed_operand_ctype_enum_t(const char* s)
{
   const name_table_xed_operand_ctype_enum_t* p = name_array_xed_operand_ctype_enum_t;
   while( p->name ) {
     if (strcmp(p->name,s) == 0) {
      return p->value;
     }
     p++;
   }
        

   return XED_OPERAND_CTYPE_INVALID;
}


const char* xed_operand_ctype_enum_t2str(const xed_operand_ctype_enum_t p)
{
   xed_operand_ctype_enum_t type_idx = p;
   if ( p > XED_OPERAND_CTYPE_LAST) type_idx = XED_OPERAND_CTYPE_LAST;
   return name_array_xed_operand_ctype_enum_t[type_idx].name;
}

xed_operand_ctype_enum_t xed_operand_ctype_enum_t_last(void) {
    return XED_OPERAND_CTYPE_LAST;
}
       
/*

Here is a skeleton switch statement embedded in a comment


  switch(p) {
  case XED_OPERAND_CTYPE_INVALID:
  case XED_OPERAND_CTYPE_XED_BITS_T:
  case XED_OPERAND_CTYPE_XED_CHIP_ENUM_T:
  case XED_OPERAND_CTYPE_XED_ERROR_ENUM_T:
  case XED_OPERAND_CTYPE_XED_ICLASS_ENUM_T:
  case XED_OPERAND_CTYPE_XED_INT64_T:
  case XED_OPERAND_CTYPE_XED_REG_ENUM_T:
  case XED_OPERAND_CTYPE_XED_UINT16_T:
  case XED_OPERAND_CTYPE_XED_UINT64_T:
  case XED_OPERAND_CTYPE_XED_UINT8_T:
  case XED_OPERAND_CTYPE_LAST:
  default:
     xed_assert(0);
  }
*/
