TARGET := wm
LDLIBS := -lX11 -lglog
CC := g++
SRC := $(wildcard *.cpp)
OBJ := $(SRC:%.cpp=%.o)
CXXFLAGS := -std=c++17

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(LDLIBS) $(CXXFLAGS)
