# remember to document in BUILD.txt
CXX := g++
CXXFLAGS := -std=c++11 -I src
SRC := src/main.cpp src/game.cpp src/engine/engine.cpp

# Platform Configuration
# Invoke with platform=<platform>
platform ?= x11

ifeq ($(platform),x11)
    CXXFLAGS += -march=x86-64
    PLATFORM_SRC := src/engine/platform/engine_x11.cpp
else ifeq ($(platform),modernx11)
    CXXFLAGS += -march=x86-64-v3
    PLATFORM_SRC := src/engine/platform/engine_x11.cpp
else
    $(error Unknown platform: $(platform). Valid options: x11, modernx11)
endif

# Strict Mode
# Invoke with strict=1
ifdef strict
    CXXFLAGS += -Wall -Wextra
endif

# Output Configuration
# Invoke with dist=1 or use 'make dist' target
ifdef dist
    OUT_DIR := dist
else
    OUT_DIR := bin
endif

RUNTIME_TARGET := $(OUT_DIR)/monotest
ENGINE_TARGET := $(OUT_DIR)/engine.so

# Build Selection
# Invoke with build=game or build=engine. Default: both.
ifeq ($(build),game)
    BUILD_TARGETS := $(RUNTIME_TARGET)
else ifeq ($(build),engine)
    BUILD_TARGETS := $(ENGINE_TARGET)
else
    BUILD_TARGETS := $(RUNTIME_TARGET) $(ENGINE_TARGET)
endif

.PHONY: all clean dist build

# Default target
all: build

# Recursive call for dist target to enable dist mode
dist:
	$(MAKE) build dist=1

build: $(BUILD_TARGETS)
ifdef dist
	@echo "Copying assets to dist..."
	@mkdir -p $(OUT_DIR)
	@cp -r assets $(OUT_DIR)/
endif

# Additional Engine Sources
BKG_SRC := src/engine/bkg/bkgimagemanager.cpp

# Build the engine shared library
# Note: -fPIC is needed for shared libraries
$(ENGINE_TARGET): src/engine/engine.cpp src/engine/engine.h $(PLATFORM_SRC) $(BKG_SRC)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -fPIC -shared -o $@ $^ -lX11

# Build the game executable (monotest)
# Links against engine.so
$(RUNTIME_TARGET): src/main.cpp src/game.cpp $(ENGINE_TARGET)
	@mkdir -p $(OUT_DIR)
	$(CXX) $(CXXFLAGS) -L$(OUT_DIR) -l:engine.so -Wl,-rpath='$$ORIGIN' -o $@ $^

clean:
	rm -rf bin dist
