/*
 * This file is part of JKQ QFR library which is released under the MIT license.
 * See file README.md or go to http://iic.jku.at/eda/research/quantum/ for more information.
 */
#include "QuantumComputation.hpp"

namespace qc {
	void QuantumComputation::emplaceQiskitOperation(const py::object& instruction, const py::list& qargs, const py::list& cargs, const py::list& params) {
		static const auto nativelySupportedGates = std::set<std::string>{"i", "id", "iden", "x", "y", "z", "h", "s", "sdg", "t", "tdg", "p", "u1", "rx", "ry", "rz", "u2", "u", "u3", "cx", "cy", "cz", "cp", "cu1", "ch", "crx", "cry", "crz", "cu3", "ccx", "swap", "cswap", "iswap", "sx", "sxdg", "csx", "mcx_gray", "mcx_recursive", "mcx_vchain", "mcphase", "mcrx", "mcry", "mcrz"};

		std::string instructionName = instruction.attr("name").cast<std::string>();
		if (instructionName == "measure") {
			auto&& qubit = qargs[0];
			auto&& clbit = cargs[0];
			auto&& qreg = qubit.attr("register").attr("name").cast<std::string>();
			auto&& creg = clbit.attr("register").attr("name").cast<std::string>();
			auto&& qidx = qubit.attr("index").cast<unsigned short>();
			auto&& cidx = clbit.attr("index").cast<unsigned short>();
			unsigned short control = getIndexFromQubitRegister({qreg, qidx});
			unsigned short target = getIndexFromClassicalRegister({creg, cidx});
			emplace_back<qc::NonUnitaryOperation>(getNqubits(), control, target);
		} else if (instructionName == "barrier") {
			std::vector<unsigned short> targets{};
			for (const auto qubit: qargs) {
				auto&& qreg = qubit.attr("register").attr("name").cast<std::string>();
				auto&& qidx = qubit.attr("index").cast<unsigned short>();
				unsigned short target = getIndexFromQubitRegister({qreg, qidx});
				targets.emplace_back(target);
			}
			emplace_back<qc::NonUnitaryOperation>(getNqubits(), targets, qc::Barrier);
		} else if (nativelySupportedGates.count(instructionName)) {
			// natively supported operations
			if (instructionName == "i" || instructionName == "id" || instructionName == "iden") {
				addQiskitOperation(qc::I, qargs, params);
			} else if (instructionName == "x" || instructionName == "cx" || instructionName == "ccx" || instructionName == "mcx_gray") {
				addQiskitOperation(qc::X, qargs, params);
			} else if (instructionName == "y"|| instructionName == "cy") {
				addQiskitOperation(qc::Y, qargs, params);
			} else if (instructionName == "z" || instructionName == "cz") {
				addQiskitOperation(qc::Z, qargs, params);
			} else if (instructionName == "h" || instructionName == "ch") {
				addQiskitOperation(qc::H, qargs, params);
			} else if (instructionName == "s") {
				addQiskitOperation(qc::S, qargs, params);
			} else if (instructionName == "sdg") {
				addQiskitOperation(qc::Sdag, qargs, params);
			} else if (instructionName == "t") {
				addQiskitOperation(qc::T, qargs, params);
			} else if (instructionName == "tdg") {
				addQiskitOperation(qc::Tdag, qargs, params);
			} else if (instructionName == "rx" || instructionName == "crx" || instructionName == "mcrx") {
				addQiskitOperation(qc::RX, qargs, params);
			} else if (instructionName == "ry" || instructionName == "cry" || instructionName == "mcry") {
				addQiskitOperation(qc::RY, qargs, params);
			} else if (instructionName == "rz" || instructionName == "crz" || instructionName == "mcrz") {
				addQiskitOperation(qc::RZ, qargs, params);
			} else if (instructionName == "p" || instructionName == "u1" || instructionName == "cp" || instructionName == "cu1" || instructionName == "mcphase") {
				addQiskitOperation(qc::Phase, qargs, params);
			} else if (instructionName == "sx" || instructionName == "csx") {
				addQiskitOperation(qc::SX, qargs, params);
			} else if (instructionName == "sxdg") {
				addQiskitOperation(qc::SXdag, qargs, params);
			} else if (instructionName == "u2") {
				addQiskitOperation(qc::U2, qargs, params);
			} else if (instructionName == "u" || instructionName == "u3" || instructionName == "cu3") {
				addQiskitOperation(qc::U3, qargs, params);
			} else if (instructionName == "swap" || instructionName == "cswap") {
				addTwoTargetQiskitOperation(qc::SWAP, qargs, params);
			} else if (instructionName == "iswap") {
				addTwoTargetQiskitOperation(qc::iSWAP, qargs, params);
			} else if (instructionName == "mcx_recursive") {
				if (qargs.size() <= 5) {
					addQiskitOperation(qc::X, qargs, params);
				} else {
					auto qargs_copy = qargs.attr("copy")();
					qargs_copy.attr("pop")(); // discard ancillaries
					addQiskitOperation(qc::X, qargs_copy, params);
				}
			} else if (instructionName == "mcx_vchain") {
				unsigned short size = qargs.size();
				unsigned short ncontrols = (size+1)/2;
				auto qargs_copy = qargs.attr("copy")();
				// discard ancillaries
				for (int i=0; i<ncontrols-2; ++i) {
					qargs_copy.attr("pop")();
				}
				addQiskitOperation(qc::X, qargs_copy, params);
			}
		} else {
			try {
				importQiskitDefinition(instruction.attr("definition"), qargs, cargs);
			} catch (py::error_already_set &e) {
				std::cerr << "Failed to import instruction " << instructionName << " from Qiskit" << std::endl;
				std::cerr << e.what() << std::endl;
			}
		}
	}

