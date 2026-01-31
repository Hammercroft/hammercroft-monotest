#!/bin/bash
cd .. && make clean && make dist platform=d3d11 optimize=1 warnings=1 && wine ./dist/monotest.exe
