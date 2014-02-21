DEPS := .deps
DEPCFLAGS = -MD -MF $(DEPS)/$*.d
OBJDIR = ./obj
SRCDIR = ./src
CXXFLAGS = -fPIC -g -std=gnu++0x -I./src -include config.h -fno-omit-frame-pointer -O3

PROTO_HDR := src/proto/fastrpc_proto.hh \
             src/proto/fastrpc_proto_client.hh \
	     src/proto/fastrpc_proto_server.hh

all: $(PROTO_HDR) $(OBJDIR)/libfastrpc.so $(OBJDIR)/protoc-gen-refcomp

OBJECTS := $(wildcard $(SRCDIR)/rpc/*.cc) $(wildcard $(SRCDIR)/rpc_common/*.cc) $(wildcard $(SRCDIR)/rpc_util/*.cc)
OBJECTS := $(subst .cc,.o,$(notdir $(OBJECTS))) 
OBJECTS := $(addprefix $(OBJDIR)/,$(OBJECTS))

CXX=g++

$(OBJDIR)/%.o: $(SRCDIR)/rpc_common/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/rpc_util/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/compiler/%.cc config.h
	mkdir -p $(DEPS) $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(OBJDIR)/protoc-gen-refcomp: $(OBJDIR)/refcomp.o
	mkdir -p $(DEPS) $(OBJDIR)
	$(CXX) $^ -o $@ -lprotobuf -lprotoc

$(OBJDIR)/%.o: $(SRCDIR)/rpc/%.cc config.h $(PROTO_HDR)
	mkdir -p $(DEPS) $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $(DEPCFLAGS) $< -o $@

$(PROTO_HDR): src/proto/$(PROTO) $(OBJDIR)/protoc-gen-refcomp
	@if test -z $(PROTO) ; then echo "Must provide a PROTO file"; exit -1; fi
	mkdir -p $(DEPS) $(OBJDIR)
	protoc --plugin=$(OBJDIR)/protoc-gen-refcomp $< --refcomp_out=src/proto:.

$(OBJDIR)/libfastrpc.so: $(OBJECTS)
	$(CXX) -shared $(OBJECTS) -o $@

DEPFILES := $(wildcard $(DEPS)/*.d)
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif

.PRECIOUS: $(OBJDIR)/*.o

clean:
	rm $(DEPS) $(OBJDIR) $(SRCDIR)/proto/fastrpc_proto* -rf

