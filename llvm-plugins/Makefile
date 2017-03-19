CC=g++
INCLUDES=-I. -I../staticlib -I../metapagetable
CFLAGS=-c -Werror -Wall -std=c++0x -g `llvm-config --cxxflags` -O0
LDFLAGS=-g -shared
TARGETDIR=.
OBJDIR=$(TARGETDIR)/obj
GOLDINSTDIR=$(METALLOC_HOME)/autosetup.dir/install/common
LINKWITHGOLDFLAGS=-Wl,--no-undefined -L $(GOLDINSTDIR)/lib/ -l:LLVMgold.so -Wl,--rpath=$(GOLDINSTDIR)/lib

EXE=$(TARGETDIR)/libplugins.so
EXE2=$(TARGETDIR)/libplugins-opt.so

SRCS    := $(wildcard *.cpp)
OBJS    := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

all: $(EXE) $(EXE2)

clean:
	rm $(OBJDIR)/*.o
	rm $(OBJDIR)/*.d
	rm $(EXE)
	rm $(EXE2)

$(EXE): $(OBJS) $(TARGETDIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LINKWITHGOLDFLAGS) -o $@

$(EXE2): $(OBJS) $(TARGETDIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

-include $(DEPS)

$(OBJDIR)/%.o: %.cpp $(OBJDIR)
	$(CC) $(INCLUDES) $(CFLAGS) -MMD -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGETDIR):
	mkdir -p $(TARGETDIR)

