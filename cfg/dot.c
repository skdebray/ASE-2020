/*
 * File: dot.c
 * Author: Saumya Debray
 * Purpose: Write out the control flow graph as a dot file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utils.h"
#include "cfgState.h"
#include "cfg.h"
#include <XedDisassembler.h>

/*
 * dotfilename(trfilename) -- given the name of a trace file,
 * returns the name of the corresponding dot file.
 */
static char *dotfilename(char *trfilename) {
  int n;
  char *p;
  assert(trfilename != NULL);

  n = strlen(trfilename);
  p = alloc(n+5);  /* 5 = strlen(".dot") + 1 byte for terminating NUL */
  strcpy(p, trfilename);
  strcat(p, ".dot");

  return p;
}

/*
 * fun_name(fun) -- returns a function's name, if the name field is non-NULL;
 * otherwise it returns a string constructed out of the address of the
 * function's first basic block.
 */
static char *fun_name(Function *ifun) {
  char buf[20], *prefix;

  assert(ifun != NULL);

  if (ifun->name != NULL) {
    return strdup(ifun->name);
  }

  //printf("Non-named function present\n");
  prefix = "f_";
  strcpy(buf, prefix);
  snprintf(buf+strlen(prefix), 16, "%d", ifun->id);

  return strdup(buf);
}

/*
 * print_dot_hdr(fp) and print_dot_ftr(fp) -- print out the dot header and footer
 * to the file fp.
 */
static void print_dot_hdr(FILE *fp, char *trfile) {
  fprintf(fp, "digraph \"G\" {  /* tracefile: %s */\n", trfile);
}

static void print_dot_ftr(FILE *fp) {
  fprintf(fp, "}\n");
}

static char * print_ins_no_r(cfgInstruction *iins, char *ins_str, cfgState *state) {
  char this_ins_str[1024], *cat_ins_str;
  int cat_ins_len;

  assert(iins && ins_str);

  char mnemonic[256];
  xed_decoded_inst_t xedIns;
  decodeIns(state->disState, &xedIns, iins->event.ins.binary, 15);
  getInsMnemonic(&xedIns, mnemonic, 256);
  snprintf(this_ins_str, 1024, "%llx: %s %d %d", (unsigned long long)iins->event.ins.addr, mnemonic, iins->block->fun->id, iins->event.ins.tid);
  cat_ins_len = strlen(ins_str) + strlen(this_ins_str) + 3;  /* 3 = strlen("\\n") + NUL */
  cat_ins_str = alloc(cat_ins_len);
  strcpy(cat_ins_str, ins_str);
  strcat(cat_ins_str, "\\n");
  strcat(cat_ins_str, this_ins_str);
  return cat_ins_str;

}

/*
 * cat_ins_str(iins, ins_str) -- returns the result of concatenating a printable
 * representation of the instruction iins at the end of the string ins_str.
 */
static char * print_ins(cfgInstruction *iins, char *ins_str, ReaderState *rState) {
  char this_ins_str[1024], *cat_ins_str;
  int cat_ins_len;

  assert(iins && ins_str);

  /*InsInfo iinfo;
  initInsInfo(&iinfo);
  fetchInsInfo(rState, &(iins->event.ins), &iinfo);
  snprintf(this_ins_str, 1024, "%llx: %s %d %d", (unsigned long long)iins->event.ins.addr, iinfo.mnemonic, iins->block->fun->id, iins->event.ins.tid);*/

  snprintf(this_ins_str, 1024, "%llx", (unsigned long long)iins->event.ins.addr);
  cat_ins_len = strlen(ins_str) + strlen(this_ins_str) + 3;  /* 3 = strlen("\\n") + NUL */
  cat_ins_str = alloc(cat_ins_len);
  strcpy(cat_ins_str, ins_str);
  strcat(cat_ins_str, "\\n");
  strcat(cat_ins_str, this_ins_str);
  return cat_ins_str;

}

static void print_bbl_no_r(FILE *fp, Bbl *ibbl, cfgState *state) {
  char *bblstyle, *label;
  cfgInstruction *iins;

  assert(ibbl != NULL);
  if( ibbl->first != NULL ){
    label = strdup(state->strTable + ibbl->first->event.ins.fnId);
  } else {
    label = "";
  }

  for (iins = ibbl->first; iins != NULL; iins = iins->next) {
    label = print_ins_no_r(iins, label, state);
  }

  if (ibbl->btype == BT_ENTRY || ibbl->btype == BT_EXIT) {  /* pseudo-blocks */
    bblstyle = "dashed";
  }
  else {
    bblstyle = "solid";
  }

  fprintf(fp, "    B%d [shape=box, style=%s, label=\"%s\"];\n", ibbl->id, bblstyle, label);
}

/*
 * print_bbl(fp, ibbl) -- print out the dot representation of basic block ibbl
 * into the file fp.
 */
