/*
CS-UY 2214
Sunny Li
Project 1
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <bitset>
#include <unordered_map>

using namespace std;

// Define global opcodes
const int op_add = 0;		// 000
const int op_sub = 0;		// 000
const int op_or = 0;		// 000
const int op_and = 0;		// 000
const int op_slt = 0;		// 000
const int op_jr = 0;		// 000
const int op_slti = 7;		// 111
const int op_lw = 4;		// 100
const int op_sw = 5;		// 101
const int op_jeq = 6;		// 110
const int op_addi = 1;		// 001
const int op_j = 2;			// 010
const int op_jal = 3;		// 011

// Create global unordered map to hold label values.
unordered_map<string, int> labels;

/**
	print_line(address, num)
	Print a line of machine code in the required format.
	Parameters:
		address = RAM address of the instructions
		num = numeric value of machine instruction 
*/
void print_machine_code(unsigned address, unsigned num) {
	bitset<16> instruction_in_binary(num);
	cout << "ram[" << address << "] = 16'b" << instruction_in_binary <<";"<<endl;
}

// Takes in a string variable line and splits the string into elements
// using a space or a comma as a delimiter. Returns a vector of the separated string elements.
vector<string> parse_line(const string line) {
	vector<string> result;
	string word = "";
	
	for (size_t i = 0; i < line.size(); i++) {
		if (line[i] != ' ' && line[i] != ',') {
			word += line[i];
			if (i == line.size() - 1)
				result.push_back(word);
		}

		else {
			if (word != "")
				result.push_back(word);
			word = "";
		}
	}

	return result;
}

// Takes a string and a char, and checks if that string contains the char.
// Returns a boolean value based on the condition.
bool contains(const string word, char target) {
	for (size_t i = 0; i < word.size(); i++) {
		if (word[i] == target)
			return true;
	}
	return false;
}

// Takes in a string and trims all leading and trailing whitespace.
const string WHITESPACE = " \t";
 
string ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}
 
string trim(const string &s) {
    return rtrim(ltrim(s));
}

// Takes in a string and returns a copy in lowercase
string convert_lower(const string &s) {
	string res = "";
	for (size_t i = 0; i < s.length(); i++)
    	res += tolower(s[i]);

    return res;
}

// Takes in a 2D vector of strings and iterates through every element.
// If a label is detected, stores the value of the label into our unordered map.
// The value of the label is tracked by int pc.
void update_labels(const vector<vector<string>> everything) {
	int pc = 0;
	
	for (size_t i = 0; i < everything.size(); i++) {
		bool add_pc = true;

		for (size_t j = 0; j < everything[i].size(); j++) {
			string word = convert_lower(everything[i][j]);

			if (contains(word, ':')) {
				labels[word.substr(0, word.size()-1)] = pc;
				add_pc = false;	// If label is last element in line, then don't increment pc.
			} else
				add_pc = true;
		}

		if (add_pc)
			pc++;
	}
}

// Takes in a 2D vector of strings and replaces any instances of labels with its corresponding value.
// For example:	addi $1 $0 some_label 
// Will become:	addi $1 $0 5
// Another example:	lw $1 some_label($2)
// Will become:		lw $1 5($2)
void substitute_labels(vector<vector<string>>& everything) {
	for (size_t i = 0; i < everything.size(); i++) {
		for (size_t j = 0; j < everything[i].size(); j++) {
			string &word = everything[i][j];
			string reg = "";

			if (contains(word, ')')) {	// If opcode is lw or sw
				reg = word.substr(word.size()-4, 4);
				word = word.substr(0, word.size()-4);
			}

			unordered_map<string, int>::const_iterator found = labels.find(convert_lower(word));
			if (found != labels.end()) {
				word = to_string(labels[word]) + reg;
			}
			else
				word = word + reg;
		}
	}
}

