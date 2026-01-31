#!/bin/bash
cd .. && make clean && make dist platform=gdi optimize=1 warnings=1 && wine ./dist/monotest.exe
