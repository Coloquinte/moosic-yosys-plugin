/*
 * Copyright (c) 2023 Gabriel Gouvine
 */

#include "gate_insertion.hpp"

using Yosys::GetSize;
using Yosys::log;
using Yosys::log_error;
using Yosys::log_id;
using Yosys::RTLIL::Cell;
using Yosys::RTLIL::escape_id;
using Yosys::RTLIL::Module;
using Yosys::RTLIL::SigBit;

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

	log("Inserting locking gate at cell %s\n", log_id(locked_cell->name));

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

	log("Inserting mixing gate at cell %s with cell %s\n", log_id(locked_cell1->name), log_id(locked_cell2->name));

	if (key_value) {
		return module->addMux(NEW_ID, mix_bit, locked_bit, key_bit, out_bit);
	} else {
		return module->addMux(NEW_ID, locked_bit, mix_bit, key_bit, out_bit);
	}
}

/**
 * @brief Add a new input port to the module to be used as a key
 */
static Wire *add_key_input(Module *module)
{
	int ind = 1;
	IdString name = module->uniquify(escape_id("lock_key"), ind);
	Wire *wire = module->addWire(name);
	wire->port_input = true;
	module->fixup_ports();
	return wire;
}

/**
 * @brief Get the output port name of a cell
 */
static IdString get_output_portname(Cell *cell)
{
	for (auto it : cell->connections()) {
		if (cell->output(it.first)) {
			return it.first;
		}
	}
	log_error("No output port found on the cell");
}

/**
 * @brief Lock the gates in the module given a key bit value; return the input key wires
 */
std::vector<Wire *> lock_gates(Module *module, const std::vector<Cell *> &cells, const std::vector<bool> &key_values)
{
	if (cells.size() > key_values.size()) {
		log_error("Number of cells and values for logic locking should be the same");
	}
	std::vector<Wire *> key_inputs;
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		Wire *key_input = add_key_input(module);
		key_inputs.push_back(key_input);
		IdString port = get_output_portname(cells[i]);
		insert_xor_locking_gate(module, cells[i], port, key_input, key_value);
	}
	return key_inputs;
}

/**
 * @brief Lock the gates in the module by name and key bit value; return the input key wires
 */
std::vector<Wire *> lock_gates(Module *module, const std::vector<IdString> &names, const std::vector<bool> &key_values)
{
	std::vector<Cell *> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		IdString name = names[i];
		Cell *cell = module->cell(name);
		if (cell) {
			cells.push_back(cell);
		} else {
			log_error("Cell %s not found in module\n", name.c_str());
		}
	}
	return lock_gates(module, cells, key_values);
}

/**
 * @brief Mix the gates in the module given a key bit value
 */
std::vector<Wire *> mix_gates(Module *module, const std::vector<std::pair<Cell *, Cell *>> &cells, const std::vector<bool> &key_values)
{
	if (cells.size() != key_values.size()) {
		log_error("Number of cells and values for logic locking should be the same");
	}
	std::vector<Wire *> key_inputs;
	for (int i = 0; i < GetSize(cells); ++i) {
		bool key_value = key_values[i];
		Wire *key_input = add_key_input(module);
		key_inputs.push_back(key_input);
		Cell *c1 = cells[i].first;
		Cell *c2 = cells[i].second;
		IdString p1 = get_output_portname(c1);
		IdString p2 = get_output_portname(c2);
		insert_mux_locking_gate(module, c1, p1, c2, p2, key_input, key_value);
	}
	return key_inputs;
}

/**
 * @brief Mix the gates in the module by name and key bit value
 */
std::vector<Wire *> mix_gates(Module *module, const std::vector<std::pair<IdString, IdString>> &names, const std::vector<bool> &key_values)
{
	std::vector<std::pair<Cell *, Cell *>> cells;
	for (int i = 0; i < GetSize(names); ++i) {
		Cell *c1 = module->cell(names[i].first);
		Cell *c2 = module->cell(names[i].second);
		if (c1 && c2) {
			cells.emplace_back(c1, c2);
		} else if (!c1) {
			log_error("Cell %s not found in module\n", names[i].first.c_str());
		} else {
			log_error("Cell %s not found in module\n", names[i].second.c_str());
		}
	}
	return mix_gates(module, cells, key_values);
}