# Compiler and flags
CXX = mpic++
CXXFLAGS = -std=c++17 -I./include -Wall -g
LDFLAGS =

# Project directories
SRCDIR = src
INCDIR = include
BUILDDIR = build

# Find all .cpp files in the source directory
SOURCES = $(wildcard $(SRCDIR)/*.cpp)

# Create a list of object files in the build directory
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

# Target executable
TARGET = $(BUILDDIR)/main

.PHONY: all clean

# Default rule
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile source files into object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to create the build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Rule to clean the project
clean:
	rm -rf $(BUILDDIR)

