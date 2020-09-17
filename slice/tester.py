#!/usr/bin/python
import os.path
import subprocess
import sys
import os

fi = open("timeOutput", "a")
numTests = 7
#os.system("echo \"Testing Time\" > ./timeOutput")
#for i in range(numTests):
#  os.system("./sliceDriver trace.out 0x40a53000 >> timeOutput")

fi = open("timeOutput", "r")
lin = fi.readlines()
sumDCFG = 0
sumSlice = 0
for i in range(1, len(lin), 2):
  sumDCFG+=int(lin[i])
  sumSlice+=int(lin[i+1])

print("DCFG Average Construction Time: " + str(sumDCFG/numTests))
print("Slice Average Construction Time: " + str(sumSlice/numTests))
