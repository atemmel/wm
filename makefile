TARGET := wm
LDLIBS := -lX11 -lglog
CC := g++
SRC := $(wildcard *.cpp)
OBJ := $(SRC:%.cpp=%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDLIBS)
