CXX = g++
CC = gcc

GLAD_DIR ?= third_party/glad
DLFDM_DIR ?= ./libs/dlfdm
include $(DLFDM_DIR)/dlfdm.mk

BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/flight-simulator.exe

CPPFLAGS := -I$(GLAD_DIR)/include -I$(DLFDM_INC_DIRS) -Ithird_party -Ithird_party/FastNoiseLite -Isrc -Isrc/common
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS := -O2 -Wall -Wextra
LDLIBS := -lglfw3 -lopengl32 -lgdi32 -lpthread

SOURCES_CPP := src/main.cpp
SOURCES_C := $(GLAD_DIR)/src/gl.c
DLFDM_SOURCES := $(foreach dir,$(DLFDM_LIB_DIRS),$(wildcard $(dir)/*.cpp))
DLFDM_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(DLFDM_SOURCES))
COMMON_SOURCES := \
	src/common/Aircraft.cpp \
	src/common/CameraSystem.cpp \
	src/common/GroundContact.cpp \
	src/common/Hud.cpp \
	src/common/HudGeometry.cpp \
	src/common/InputHandler.cpp \
	src/common/Mesh.cpp \
	src/common/Primitives.cpp \
	src/common/ResourceManager.cpp \
	src/common/Shader.cpp \
	src/common/Terrain.cpp \
	src/common/Texture.cpp
COMMON_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(COMMON_SOURCES))
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/glad.o $(COMMON_OBJECTS) $(DLFDM_OBJECTS)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

$(BUILD_DIR)/main.o: $(SOURCES_CPP)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/glad.o: $(SOURCES_C)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

TEST_TARGET := $(BIN_DIR)/flight-integration-test.exe
TEST_OBJECT := $(BUILD_DIR)/tests/flight_integration_test.o

$(TEST_TARGET): $(TEST_OBJECT) $(BUILD_DIR)/src/common/InputHandler.o $(BUILD_DIR)/src/common/GroundContact.o $(BUILD_DIR)/src/common/Terrain.o $(BUILD_DIR)/src/common/CameraSystem.o $(BUILD_DIR)/src/common/HudGeometry.o $(DLFDM_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $^ -o $@ $(LDLIBS)

$(TEST_OBJECT): tests/flight_integration_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
