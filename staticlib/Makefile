CC=clang
METAPAGETABLEDIR=../metapagetable
INCLUDES=-I. -I$(METAPAGETABLEDIR)
CFLAGS=-c -Werror -Wall -flto -fPIC -std=gnu11
OBJDIR=./obj

CFLAGS += -O3

EXE=$(OBJDIR)/libmetadata.a

SRCS    := $(wildcard *.c) $(wildcard **/*.c)
OBJS    := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

# DangSan configuration
CFLAGS += -DTRACK_ALL_PTRS
CFLAGS += -DNDEBUG
ifdef DANG_STATS
	CFLAGS += -DDANG_STATS
endif
ifdef DANG_HASHTABLE_INITSIZE
	CFLAGS += -DDANG_HASHTABLE_INITSIZE=${DANG_HASHTABLE_INITSIZE}
endif
ifdef DANG_HASHTABLE_GROWFACTOR
	CFLAGS += -DDANG_HASHTABLE_GROWFACTOR=${DANG_HASHTABLE_GROWFACTOR}
endif
ifdef DANG_HASHTABLE_MAXLOADFRAC
	CFLAGS += -DDANG_HASHTABLE_MAXLOADFRAC=${DANG_HASHTABLE_MAXLOADFRAC}
endif
ifdef DANG_HASHTABLE_SKIP
	CFLAGS += -DDANG_HASHTABLE_SKIP=${DANG_HASHTABLE_SKIP}
endif
ifdef DANG_STATICLOG_LOOKBACK
	CFLAGS += -DDANG_STATICLOG_LOOKBACK=${DANG_STATICLOG_LOOKBACK}
endif
ifdef DANG_OBJLOG_ENTRIES
	CFLAGS += -DDANG_OBJLOG_ENTRIES=${DANG_OBJLOG_ENTRIES}
endif

all: directories $(EXE) $(EXE2)

clean:
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(EXE)

$(EXE): $(OBJS) directories
	llvm-ar crv $@ $(OBJS)
	llvm-ranlib $@

-include $(DEPS)

$(OBJDIR)/%.o: %.c directories
	$(CC) $(INCLUDES) $(CFLAGS) -MMD -o $@ $< 

directories:
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/Dangling
