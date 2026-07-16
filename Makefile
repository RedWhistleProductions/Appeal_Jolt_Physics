PLUGIN_NAME := Appeal_Jolt_Physics
SRC_DIR := src
OBJ_ROOT := obj
CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -Wall
LDFLAGS ?= -shared
LDLIBS ?=

ifeq ($(OS),Windows_NT)
  PLATFORM := windows
  TARGET := $(PLUGIN_NAME).dll
else
  PLATFORM := linux
  TARGET := $(PLUGIN_NAME).so
  CXXFLAGS += -fPIC
endif

OBJ_DIR := $(OBJ_ROOT)/$(PLATFORM)
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_ROOT) $(PLUGIN_NAME).dll $(PLUGIN_NAME).so

.PHONY: all clean