static void print_bbl(FILE *fp, Bbl *ibbl, ReaderState *rState) {
  char *bblstyle, *label;
  cfgInstruction *iins;

  assert(ibbl != NULL);
  if( ibbl->first != NULL ){
    label = strdup(fetchStrFromId(rState, ibbl->first->event.ins.fnId));
  } else {
    label = "";
  }

  for (iins = ibbl->first; iins != NULL; iins = iins->next) {
    label = print_ins(iins, label, rState);
  }

  if (ibbl->btype == BT_ENTRY || ibbl->btype == BT_EXIT) {  /* pseudo-blocks */
    bblstyle = "dashed";
  }
  else {
    bblstyle = "solid";
  }

  fprintf(fp, "    B%d [shape=box, style=%s, label=\"%s\"];\n", ibbl->id, bblstyle, label);
}

/*
 * print_edg(fp, iedg) -- print out the dot representation of edge iedg
 * into the file fp.
 */
static void print_edg(FILE *fp, Edge *iedg) {
  char *edgstyle;
  char *edgcolor = "black";

  assert(iedg != NULL);

  if (iedg->etype == ET_ENTRY || iedg->etype == ET_EXIT || iedg->etype == ET_LINK) {
    edgstyle = "dashed";  /* pseudo-edges */
  } else if (iedg->etype == ET_DYNAMIC){
    printf("DYNAMIC\n");
    edgstyle = "dotted";
    edgcolor = "red";
  } else if (iedg->etype == ET_EXCEPTION) {
    edgcolor = "red";
    edgstyle = "solid";
  }else {
    edgstyle = "solid";
  }

  fprintf(fp, "    B%d -> B%d [style=%s, color=\"%s\", label=\" ct:%ld\"];\n",
      iedg->from->id, iedg->to->id, edgstyle, edgcolor, iedg->count);
}

/*
 * print_fun(fp, ifun) -- print out the dot representation of function fun
 * to file fp.
 */
static void print_fun(FILE *fp, Function *ifun, ReaderState *rState) {
  char *fname;
  Bbl *ibbl;
  Edge *iedg;

  assert(ifun != NULL);

  // Get function name
  fname = fun_name(ifun);
  fprintf(fp, "//Starting Function %s", fname);

  fprintf(fp, "\n");
  fprintf(fp, "  subgraph %d {\n", ifun->id);
  fprintf(fp, "    label = \"%d\";\n", ifun->id);

  /* print all the blocks */
  for (ibbl = ifun->first; ibbl != NULL; ibbl = ibbl->next) {
    print_bbl(fp, ibbl, rState);
  }

  /* print all the edges */
  for (ibbl = ifun->first; ibbl != NULL; ibbl = ibbl->next) {
    for (iedg = ibbl->succs; iedg != NULL; iedg = iedg->next) {
      print_edg(fp, iedg);
    }
  }
  fprintf(fp, "//Ending Function %s\n", fname);
  fprintf(fp, "  }\n");
}

static void print_fun_no_r(FILE *fp, Function *ifun, cfgState *state) {
  char *fname;
  Bbl *ibbl;
  Edge *iedg;

  assert(ifun != NULL);

  // Get function name
  fname = fun_name(ifun);
  fprintf(fp, "//Starting Function %s", fname);

  fprintf(fp, "\n");
  fprintf(fp, "  subgraph %d {\n", ifun->id);
  fprintf(fp, "    label = \"%d\";\n", ifun->id);

  /* print all the blocks */
  for (ibbl = ifun->first; ibbl != NULL; ibbl = ibbl->next) {
    print_bbl_no_r(fp, ibbl, state);
  }

  /* print all the edges */
  for (ibbl = ifun->first; ibbl != NULL; ibbl = ibbl->next) {
    for (iedg = ibbl->succs; iedg != NULL; iedg = iedg->next) {
      print_edg(fp, iedg);
    }
  }
  fprintf(fp, "//Ending Function %s\n", fname);
  fprintf(fp, "  }\n");
}

/*
 * mkdotfile(trfile, cfg) -- create a dot file for the control flow graph cfg
 * corresponding to the trace file trfile.
 */
void mkdotfile(char *trfile, cfgState *cfgStructArg, ReaderState *rState) {
  cfg *cfgroot = cfgStructArg->curCfgStruct;
  char *dotfile = dotfilename(trfile);
  FILE *fp;
  Function *ifun;

  assert(cfgroot != NULL);

  fp = fopen(dotfile, "w");
  if (fp == NULL) {
    perror(dotfile);
    exit(1);
  }

  print_dot_hdr(fp, trfile);

  for (ifun = cfgroot->startingpoint; ifun != NULL; ifun = ifun->next) {
    print_fun(fp, ifun, rState);
  }

  print_dot_ftr(fp);

  fclose(fp);
}

void mkdotfile_no_r(char *trfile, cfgState *state) {
  cfg *cfgroot = state->curCfgStruct;
  char *dotfile = dotfilename(trfile);
  FILE *fp;
  Function *ifun;

  assert(cfgroot != NULL);

  fp = fopen(dotfile, "w");
  if (fp == NULL) {
    perror(dotfile);
    exit(1);
  }

  print_dot_hdr(fp, trfile);

  for (ifun = cfgroot->startingpoint; ifun != NULL; ifun = ifun->next) {
    print_fun_no_r(fp, ifun, state);
  }

  print_dot_ftr(fp);

  fclose(fp);
}
