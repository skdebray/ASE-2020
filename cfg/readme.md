# CFG Library

This directory holds the tool to construct control flow graphs (CFG) for a given trace. To use this library one must have a trace with which to construct a CFG and the reader library built.

Build the reader by following the instructions in the reader library. Once the reader is built you can build the CFG tool by running make.

To use the CFG library, one will need to initialize both a reader and the CFG library (see the cfg2dot.c file in the CFG2dot directory for a basic example on how to create a cfg)

Basic API:

initCFG: Initialize a CFG struct. This must be done before a CFG can be created

addInstructionToCFG: Add a given instruction into the cfg.

closeCFG: Close the CFG

For a breakdown of the CFG data structure and how to iterate through a CFG, click here:
https://docs.google.com/document/d/1XSGaoO7jTDQzZc1K8EF9UghQlHKBT4OBwTrg9Z_03hA/edit?usp=sharing