// Takes in a 2D vector of strings and removes any lines containing only labels.
// For example:
// 		movi $1,10
// 		beginning:
// 			jeq $1 $0 done
// Will become:
// 		movi $1 10
// 		jeq $1 $0 done
void strip_labels(vector<vector<string>>& everything) {
	vector<vector<string>>::iterator row = everything.begin();
	for (size_t i = 0; i < everything.size(); i++) {

		vector<string>::iterator col = everything[i].begin();
		for (size_t j = 0; j < everything[i].size(); j++) {
			
			if (contains(everything[i][j], ':'))
				everything[i].erase(col);

			if (everything[i].size() == 0)	// If line is empty, remove the whole row.
				everything.erase(row);

			col++;
		}

		row++;
	}
}

// Takes in a string representing a register and
// returns an int corresponding to the register.
int reg_to_int(string reg) {
	if (reg == "$0")
		return 0;
	else if (reg == "$1")
		return 1;
	else if (reg == "$2")
		return 2;
	else if (reg == "$3")
		return 3;
	else if (reg == "$4")
		return 4;
	else if (reg == "$5")
		return 5;
	else if (reg == "$6")
		return 6;
	else
		return 7;
}

// Converts a 2D vector of strings into a 1D vector of ints based on the E20 instruction structure.
// For example:	
// 		movi $1 10
// 		jeq $1 $0 4
// 		addi $1 $1 -1
// 		j 1
// 		halt
// Will become:
// 		8330 50178 4294911231 16385 16388
vector<unsigned> program_to_int(const vector<vector<string>>& input) {
	vector<unsigned> result;
	
	for (size_t i = 0; i < input.size(); i++) {
		unsigned instruction_val;
		string operation = convert_lower(input[i][0]);

		// Check for opcodes and translate the program into an integer line by line.
		if (operation == "add") {
			instruction_val = op_add;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][3]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 4;
		}
		else if (operation == "sub") {
			instruction_val = op_sub;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][3]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 4;
			instruction_val = instruction_val | 1;
		}
		else if (operation == "or") {
			instruction_val = op_or;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][3]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 4;
			instruction_val = instruction_val | 2;
		}
		else if (operation == "and") {
			instruction_val = op_and;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][3]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 4;
			instruction_val = instruction_val | 3;
		}
		else if (operation == "slt") {
			instruction_val = op_slt;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][3]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 4;
			instruction_val = instruction_val | 4;
			
		}
		else if (operation == "jr") {
			instruction_val = op_jr;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 10;
			instruction_val = instruction_val | 8;
		}
		else if (operation == "slti") {
			// Use mask in specific opcodes that use imm data to
			// make sure any negative values are represented correctly.
			int mask = (stoi(input[i][3])) < 0 ? 511 << 7 : 0;

			instruction_val = op_slti;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 7;
			instruction_val = instruction_val | (stoi(input[i][3]) ^ mask);
		}
		else if (operation == "lw") {
			instruction_val = op_lw;
			instruction_val = instruction_val << 3;

			string reg = input[i][2].substr(input[i][2].size()-3, 2);
			string imm = input[i][2].substr(0, input[i][2].size()-4);
			int mask = (stoi(imm)) < 0 ? 511 << 7 : 0;

			instruction_val = instruction_val | reg_to_int(reg);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 7;
			instruction_val = instruction_val | (stoi(imm) ^ mask);
		}
		else if (operation == "sw") {
			instruction_val = op_sw;
			instruction_val = instruction_val << 3;

			string reg = input[i][2].substr(input[i][2].size()-3, 2);
			string imm = input[i][2].substr(0, input[i][2].size()-4);
			int mask = (stoi(imm)) < 0 ? 511 << 7 : 0;

			instruction_val = instruction_val | reg_to_int(reg);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 7;
			instruction_val = instruction_val | (stoi(imm) ^ mask);
		}
		else if (operation == "jeq") {
			instruction_val = op_jeq;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 7;

			int imm = stoi(input[i][3]);
			int rel_imm = imm - i - 1;
			int mask = rel_imm < 0 ? 511 << 7 : 0;

			instruction_val = instruction_val | (rel_imm ^ mask);

		}
		else if (operation == "addi") {
			int mask = (stoi(input[i][3])) < 0 ? 511 << 7 : 0;

			instruction_val = op_addi;
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][2]);
			instruction_val = instruction_val << 3;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 7;
			instruction_val = instruction_val | (stoi(input[i][3]) ^ mask);
		}
		else if (operation == "j") {
			instruction_val = op_j;
			instruction_val = instruction_val << 13;
			instruction_val = instruction_val | stoi(input[i][1]);
		}
		else if (operation == "jal") {
			instruction_val = op_jal;
			instruction_val = instruction_val << 13;
			instruction_val = instruction_val | stoi(input[i][1]);
		}
		else if (operation == "movi") {
			int mask = (stoi(input[i][2])) < 0 ? 511 << 7 : 0;

			instruction_val = op_addi;
			instruction_val = instruction_val << 6;
			instruction_val = instruction_val | reg_to_int(input[i][1]);
			instruction_val = instruction_val << 7;
			instruction_val = instruction_val | (stoi(input[i][2]) ^ mask);
		}
		else if (operation == "nop") {
			instruction_val = op_add;
			instruction_val = instruction_val << 13;
		}
		else if (operation == "halt") {
			instruction_val = op_j;
			instruction_val = instruction_val << 13;
			instruction_val = instruction_val | i;
		}
		else if (operation == ".fill") {
			instruction_val = 0;
			instruction_val = instruction_val << 13;
			instruction_val = instruction_val | stoi(input[i][1]);
		}

		result.push_back(instruction_val);
	}

	return result;
}

