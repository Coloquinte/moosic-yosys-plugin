
CXX_FLAGS ?= -O2
OBJECTS = yosys_plugin.o logic_locking_optimizer.o
LIBNAME = moosic-yosys-plugin.so

all: $(LIBNAME)


$(LIBNAME): $(OBJECTS)
	yosys-config --build $@ $< -shared

%.o: src/%.cpp
	yosys-config --exec --cxx -c --cxxflags $(CXX_FLAGS) -o $@ $<

install: $(LIBNAME)
	yosys-config --exec mkdir -p --datdir/plugins/
	yosys-config --exec cp $(LIBNAME) --datdir/plugins/

clean:
	rm $(OBJECTS)


