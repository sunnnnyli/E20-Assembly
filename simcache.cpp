/*
Sunny Li
Project 3
simcache.cpp
*/

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <iomanip>
#include <regex>
#include <deque>

using namespace std;

/*
	Prints out the correctly-formatted configuration of a cache.

	@param cache_name The name of the cache. "L1" or "L2"

	@param size The total size of the cache, measured in memory cells.
		Excludes metadata

	@param assoc The associativity of the cache. One of [1,2,4,8,16]

	@param blocksize The blocksize of the cache. One of [1,2,4,8,16,32,64])

	@param num_rows The number of rows in the given cache.
*/
void print_cache_config(const string &cache_name, int size, int assoc, int blocksize, int num_rows) {
	cout << "Cache " << cache_name << " has size " << size <<
		", associativity " << assoc << ", blocksize " << blocksize <<
		", rows " << num_rows << endl;
}

/*
	Prints out a correctly-formatted log entry.

	@param cache_name The name of the cache where the event
		occurred. "L1" or "L2"

	@param status The kind of cache event. "SW", "HIT", or
		"MISS"

	@param pc The program counter of the memory
		access instruction

	@param addr The memory address being accessed.

	@param row The cache row or set number where the data
		is stored.
*/
void print_log_entry(const string &cache_name, const string &status, int pc, int addr, int row) {
	cout << left << setw(8) << cache_name + " " + status <<  right <<
		" pc:" << setw(5) << pc <<
		"\taddr:" << setw(5) << addr <<
		"\trow:" << setw(4) << row << endl;
}

// Helpful constants
size_t const static NUM_REGS = 8; 
size_t const static MEM_SIZE = 1<<13;
size_t const static REG_SIZE = 1<<16;

// Initialize the processor state with global variables
unsigned pc = 0;
unsigned registers[NUM_REGS] = {};	// initialize every value to 0
unsigned memory[MEM_SIZE] = {};	// initialize every value to 0
vector<vector<int>> L1cache;
vector<vector<int>> L2cache;
vector<vector<int>> L1blockdata;
vector<vector<int>> L2blockdata;
vector<deque<int>> L1MRU;	// Vector of int deques to keep track of most recently used data in L1
vector<deque<int>> L2MRU;	// Same for L2

// Define global opcodes
const unsigned op_add = 0;		// 000
const unsigned op_sub = 0;		// 000
const unsigned op_or = 0;		// 000
const unsigned op_and = 0;		// 000
const unsigned op_slt = 0;		// 000
const unsigned op_jr = 0;		// 000
const unsigned op_slti = 7;		// 111
const unsigned op_lw = 4;		// 100
const unsigned op_sw = 5;		// 101
const unsigned op_jeq = 6;		// 110
const unsigned op_addi = 1;		// 001
const unsigned op_j = 2;		// 010
const unsigned op_jal = 3;		// 011

/*
	Loads an E20 machine code file into the list
	provided by mem. We assume that mem is
	large enough to hold the values in the machine
	code file.

	@param f Open file to read from
	@param mem Array represetnting memory into which to read program
*/
void load_machine_code(ifstream &f, unsigned mem[]) {
	regex machine_code_re("^ram\\[(\\d+)\\] = 16'b(\\d+);.*$");
	size_t expectedaddr = 0;
	string line;
	while (getline(f, line)) {
		smatch sm;
		if (!regex_match(line, sm, machine_code_re)) {
			cerr << "Can't parse line: " << line << endl;
			exit(1);
		}
		size_t addr = stoi(sm[1], nullptr, 10);
		unsigned instr = stoi(sm[2], nullptr, 2);
		if (addr != expectedaddr) {
			cerr << "Memory addresses encountered out of sequence: " << addr << endl;
			exit(1);
		}
		if (addr >= MEM_SIZE) {
			cerr << "Program too big for memory" << endl;
			exit(1);
		}
		expectedaddr ++;
		mem[addr] = instr;
	}
}

