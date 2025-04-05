
OBJECTS = \
	  pairwise_security_optimizer.o \
	  output_corruption_optimizer.o \
	  delay_analyzer.o \
	  logic_locking_analyzer.o \
	  logic_locking_statistics.o \
	  mini_aig.o \
	  gate_insertion.o \
	  optimization_objectives.o \
	  optimization.o \
	  report_locking.o \
	  sat_attack.o \
	  antisat.o \
	  cmd_logic_locking.o \
	  cmd_analyze.o \
	  cmd_apply.o \
	  cmd_direct_locking.o \
	  cmd_explore.o \
	  cmd_show.o \
	  cmd_sat_attack.o \
	  cmd_unlock.o \
	  command_utils.o \

LIBNAME = moosic.so

CXX_FLAGS ?=
LD_FLAGS ?=

# Default command substitution for yosys
DESTDIR ?= $(shell yosys-config --datdir)
CXX := $(shell yosys-config --cxx)
YOSYS_LD_FLAGS := $(shell yosys-config --ldflags --ldlibs)
YOSYS_CXX_FLAGS := $(shell yosys-config --cxxflags)

# Features
ENABLE_WERROR := 0
ifeq ($(ENABLE_WERROR),1)
CXXFLAGS := -Werror $(CXXFLAGS)
endif

all: $(LIBNAME)


$(LIBNAME): $(OBJECTS)
	$(CXX) -o $@ $^ -shared $(YOSYS_LD_FLAGS) $(LD_FLAGS)

%.o: src/%.cpp
	$(CXX) -c $(YOSYS_CXX_FLAGS) $(CXX_FLAGS) -o $@ $<

install: $(LIBNAME)
	mkdir -p $(DESTDIR)/plugins/
	cp $(LIBNAME) $(DESTDIR)/plugins/

clean:
	$(RM) $(OBJECTS)
	$(RM) $(LIBNAME)


