#
# Cross-Platform Makefile for ImGui App
# Works on Linux, macOS, and Windows (MinGW)
# Builds object files in build/ and executables in bin/
#

# -----------------------------------------------------
# Configurable paths
# -----------------------------------------------------

EXE_NAME = app
BIN_DIR  = bin
BUILD_DIR = build
SRC_DIR = src/linux
IMGUI_DIR = lib/imgui
BACKEND_DIR = backends

# -----------------------------------------------------
# Compiler and flags
# -----------------------------------------------------

CXX = g++
CXXFLAGS = -std=c++17 -I$(IMGUI_DIR) -I$(BACKEND_DIR) -I$(SRC_DIR) -g -Wall -Wformat
UNAME_S := $(shell uname -s)
LIBS =

# -----------------------------------------------------
# Platform-specific setup
# -----------------------------------------------------

ifeq ($(UNAME_S), Linux)
	ECHO_MESSAGE = "Linux build"
	LIBS += -lGL -lX11 `pkg-config --static --libs glfw3`
	CXXFLAGS += `pkg-config --cflags glfw3`
endif

ifeq ($(UNAME_S), Darwin)
	ECHO_MESSAGE = "macOS build"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lglfw
	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "Windows (MinGW) build"
	LIBS += -lglfw3 -lgdi32 -lopengl32 -limm32
endif

# -----------------------------------------------------
# Source files
# -----------------------------------------------------

SOURCES = \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/application.cpp \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_demo.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(BACKEND_DIR)/imgui_impl_glfw.cpp \
	$(BACKEND_DIR)/imgui_impl_opengl3.cpp

# -----------------------------------------------------
# Derived file lists
# -----------------------------------------------------

OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.cpp=.o)))
EXE  = $(BIN_DIR)/$(EXE_NAME)

# -----------------------------------------------------
# Default target
# -----------------------------------------------------

all: setup $(EXE)
	@echo "✅ Build complete for $(ECHO_MESSAGE)"

# -----------------------------------------------------
# Linking
# -----------------------------------------------------

$(EXE): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

# -----------------------------------------------------
# Compilation rules
# -----------------------------------------------------

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BACKEND_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -----------------------------------------------------
# Utility targets
# -----------------------------------------------------

setup:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: all
	./$(EXE)

.PHONY: all clean run setup