/*
	Prints the current state of the simulator, including
	the current program counter, the current register values,
	and the first memquantity elements of memory.

	@param pc The final value of the program counter
	@param regs Final value of all registers
	@param memory Final value of memory
	@param memquantity How many words of memory to dump
*/
void print_state(unsigned pc, unsigned regs[], unsigned memory[], size_t memquantity) {
	cout << setfill(' ');
	cout << "Final state:" << endl;
	cout << "\tpc=" <<setw(5)<< pc << endl;

	for (size_t reg=0; reg<NUM_REGS; reg++)
		cout << "\t$" << reg << "="<<setw(5)<<regs[reg]<<endl;

	cout << setfill('0');
	bool cr = false;
	for (size_t count=0; count<memquantity; count++) {
		cout << hex << setw(4) << memory[count] << " ";
		cr = true;
		if (count % 8 == 7) {
			cout << endl;
			cr = false;
		}
	}
	if (cr)
		cout << endl;
}

// Takes an unsigned int representing an E20 instruction
// Returns the most significant 3 bits (op code)
unsigned find_opcode(unsigned instruction) {
	return (instruction >> 13);
}

// Takes a binary number and converts it into signed binary if applicable
// Returns the new 7 bit signed binary value
int to_signed_binary(int imm) {
	int res = ~(imm);
	res = res & 127;
	res++;
	return -1 * res;
}

// Takes an optional int input and increments the pc
// Automatically "wraps" pc if it's too large for memory
// Usually, pc += inc shouldn't be negative. However, if it is, the unsigned pc variable will
// wrap around its range, and pc will be changed into UINT_MAX - some_number (which is positive).
// And so, doing pc % MEM_SIZE will still yield us the same remainder.
void increment_pc(int inc = 1) {
	pc += inc;

	if (pc > MEM_SIZE - 1)
		pc %= MEM_SIZE;
}

// Takes an int input and sets the pc to that value
// Automatically "wraps" pc if it's too large for memory
void set_pc(int new_pc) {
	pc = new_pc;

	if (pc > MEM_SIZE - 1)
		pc %= MEM_SIZE;
}

// Takes an int representing L1 or L2 cache, an int representing the row that we're considering,
// an int indicating the block that was accessed, and another int indicating
// the associativity of cache the deque corresponds to.
// Function will update the deque by pushing MRU data to rightmost side of deque
// while also removing any duplicates of it currently in the deque.
// This means that the LRU data is at the leftmost side of the deque
void updateMRU(int cache_num, int row, int block_in_row, int assoc) {
	if (cache_num == 1) {
		// Remove duplicate
		for (auto it = L1MRU[row].begin(); it != L1MRU[row].end();) {
			if (*it == block_in_row)
				it = L1MRU[row].erase(it);
			else
				it++;
		}

		// Push back newly accessed data
		L1MRU[row].push_back(block_in_row);

		// If deque has more elements than possible, pop first element
		if (L1MRU[row].size() > assoc)
			L1MRU[row].pop_front();
	} else if (cache_num == 2) {
		for (auto it = L2MRU[row].begin(); it != L2MRU[row].end();) {
			if (*it == block_in_row)
				it = L2MRU[row].erase(it);
			else
				it++;
		}

		L2MRU[row].push_back(block_in_row);

		if (L2MRU[row].size() > assoc)
			L2MRU[row].pop_front();
	} else {
		cout << "Not a valid cache" << endl;
	}
}

