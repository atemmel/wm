TARGET := wm
LDLIBS := -lX11 -lglog
CC := g++
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:%.cpp=%.o)
INCLUDES = -Iinclude/
CXXFLAGS := -std=c++17

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(LDLIBS) $(CXXFLAGS) $(INCLUDES)
