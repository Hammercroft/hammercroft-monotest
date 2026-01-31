#!/bin/bash
cd .. && make clean && make dist platform=modernx11 optimize=1 warnings=1 && ./dist/monotest