// Takes an unsigned int representing an E20 instruction
// Returns true if the instruction executed is halt
// Performs the instruction and updates the global pc and registers variable accordingly
bool execute_instruction(unsigned instruction, int blocks1, int assoc1, int blocks2 = 0, int assoc2 = 0) {
	unsigned op_code = find_opcode(instruction);
	unsigned regSrcA;
	unsigned regSrcB;
	unsigned regDst;
	int imm;
	// cout << "pc: " << pc << endl;

	if (op_code == op_add && ((instruction & 15) == 0)) {	// check opcode and least sig 4 bits
		regSrcA = (instruction >> 10) & 7;	// isolate least significant 3 bits
		regSrcB = (instruction >> 7) & 7;
		regDst = (instruction >> 4) & 7;

		if (regDst != 0)	// if we are not modifying register 0
			registers[regDst] = (registers[regSrcA] + registers[regSrcB]) & 65535;	// trim result to 16 bits

		increment_pc();
		return false;
	}

	else if (op_code == op_sub && ((instruction & 15) == 1)) {
		regSrcA = (instruction >> 10) & 7;
		regSrcB = (instruction >> 7) & 7;
		regDst = (instruction >> 4) & 7;

		if (regDst != 0)
			registers[regDst] = (registers[regSrcA] - registers[regSrcB]) & 65535;

		increment_pc();
		return false;
	}

	else if (op_code == op_or && ((instruction & 15) == 2)) {
		regSrcA = (instruction >> 10) & 7;
		regSrcB = (instruction >> 7) & 7;
		regDst = (instruction >> 4) & 7;

		if (regDst != 0)
			registers[regDst] = (registers[regSrcA] | registers[regSrcB]) & 65535;

		increment_pc();
		return false;
	}

	else if (op_code == op_and && ((instruction & 15) == 3)) {
		regSrcA = (instruction >> 10) & 7;
		regSrcB = (instruction >> 7) & 7;
		regDst = (instruction >> 4) & 7;

		if (regDst != 0)
			registers[regDst] = (registers[regSrcA] & registers[regSrcB]) & 65535;

		increment_pc();
		return false;
	}

	else if (op_code == op_slt && ((instruction & 15) == 4)) {
		regSrcA = (instruction >> 10) & 7;
		regSrcB = (instruction >> 7) & 7;
		regDst = (instruction >> 4) & 7;

		if (regDst != 0) {
			if (registers[regSrcA] < registers[regSrcB])
				registers[regDst] = 1;
			else
				registers[regDst] = 0;
		}

		increment_pc();
		return false;
	}

	else if (op_code == op_jr && ((instruction & 15) == 8)) {
		regSrcA = (instruction >> 10) & 7;
		set_pc(registers[regSrcA]);
		return false;
	}

	else if (op_code == op_slti) {
		regSrcA = (instruction >> 10) & 7;
		regDst = (instruction >> 7) & 7;
		imm = instruction & 127;	// isolate least significant 7 bits

		if (imm > 63)			// if 7th most sig bit is 1, then
			imm = imm | 65408;	// sign extend 7 bit immediate to 16 bits (using 1111111110000000 mask)

		if (regDst != 0) {
			if (registers[regSrcA] < imm)
				registers[regDst] = 1;
			else
				registers[regDst] = 0;
		}

		increment_pc();
		return false;
	}

	else if (op_code == op_lw) {
		regSrcA = (instruction >> 10) & 7;
		regDst = (instruction >> 7) & 7;
		imm = instruction & 127;

		if (imm > 63)	// if 7th most sig bit is 1, then negate
			imm = to_signed_binary(imm);

		unsigned pointer = (registers[regSrcA] + imm) & 8191;	// only care about least sig 13 bits
		// mask will ensure the pointer points to a valid memory address (it will always be < 8192)

		/*Start of cache simulation*/
		string status = "";
		int L1row = (pointer / blocks1) % (L1cache.size() / assoc1); // L1cache.size() / assoc1 = number of rows
		int L1tag = (pointer / blocks1) / (L1cache.size() / assoc1);

		// Check if hit in L1 cache
		for (size_t i = 0; i < assoc1; i++) {
			if (L1cache[L1row * assoc1 + i][1] != 0) {
				if (L1cache[L1row * assoc1 + i][2] == L1tag) {	// If tag is equal
					status = "HIT";
					print_log_entry("L1", status, pc, pointer, L1row);

					if (regDst != 0)
						registers[regDst] = L1blockdata[L1row * assoc1 + i][pointer % blocks1];	// Fetch data from cache

					updateMRU(1, L1row, L1row * assoc1 + i, assoc1);
					increment_pc();
					return false;
				}
			}
		}

		// No hits in L1 cache, so print miss log entry for L1 cache
		status = "MISS";
		print_log_entry("L1", status, pc, pointer, L1row);

		// Now check L2 cache (if available) for any hits
		if (blocks2 != 0) {
			int L2row = (pointer / blocks2) % (L2cache.size() / assoc2);
			int L2tag = (pointer / blocks2) / (L2cache.size() / assoc2);

			// Check if hit in L2 cache
			for (size_t i = 0; i < assoc2; i++) {
				if (L2cache[L2row * assoc2 + i][1] != 0) {
					if (L2cache[L2row * assoc2 + i][2] == L2tag) {
						status = "HIT";
						print_log_entry("L2", status, pc, pointer, L2row);

						if (regDst != 0)
							registers[regDst] = L2blockdata[L2row * assoc2 + i][pointer % blocks2];

						updateMRU(2, L2row, L2row * assoc2 + i, assoc2);
						increment_pc();
						return false;
					}
				}
			}

			// No hits in L2 cache, so print miss log entry for L2 cache
			print_log_entry("L2", status, pc, pointer, L2row);
		}

		// Now we have to fetch data from RAM and write to cache
		bool written = false;
		if (regDst != 0)
			registers[regDst] = memory[pointer];

		// Write to L2 cache
		if (blocks2 != 0) {
			int L2row = (pointer / blocks2) % (L2cache.size() / assoc2);
			int L2tag = (pointer / blocks2) / (L2cache.size() / assoc2);

			// Fetch all data from that block in RAM
			vector<int> blockdata_for_L2;
			for (size_t i = 0; i < blocks2; i++) {
				blockdata_for_L2.push_back(memory[(pointer / blocks2) * blocks2 + i]);
			}

			// Check for free blocks in L2 cache
			for (size_t i = 0; i < assoc2; i++) {	
				if (L2cache[L2row * assoc2 + i][1] == 0) {
					// Write to L2 cache
					L2cache[L2row * assoc2 + i][1] = 1;
					L2cache[L2row * assoc2 + i][2] = L2tag;
					L2blockdata[L2row * assoc2 + i] = blockdata_for_L2;

					written = true;
					updateMRU(2, L2row, L2row * assoc2 + i, assoc2);
					break;
				}
			}

			// If no free blocks in L2 cache
			if (!written) {
				int LRU_data = L2MRU[L2row].front();

				// Write to L2 cache
				L2cache[LRU_data][2] = L2tag;
				L2blockdata[LRU_data] = blockdata_for_L2;

				updateMRU(2, L2row, LRU_data, assoc2);
			}
		}

		// Reset written boolean
		written = false;

		// Fetching data from block in RAM
		vector<int> blockdata_for_L1;
		for (size_t i = 0; i < blocks1; i++) {
			blockdata_for_L1.push_back(memory[(pointer / blocks1) * blocks1 + i]);
		}

		// Check for free blocks in L1 cache
		for (size_t i = 0; i < assoc1; i++) {	
			if (L1cache[L1row * assoc1 + i][1] == 0) {
				// Write to L1 cache
				L1cache[L1row * assoc1 + i][1] = 1;
				L1cache[L1row * assoc1 + i][2] = L1tag;
				L1blockdata[L1row * assoc1 + i] = blockdata_for_L1;

				written = true;
				updateMRU(1, L1row, L1row * assoc1 + i, assoc1);
				break;
			}
		}

		// If no free blocks in L1 cache
		if (!written) {
			int LRU_data = L1MRU[L1row].front();

			// Write to L1 cache
			L1cache[LRU_data][2] = L1tag;
			L1blockdata[LRU_data] = blockdata_for_L1;

			updateMRU(1, L1row, LRU_data, assoc1);
		}
		
		increment_pc();
		return false;
	}

	else if (op_code == op_sw) {
		regSrcA = (instruction >> 10) & 7;
		regDst = (instruction >> 7) & 7;
		imm = instruction & 127;

		if (imm > 63)
			imm = to_signed_binary(imm);

		unsigned pointer = (registers[regSrcA] + imm) & 8191;

		memory[pointer] = registers[regDst];

		/*Start of cache simulation*/
		int L1row = (pointer / blocks1) % (L1cache.size() / assoc1);
		int L1tag = (pointer / blocks1) / (L1cache.size() / assoc1);

		bool written = false;

		// Fetch data from block in RAM
		vector<int> blockdata_for_L1;
		for (size_t i = 0; i < blocks1; i++) {
			blockdata_for_L1.push_back(memory[(pointer / blocks1) * blocks1 + i]);
		}

		// Check for free blocks in L1 cache
		for (size_t i = 0; i < assoc1; i++) {	
			if (L1cache[L1row * assoc1 + i][1] == 0) {
				// Write to L1 cache
				L1cache[L1row * assoc1 + i][1] = 1;
				L1cache[L1row * assoc1 + i][2] = L1tag;
				L1blockdata[L1row * assoc1 + i] = blockdata_for_L1;

				written = true;
				print_log_entry("L1", "SW", pc, pointer, L1row);
				updateMRU(1, L1row, L1row * assoc1 + i, assoc1);
				break;
			}
		}

		// If no free blocks in L1 cache
		if (!written) {
			int LRU_data = L1MRU[L1row].front();

			// Write to L1 cache
			L1cache[LRU_data][2] = L1tag;
			L1blockdata[LRU_data] = blockdata_for_L1;

			print_log_entry("L1", "SW", pc, pointer, L1row);
			updateMRU(1, L1row, LRU_data, assoc1);
		}

		// Write to L2 cache if it exists
		if (blocks2 != 0) {
			int L2row = (pointer / blocks2) % (L2cache.size() / assoc2);
			int L2tag = (pointer / blocks2) / (L2cache.size() / assoc2);

			written = false;

			vector<int> blockdata_for_L2;
			for (size_t i = 0; i < blocks2; i++) {
				blockdata_for_L2.push_back(memory[(pointer / blocks2) * blocks2 + i]);
			}

			// Check for free blocks in L2 cache
			for (size_t i = 0; i < assoc2; i++) {	
				if (L2cache[L2row * assoc2 + i][1] == 0) {
					// Write to L2 cache
					L2cache[L2row * assoc2 + i][1] = 1;
					L2cache[L2row * assoc2 + i][2] = L2tag;
					L2blockdata[L2row * assoc2 + i] = blockdata_for_L2;

					written = true;
					print_log_entry("L2", "SW", pc, pointer, L2row);
					updateMRU(2, L2row, L2row * assoc2 + i, assoc2);
					break;
				}
			}

			// If no free blocks in L2 cache
			if (!written) {
				int LRU_data = L2MRU[L2row].front();

				// Write to L2 cache
				L2cache[LRU_data][2] = L2tag;
				L2blockdata[LRU_data] = blockdata_for_L2;

				print_log_entry("L2", "SW", pc, pointer, L2row);
				updateMRU(2, L2row, LRU_data, assoc2);
			}
		}

		increment_pc();
		return false;
	}

	else if (op_code == op_jeq) {
		regSrcA = (instruction >> 10) & 7;
		regSrcB = (instruction >> 7) & 7;
		imm = instruction & 127;

		if (imm > 63)
			imm = to_signed_binary(imm);

		if (registers[regSrcA] == registers[regSrcB])
			increment_pc(1 + imm);
		else
			increment_pc();

		return false;
	}

	else if (op_code == op_addi) {
		regSrcA = (instruction >> 10) & 7;
		regDst = (instruction >> 7) & 7;
		imm = instruction & 127;

		if (imm > 63)
			imm = to_signed_binary(imm);

		if (regDst != 0)
			registers[regDst] = (registers[regSrcA] + imm) & 65535;

		increment_pc();
		return false;
	}

	else if (op_code == op_j) {
		imm = instruction & 8191;	// isolate least significant 13 bits

		if (pc == imm)	// if instruction is halt, do nothing to pc
			return true;
		else {
			set_pc(imm);
			return false;
		}
	}

	else if (op_code == op_jal) {
		imm = instruction & 8191;

		registers[7] = pc + 1;
		set_pc(imm);
		return false;
	}

	else {
		cout << "invalid instruction at pc: " << pc << endl;
		return false;
	}
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
	string cache_config;
	for (int i=1; i<argc; i++) {
		string arg(argv[i]);
		if (arg.rfind("-",0)==0) {
			if (arg== "-h" || arg == "--help")
				do_help = true;
			else if (arg=="--cache") {
				i++;
				if (i>=argc)
					arg_error = true;
				else
					cache_config = argv[i];
			}
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
		cerr << "usage " << argv[0] << " [-h] [--cache CACHE] filename" << endl << endl; 
		cerr << "Simulate E20 cache" << endl << endl;
		cerr << "positional arguments:" << endl;
		cerr << "  filename    The file containing machine code, typically with .bin suffix" << endl<<endl;
		cerr << "optional arguments:"<<endl;
		cerr << "  -h, --help  show this help message and exit"<<endl;
		cerr << "  --cache CACHE  Cache configuration: size,associativity,blocksize (for one"<<endl;
		cerr << "                 cache) or"<<endl;
		cerr << "                 size,associativity,blocksize,size,associativity,blocksize"<<endl;
		cerr << "                 (for two caches)"<<endl;
		return 1;
	}

	// Open file
	ifstream f(filename);
	if (!f.is_open()) {
		cerr << "Can't open file "<< filename << endl;
		return 1;
	}

	// Load f and parse using load_machine_code
	load_machine_code(f, memory);

	/* parse cache config */
	if (cache_config.size() > 0) {
		vector<int> parts;
		size_t pos;
		size_t lastpos = 0;
		while ((pos = cache_config.find(",", lastpos)) != string::npos) {
			parts.push_back(stoi(cache_config.substr(lastpos,pos)));
			lastpos = pos + 1;
		}
		parts.push_back(stoi(cache_config.substr(lastpos)));
		if (parts.size() == 3) {
			int L1size = parts[0];
			int L1assoc = parts[1];
			int L1blocksize = parts[2];
			int L1rows = L1size / (L1assoc * L1blocksize);

			// Create our cache as global 2D vector with columns "Row", "V", and "Tag"
			// Need a "Row" column for n-way set-associative caches
			for (int i = 0; i < L1rows; i++) {
				for (size_t j = 0; j < L1assoc; j++) {
					vector<int> row = {i, 0, 0, 0};
					L1cache.push_back(row);
					
					vector<int> blockdata;
					L1blockdata.push_back(blockdata);
				}

				deque<int> rowMRU;
				L1MRU.push_back(rowMRU);
			}

			print_cache_config("L1", L1size, L1assoc, L1blocksize, L1rows);

			// TODO: execute E20 program and simulate one cache here
			bool halt = false;
			while (!halt) {
				halt = execute_instruction(memory[pc], L1blocksize, L1assoc);
			}
		} else if (parts.size() == 6) {
			int L1size = parts[0];
			int L1assoc = parts[1];
			int L1blocksize = parts[2];
			int L2size = parts[3];
			int L2assoc = parts[4];
			int L2blocksize = parts[5];
			int L1rows = L1size / (L1assoc * L1blocksize);
			int L2rows = L2size / (L2assoc * L2blocksize);

			// Initialize our cache as 2D array with columns "Row" "V", and "Tag"
			for (int i = 0; i < L1rows; i++) {
				for (size_t j = 0; j < L1assoc; j++) {
					vector<int> row = {i, 0, 0};
					L1cache.push_back(row);

					vector<int> blockdata;
					L1blockdata.push_back(blockdata);
				}

				deque<int> rowMRU;
				L1MRU.push_back(rowMRU);
			}
			for (int i = 0; i < L2rows; i++) {
				for (size_t j = 0; j < L2assoc; j++) {
					vector<int> row = {i, 0, 0};
					L2cache.push_back(row);

					vector<int> blockdata;
					L2blockdata.push_back(blockdata);
				}

				deque<int> rowMRU;
				L2MRU.push_back(rowMRU);
			}

			print_cache_config("L1", L1size, L1assoc, L1blocksize, L1rows);
			print_cache_config("L2", L2size, L2assoc, L2blocksize, L2rows);

			// TODO: execute E20 program and simulate two caches here
			bool halt = false;
			while (!halt) {
				halt = execute_instruction(memory[pc], L1blocksize, L1assoc, L2blocksize, L2assoc);
			}
		} else {
			cerr << "Invalid cache config"  << endl;
			return 1;
		}
	}

	// Print the final state of the simulator before ending, using print_state
	// print_state(pc, registers, memory, 128);
	return 0;
}
