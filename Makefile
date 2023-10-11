
CXX_FLAGS ?= -O2
LD_FLAGS ?= -lboost_system -lboost_filesystem
OBJECTS = yosys_plugin.o logic_locking_optimizer.o output_corruption_optimizer.o logic_locking_analyzer.o logic_locking_statistics.o mini_aig.o gate_insertion.o
LIBNAME = moosic-yosys-plugin.so
# Default command substitution for yosys
DESTDIR ?= --datdir

all: $(LIBNAME)


$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $^ -shared --ldflags $(LD_FLAGS)

%.o: src/%.cpp
	yosys-config --exec --cxx -c --cxxflags -I $(DESTDIR)/include $(CXX_FLAGS) -o $@ $<

install: $(LIBNAME)
	yosys-config --exec mkdir -p $(DESTDIR)/plugins/
	yosys-config --exec cp $(LIBNAME) $(DESTDIR)/plugins/

clean:
	rm $(OBJECTS)


