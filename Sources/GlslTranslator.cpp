#include "GlslTranslator.h"
#include <fstream>
#include <map>

using namespace krafix;

namespace {
	struct Variable {
		unsigned type;
		spv::StorageClass storage;
	};

	struct Type {
		const char* name;
		unsigned length;

		Type() : length(1) {}
	};

	struct Name {
		const char* name;
	};
	
	const char* indexName(unsigned index) {
		switch (index) {
			case 0:
				return "x";
			case 1:
				return "y";
			case 2:
				return "z";
			case 3:
			default:
				return "w";
		}
	}
}

void GlslTranslator::outputCode(const char* baseName) {
	using namespace spv;

	std::map<unsigned, Name> names;
	std::map<unsigned, Type> types;
	std::map<unsigned, Variable> variables;

	std::ofstream out;
	std::string fileName(baseName);
	fileName.append(".glsl");
	out.open(fileName.c_str(), std::ios::binary | std::ios::out);
	
	for (unsigned i = 0; i < instructions.size(); ++i) {
		Instruction& inst = instructions[i];
		switch (inst.opcode) {
		case OpName: {
			Name n;
			unsigned id = inst.operands[0];
			n.name = inst.string;
			names[id] = n;
			break;
		}
		case OpTypePointer: {
			Type t;
			unsigned id = inst.operands[0];
			Type subtype = types[inst.operands[2]];
			t.name = subtype.name;
			types[id] = t;
			break;
		}
		case OpTypeFloat: {
			Type t;
			unsigned id = inst.operands[0];
			t.name = "float";
			types[id] = t;
			break;
		}
		case OpTypeVector: {
			Type t;
			unsigned id = inst.operands[0];
			Type subtype = types[inst.operands[1]];
			if (subtype.name != NULL) {
				if (strcmp(subtype.name, "float") == 0 && inst.operands[2] == 2) {
					t.name = "vec2";
					t.length = 2;
					types[id] = t;
				}
				else if (strcmp(subtype.name, "float") == 0 && inst.operands[2] == 3) {
					t.name = "vec3";
					t.length = 3;
					types[id] = t;
				}
				else if (strcmp(subtype.name, "float") == 0 && inst.operands[2] == 4) {
					t.name = "vec4";
					t.length = 4;
					types[id] = t;
				}
			}
			break;
		}
		case OpTypeMatrix: {
			Type t;
			unsigned id = inst.operands[0];
			Type subtype = types[inst.operands[1]];
			if (subtype.name != NULL) {
				if (strcmp(subtype.name, "vec4") == 0 && inst.operands[2] == 4) {
					t.name = "mat4";
					t.length = 4;
					types[id] = t;
				}
			}
			break;
		}
		case OpTypeSampler: {
			Type t;
			unsigned id = inst.operands[0];
			t.name = "sampler2D";
			types[id] = t;
			break;
		}
		case OpConstant: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			out << "const " << resultType.name << " _" << result << " = " << *(float*)&inst.operands[2] << ";\n";
			break;
		}
		case OpVariable: {
			Variable v;
			unsigned id = inst.operands[1]; 
			v.type = inst.operands[0];
			v.storage = (StorageClass)inst.operands[2];
			variables[id] = v;

			Type t = types[v.type];
			Name n = names[id];

			switch (stage) {
			case EShLangVertex:
				if (v.storage == StorageInput) {
					out << "attribute " << t.name << " " << n.name << ";\n";
				}
				else if (v.storage == StorageOutput) {
					out << "varying " << t.name << " " << n.name << ";\n";
				}
				else if (v.storage == StorageConstantUniform) {
					out << "uniform " << t.name << " " << n.name << ";\n";
				}
				break;
			case EShLangFragment:
				if (v.storage == StorageInput) {
					out << "varying " << t.name << " " << n.name << ";\n";
				}
				else if (v.storage == StorageConstantUniform) {
					out << "uniform " << t.name << " " << n.name << ";\n";
				}
				break;
			}
			
			break;
		}
		case OpFunction:
			out << "\nvoid main() {\n";
			break;
		case OpFunctionEnd:
			out << "}\n";
			break;
		case OpCompositeConstruct: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			out << "\t" << resultType.name << " _" << result << " = vec4(_"
			<< inst.operands[2] << ", _" << inst.operands[3] << ", _"
			<< inst.operands[4] << ", _" << inst.operands[5] << ");\n";
			break;
		}
		case OpCompositeExtract: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned composite = inst.operands[2];
			out << "\t" << resultType.name << " _" << result << " = _"
			<< composite << "." << indexName(inst.operands[3]) << ";\n";
			break;
		}
		case OpMatrixTimesVector: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned matrix = inst.operands[2];
			unsigned vector = inst.operands[3];
			out << "\t" << resultType.name << " _" << result << " = _" << matrix << " * _" << vector << ";\n";
			break;
		}
		case OpTextureSample: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned sampler = inst.operands[2];
			unsigned coordinate = inst.operands[3];
			out << "\t" << resultType.name << " _" << result << " = texture2D(_" << sampler << ", _" << coordinate << ");\n";
			break;
		}
		case OpVectorShuffle: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned vector1 = inst.operands[2];
			unsigned vector1length = 4; // types[variables[inst.operands[2]].type].length;
			unsigned vector2 = inst.operands[3];
			unsigned vector2length = 4; // types[variables[inst.operands[3]].type].length;

			out << "\t" << resultType.name << " _" << result << " = " << resultType.name << "(";
			for (unsigned i = 4; i < inst.length; ++i) {
				unsigned index = inst.operands[i];
				if (index < vector1length) out << "_" << vector1 << "." << indexName(index);
				else out << "_" << vector2 << "." << indexName(index - vector1length);
				if (i < inst.length - 1) out << ", ";
			}
			out << ");\n";
			break;
		}
		case OpFMul: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned operand1 = inst.operands[2];
			unsigned operand2 = inst.operands[3];
			out << "\t" << resultType.name << " _" << result << " = _"
			<< operand1 << " * _" << operand2 << ";\n";
			break;
		}
		case OpVectorTimesScalar: {
			Type resultType = types[inst.operands[0]];
			unsigned result = inst.operands[1];
			unsigned vector = inst.operands[2];
			unsigned scalar = inst.operands[3];
			out << "\t" << resultType.name << " _" << result << " = _"
			<< vector << " * _" << scalar << ";\n";
			break;
		}
		case OpReturn:
			out << "\treturn;\n";
			break;
		case OpLabel:
			break;
		case OpBranch:
			break;
		case OpDecorate:
			break;
		case OpTypeFunction:
			break;
		case OpTypeVoid:
			break;
		case OpEntryPoint:
			break;
		case OpMemoryModel:
			break;
		case OpExtInstImport:
			break;
		case OpSource:
			break;
		case OpLoad: {
			Type t = types[inst.operands[0]];
			if (names.find(inst.operands[2]) != names.end()) {
				Name n = names[inst.operands[2]];
				out << "\t" << t.name << " _" << inst.operands[1] << " = " << n.name << ";\n";
			}
			else {
				out << "\t" << t.name << " _" << inst.operands[1] << " = _" << inst.operands[2] << ";\n";
			}
			break;
		}
		case OpStore: {
			Variable v = variables[inst.operands[0]];
			if (stage == EShLangFragment && v.storage == StorageOutput) {
				out << "\tgl_FragColor" << " = _" << inst.operands[1] << ";\n";
			}
			else {
				out << "\t" << names[inst.operands[0]].name << " = _" << inst.operands[1] << ";\n";
			}
			break;
		}
		default:
			out << "Unknown operation " << inst.opcode << ".\n";
		}
	}

	out.close();
}
