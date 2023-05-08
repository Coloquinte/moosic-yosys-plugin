
CXX_FLAGS ?= -O2
OBJECTS = yosys_plugin.o logic_locking_optimizer.o
LIBNAME = moosic_logic_locking.so

all: moosic_logic_locking.so


$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $< -shared

%.o: src/%.cpp
	yosys-config --exec --cxx -c --cxxflags $(CXX_FLAGS) -o $@ $<

install:
	yosys-config --exec mkdir -p --datdir/plugins/
	yosys-config --exec cp $(LIBNAME) --datdir/plugins/

clean:
	rm $(OBJECTS)


