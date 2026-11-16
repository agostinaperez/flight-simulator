CXX = g++
CC = gcc

GLAD_DIR ?= third_party/glad
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/flight-simulator.exe

CPPFLAGS := -I$(GLAD_DIR)/include -Ithird_party -Ithird_party/FastNoiseLite
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS := -O2 -Wall -Wextra
LDLIBS := -lglfw3 -lopengl32 -lgdi32 -lpthread

SOURCES_CPP := src/main.cpp
SOURCES_C := $(GLAD_DIR)/src/gl.c
OBJECTS := $(BUILD_DIR)/main.o $(BUILD_DIR)/glad.o

.PHONY: all run clean

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

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
