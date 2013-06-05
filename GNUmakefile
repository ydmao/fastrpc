DEPS := .deps
DEPCFLAGS = -MD -MF $(DEPS)/$*.d
OBJDIR = ./obj
SRCDIR = ./src
CXXFLAGS = -fPIC -g -std=gnu++0x -I./src -include config.h -fno-omit-frame-pointer -O3

all: $(OBJDIR)/libfastrpc.so

OBJECTS := $(wildcard $(SRCDIR)/rpc/*.cc) $(wildcard $(SRCDIR)/rpc_common/*.cc)
OBJECTS := $(subst .cc,.o,$(notdir $(OBJECTS))) 
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))

$(OBJDIR)/%.o: $(SRCDIR)/rpc_common/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	g++ $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/rpc/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	g++ $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/libfastrpc.so: $(OBJECTS)
	g++ -shared $^ -o $@

DEPFILES := $(wildcard $(DEPS)/*.d)
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif

.PRECIOUS: $(OBJDIR)/*.o

clean:
	rm $(DEPS) $(OBJDIR) -rf

