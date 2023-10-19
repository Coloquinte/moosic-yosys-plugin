
OBJECTS = yosys_plugin.o \
	  logic_locking_optimizer.o \
	  output_corruption_optimizer.o \
	  delay_analyzer.o \
	  logic_locking_analyzer.o \
	  logic_locking_statistics.o \
	  mini_aig.o \
	  gate_insertion.o \

LIBNAME = moosic.so
# Default command substitution for yosys
CXX_FLAGS ?= -O2
LD_FLAGS ?= -lboost_system -lboost_filesystem
DESTDIR ?= $(shell yosys-config --datdir)
YOSYS_LD_FLAGS := $(shell yosys-config --ldflags --ldlibs)
YOSYS_CXX_FLAGS := $(shell yosys-config --cxxflags)


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


