#—————————————————————————————————————————————————————————————————
OUT_NAME := chip8
ELF      := $(OUT_NAME).elf

CC       := clang++
CXXFLAGS := -g -std=c++20 -I./src $(shell sdl2-config --cflags)
LDFLAGS  := $(shell sdl2-config --libs)

# grab every .cpp in src/
CPPFILES := $(wildcard src/*.cpp)
OBJS     := $(CPPFILES:.cpp=.o)

.PHONY: all clean

all: $(ELF)

# link step
$(ELF): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# compile each .cpp → .o
src/%.o: src/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(ELF)
