TARGETS := wm wmc
LDLIBS := -lX11 -lglog
CC := g++
SRCDIR := src
SRC := $(wildcard $(SRCDIR)/*.cpp)
SRC := $(filter-out $(SRCDIR)/wm.cpp, $(SRC))
SRC := $(filter-out $(SRCDIR)/wmc.cpp, $(SRC))
OBJ := $(SRC:%.cpp=%.o)
INCLUDES = -Iinclude/
CXXFLAGS := -std=c++17

all: $(TARGETS)

$(TARGETS): $(SRC)
	$(CC) -o $@ $(SRCDIR)/$@.cpp $^ $(LDLIBS) $(CXXFLAGS) $(INCLUDES)
