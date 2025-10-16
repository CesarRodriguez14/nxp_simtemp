# Name of the source file (change this to your actual file)
SRC = main.cpp

# Name of the output executable
OUT = cli_nxp_simtemp

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g

# Default target
all: $(OUT)

# Build rule
$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(OUT) $(SRC)

# Clean up build artifacts
clean:
	rm -f $(OUT)
