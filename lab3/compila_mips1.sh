#!/bin/bash

cd processors/mips1
~rodolfo/mc723/archc/bin/acsim mips1.ac -abi
make -f Makefile.archc
