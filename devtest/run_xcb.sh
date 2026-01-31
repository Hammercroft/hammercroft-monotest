#!/bin/bash
cd .. && make clean && make dist platform=xcb optimize=3 warnings=1 && ./dist/monotest
