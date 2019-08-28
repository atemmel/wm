RELEASE := $(TARGET)-release
LDLIBS := -lX11
OBJDIR := bin
INCDIR := include
SRCDIR := src
SRC := $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/*/*.cpp)
SRC := $(filter-out $(SRCDIR)/$(EXCLUDE).cpp, $(SRC))
OBJ := $(subst $(SRCDIR),$(OBJDIR),$(SRC:%.cpp=%.o))
CC := g++
CXXFLAGS := -pedantic -Wall -Wextra -Wfloat-equal -Wwrite-strings -Wno-unused-parameter -Wundef -Wcast-qual -Wshadow -Wredundant-decls -std=c++17 -I$(INCDIR)
DBGFLAGS := -g
RELEASEFLAGS := -Ofast

TARGET := $(OBJDIR)/$(TARGET)
RELEASE := $(OBJDIR)/$(RELEASE)

all: $(TARGET)

debug: CXXFLAGS += $(DBGFLAGS)
debug: $(TARGET)

release: 
	$(eval CXXFLAGS += $(RELEASEFLAGS))
	$(CC) -o $(RELEASE) $(SRC) $(LDLIBS) $(CXXFLAGS) 

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDLIBS)  $(CXXFLAGS)

$(OBJ): $(OBJDIR)%.o : $(SRCDIR)%.cpp
	$(CC) -o $@ -c $< $(LDLIBS) $(CXXFLAGS)

.PHONY: clean setup
clean: 
	rm $(TARGET) $(OBJ)

setup:
	-mkdir $(OBJDIR)

install:
	cp $(RELEASE) /usr/bin/$(TARGET)
