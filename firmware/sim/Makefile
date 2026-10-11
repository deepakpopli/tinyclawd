# desktop build of the pet engine, renders animations to frames
PET = ../components/pet
CXX ?= g++
CXXFLAGS = -O2 -g -Wall -Wextra -std=c++17 -I$(PET)/include
SRC = $(wildcard $(PET)/src/*.cpp) main.cpp
OBJ = $(patsubst %.cpp,build/%.o,$(notdir $(SRC)))

vpath %.cpp $(PET)/src .

sim: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lm

build/%.o: %.cpp $(wildcard $(PET)/include/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf build sim

.PHONY: clean