/**
	Main function
	Takes command-line args as documented below
*/
int main(int argc, char *argv[]) {
	/*
		Parse the command-line arguments
	*/
	char *filename = nullptr;
	bool do_help = false;
	bool arg_error = false;
	for (int i=1; i<argc; i++) {
		string arg(argv[i]);
		if (arg.rfind("-",0)==0) {
			if (arg== "-h" || arg == "--help")
				do_help = true;
			else
				arg_error = true;
		} else {
			if (filename == nullptr)
				filename = argv[i];
			else
				arg_error = true;
		}
	}
	/* Display error message if appropriate */
	if (arg_error || do_help || filename == nullptr) {
		cerr << "usage " << argv[0] << " [-h] filename" << endl << endl; 
		cerr << "Assemble E20 files into machine code" << endl << endl;
		cerr << "positional arguments:" << endl;
		cerr << "  filename    The file containing assembly language, typically with .s suffix" << endl<<endl;
		cerr << "optional arguments:"<<endl;
		cerr << "  -h, --help  show this help message and exit"<<endl;
		return 1;
	}

	/* iterate through the line in the file, construct a list
	   of numeric values representing machine code */
	ifstream f(filename);
	if (!f.is_open()) {
		cerr << "Can't open file "<<filename<<endl;
		return 1;
	}

	/* our final output is a list of ints values representing
	   machine code instructions */
	vector<unsigned> instructions;
	vector<vector<string>> program;
	vector<string> curr_line;
	string line;

	// Set up our 2d vector to hold program elements/keywords.
	// Also set up unordered map for labels.
	while (getline(f, line)) {
		size_t pos = line.find("#");
		if (pos != string::npos)
			line = line.substr(0, pos);

		// Trim leading and trialing whitespace
		line = trim(line);
		
		// If line is not empty then add it into our 2D program vector.
		if (line != "") {
			curr_line = parse_line(line);
			program.push_back(curr_line);
		}
	}

	// Add value of our labels into unordered map.
	update_labels(program);

	// Replace all instances of labels with its value.
	substitute_labels(program);

	// Remove lines that have label definitions.
	strip_labels(program);

	// Change our instructions into ints.
	instructions = program_to_int(program);

	/* print out each instruction in the required format */
	unsigned address = 0;
	for (unsigned instruction : instructions) {
		print_machine_code(address, instruction); 
		address ++;
	}
 
	return 0;
}
