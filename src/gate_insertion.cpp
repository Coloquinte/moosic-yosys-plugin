/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "gate_insertion.hpp"
#include "command_utils.hpp"

USING_YOSYS_NAMESPACE

using RTLIL::Cell;
using RTLIL::escape_id;
using RTLIL::Module;
using RTLIL::SigBit;

/**
 * @brief Insert a Xor/Xnor locking gate at the output port of a cell
 *
 * @param module Module to edit
 * @param locked_cell Cell whose output must be locked
 * @param locked_port Output port of the cell
 * @param key_bit Locking bit
 * @param key_value Valid value of the key
 */
Cell *insert_xor_locking_gate(Module *module, Cell *locked_cell, IdString locked_port, SigBit key_bit, bool key_value)
{
	log_assert(locked_cell->output(locked_port));
	SigBit out_bit(locked_cell->getPort(locked_port));

	// Create a new wire to replace the former output
	Wire *locked_bitwire = module->addWire(NEW_ID);
	SigBit locked_bit(locked_bitwire);
	locked_cell->unsetPort(locked_port);
	locked_cell->setPort(locked_port, locked_bitwire);

	log_debug("Inserting locking gate at cell %s\n", log_id(locked_cell->name));

	if (key_value) {
		return module->addXnor(NEW_ID, locked_bit, key_bit, out_bit);
	} else {
		return module->addXor(NEW_ID, locked_bit, key_bit, out_bit);
	}
}

/**
 * @brief Insert a mux locking gate at the output port of a cell
 *
 * @param module Module to edit
 * @param locked_cell1 First cell whose output must be locked
 * @param locked_port1 Output port of the first cell
 * @param locked_cell2 Second cell whose output must be locked
 * @param locked_port2 Output port of the second cell
 * @param key_bit Locking bit
 * @param key_value Valid value of the key
 */
Cell *insert_mux_locking_gate(Module *module, Cell *locked_cell1, IdString locked_port1, Cell *locked_cell2, IdString locked_port2, SigBit key_bit,
			      bool key_value)
{
	log_assert(locked_cell1->output(locked_port1));
	log_assert(locked_cell1->output(locked_port2));
	SigBit out_bit(locked_cell1->getPort(locked_port1));
	SigBit mix_bit(locked_cell2->getPort(locked_port2));

	// Create a new wire to replace the former output
	Wire *locked_bitwire = module->addWire(NEW_ID);
	SigBit locked_bit(locked_bitwire);
	locked_cell1->unsetPort(locked_port1);
	locked_cell1->setPort(locked_port1, locked_bitwire);

	log_debug("Inserting mixing gate at cell %s with cell %s\n", log_id(locked_cell1->name), log_id(locked_cell2->name));

	if (key_value) {
		return module->addMux(NEW_ID, mix_bit, locked_bit, key_bit, out_bit);
	} else {
		return module->addMux(NEW_ID, locked_bit, mix_bit, key_bit, out_bit);
	}
}

Wire *add_key_input(Module *module, int width, const std::string &port_name)
{
	IdString name = escape_id(port_name);
	if (module->wire(name)) {
		log_cmd_error("Wire %s is already present in the module. Did you run logic locking twice\n", log_id(name));
	}
	Wire *wire = module->addWire(name, width);
	wire->port_input = true;
	module->fixup_ports();
	return wire;
}

/**
 * @brief Lock the gates in the module given a key bit value; return the input key wires
 */
void lock_gates(Module *module, const std::vector<Cell *> &cells, SigSpec key, const std::vector<bool> &key_values)
{

	if (GetSize(cells) != GetSize(key_values)) {
		log_cmd_error("Number of cells to lock %d does not match the number of key values %d\n", GetSize(cells), GetSize(key_values));
	}
	if (GetSize(cells) != GetSize(key)) {
		log_cmd_error("Number of cells to lock %d does not match the key length %d\n", GetSize(cells), GetSize(key));
	}
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		IdString port = get_output_portname(cells[i]);
		insert_xor_locking_gate(module, cells[i], port, key[i], key_value);
	}
}

/**
 * @brief Lock the gates in the module by name and key bit value; return the input key wires
 */
void lock_gates(Module *module, const std::vector<IdString> &names, SigSpec key, const std::vector<bool> &key_values)
{
	std::vector<Cell *> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		IdString name = names[i];
		Cell *cell = module->cell(name);
		if (cell) {
			cells.push_back(cell);
		} else {
			log_cmd_error("Cell %s not found in module\n", name.c_str());
		}
	}
	lock_gates(module, cells, key, key_values);
}

/**
 * @brief Mix the gates in the module given a key bit value
 */
void mix_gates(Module *module, const std::vector<std::pair<Cell *, Cell *>> &cells, SigSpec key, const std::vector<bool> &key_values)
{
	if (GetSize(cells) != GetSize(key_values)) {
		log_cmd_error("Number of cells to lock %d does not match the number of key values %d\n", GetSize(cells), GetSize(key_values));
	}
	if (GetSize(cells) != GetSize(key)) {
		log_cmd_error("Number of cells to lock %d does not match the key length %d\n", GetSize(cells), GetSize(key));
	}
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		Cell *c1 = cells[i].first;
		Cell *c2 = cells[i].second;
		IdString p1 = get_output_portname(c1);
		IdString p2 = get_output_portname(c2);
		insert_mux_locking_gate(module, c1, p1, c2, p2, key[i], key_value);
	}
}

/**
 * @brief Mix the gates in the module by name and key bit value
 */
void mix_gates(Module *module, const std::vector<std::pair<IdString, IdString>> &names, SigSpec key, const std::vector<bool> &key_values)
{
	std::vector<std::pair<Cell *, Cell *>> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		Cell *c1 = module->cell(names[i].first);
		Cell *c2 = module->cell(names[i].second);
		if (c1 && c2) {
			cells.emplace_back(c1, c2);
		} else if (!c1) {
			log_cmd_error("Cell %s not found in module\n", names[i].first.c_str());
		} else {
			log_cmd_error("Cell %s not found in module\n", names[i].second.c_str());
		}
	}
	mix_gates(module, cells, key, key_values);
}