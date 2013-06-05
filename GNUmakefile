DEPS := .deps
DEPCFLAGS = -MD -MF $(DEPS)/$*.d
OBJDIR = ./obj
SRCDIR = ./src
CXXFLAGS = -fPIC -Wno-pmf-conversions -g -std=gnu++0x -I./src -include config.h\
	   -I$(OBJDIR) -I. -fno-omit-frame-pointer -O2

LIBS = -lboost_program_options-mt -lboost_thread-mt \
	-lboost_system-mt -lboost_serialization-mt \
	-lev -lprotobuf -ldl -liberty -ltcmalloc

all: $(OBJDIR)/librpc.so

COMMON_OBJS := $(wildcard $(SRCDIR)/rpc/*.cc) $(wildcard $(SRCDIR)/rpc_common/*.cc)
COMMON_OBJS := $(subst .cc,.o,$(notdir $(COMMON_OBJS))) 
COMMON_OBJS := $(addprefix $(OBJDIR)/,$(COMMON_OBJS))

$(OBJDIR)/%.o: $(SRCDIR)/rpc_common/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	g++ $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/rpc/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	g++ $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/librpc.so: $(COMMON_OBJS)
	g++ -shared $^ -o $@

DEPFILES := $(wildcard $(DEPS)/*.d)
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif

.PRECIOUS: $(OBJDIR)/*.o

clean:
	rm $(DEPS) $(OBJDIR) -rf