	void QuantumComputation::importQiskitDefinition(const py::object& circ, const py::list& qargs, const py::list& cargs) {
		py::dict qargMap{};
		py::list&& def_qubits = circ.attr("qubits");
		for (size_t i=0; i<qargs.size(); ++i) {
			qargMap[def_qubits[i]] = qargs[i];
		}

		py::dict cargMap{};
		py::list&& def_clbits = circ.attr("clbits");
		for (size_t i=0; i<cargs.size(); ++i) {
			cargMap[def_clbits[i]] = cargs[i];
		}

		auto&& data = circ.attr("data");
		for (const auto pyinst: data) {
			auto&& inst = pyinst.cast<std::tuple<py::object, py::list, py::list>>();
			auto&& instruction = std::get<0>(inst);

			py::list& inst_qargs = std::get<1>(inst);
			py::list mapped_qargs{};
			for (auto && inst_qarg : inst_qargs) {
				mapped_qargs.append(qargMap[inst_qarg]);
			}

			py::list inst_cargs = std::get<2>(inst);
			py::list mapped_cargs{};
			for (auto && inst_carg : inst_cargs) {
				mapped_cargs.append(cargMap[inst_carg]);
			}

			auto&& inst_params = instruction.attr("params");

			emplaceQiskitOperation(instruction, mapped_qargs, mapped_cargs, inst_params);
		}
	}

	void QuantumComputation::addQiskitOperation(qc::OpType type, const py::list& qargs, const py::list& params) {
		std::vector<qc::Control> qubits{};
		for (const auto qubit: qargs) {
			auto&& qreg = qubit.attr("register").attr("name").cast<std::string>();
			auto&& qidx = qubit.attr("index").cast<unsigned short>();
			unsigned short target = getIndexFromQubitRegister({qreg, qidx});
			qubits.emplace_back(target);
		}
		auto target = qubits.back().qubit;
		qubits.pop_back();
		fp theta=0, phi=0, lambda = 0;
		if (params.size() == 1) {
			lambda = params[0].cast<fp>();
		} else if (params.size() == 2) {
			phi = params[0].cast<fp>();
			lambda = params[1].cast<fp>();
		} else if (params.size() == 3) {
			theta = params[0].cast<fp>();
			phi = params[1].cast<fp>();
			lambda = params[2].cast<fp>();
		}
		emplace_back<qc::StandardOperation>(getNqubits(), qubits, target, type, lambda, phi, theta);
	}

	void QuantumComputation::addTwoTargetQiskitOperation(qc::OpType type, const py::list& qargs, const py::list& params) {
		std::vector<qc::Control> qubits{};
		for (const auto qubit: qargs) {
			auto&& qreg = qubit.attr("register").attr("name").cast<std::string>();
			auto&& qidx = qubit.attr("index").cast<unsigned short>();
			unsigned short target = getIndexFromQubitRegister({qreg, qidx});
			qubits.emplace_back(target);
		}
		auto target1 = qubits.back().qubit;
		qubits.pop_back();
		auto target0 = qubits.back().qubit;
		qubits.pop_back();
		fp theta=0, phi=0, lambda = 0;
		if (params.size() == 1) {
			lambda = params[0].cast<fp>();
		} else if (params.size() == 2) {
			phi = params[0].cast<fp>();
			lambda = params[1].cast<fp>();
		} else if (params.size() == 3) {
			theta = params[0].cast<fp>();
			phi = params[1].cast<fp>();
			lambda = params[2].cast<fp>();
		}
		emplace_back<qc::StandardOperation>(getNqubits(), qubits, target0, target1, type, lambda, phi, theta);
	}

}
