#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <bitset>
#include <algorithm>
#include <cctype> 
using namespace std;

map<string, string> dest_map = {
    {"null", "000"}, {"M", "001"}, {"D", "010"}, {"MD", "011"},
    {"A", "100"}, {"AM", "101"}, {"AD", "110"}, {"ADM", "111"}
};

map<string, string> comp_map = {
    {"0",   "0101010"}, {"1",   "0111111"}, {"-1",  "0111010"},
    {"D",   "0001100"}, {"A",   "0110000"}, {"!D",  "0001101"},
    {"!A",  "0110001"}, {"-D",  "0001111"}, {"-A",  "0110011"},
    {"D+1", "0011111"}, {"A+1", "0110111"}, {"D-1", "0001110"},
    {"A-1", "0110010"}, {"D+A", "0000010"}, {"D-A", "0010011"},
    {"A-D", "0000111"}, {"D&A", "0000000"}, {"D|A", "0010101"},
    {"M",   "1110000"}, {"!M",  "1110001"}, {"-M",  "1110011"},
    {"M+1", "1110111"}, {"M-1", "1110010"}, {"D+M", "1000010"},
    {"D-M", "1010011"}, {"M-D", "1000111"}, {"D&M", "1000000"},
    {"D|M", "1010101"}
};

map<string, string> jump_map = {
    {"null", "000"}, {"JGT", "001"}, {"JEQ", "010"}, {"JGE", "011"},
    {"JLT", "100"}, {"JNE", "101"}, {"JLE", "110"}, {"JMP", "111"}
};

map<string, int> symbols = {
    {"R0", 0}, {"R1", 1}, {"R2", 2}, {"R3", 3}, {"R4", 4}, {"R5", 5},
    {"R6", 6}, {"R7", 7}, {"R8", 8}, {"R9", 9}, {"R10", 10}, {"R11", 11},
    {"R12", 12}, {"R13", 13}, {"R14", 14}, {"R15", 15},
    {"SCREEN", 16384}, {"KBD", 24576},
    {"SP", 0}, {"LCL", 1}, {"ARG", 2}, {"THIS", 3}, {"THAT", 4}
};


// --- Helper Functions ---

/**
 * @brief Removes comments and all whitespace from a given line.
 * @param line The raw line from the input file.
 * @return A cleaned string with no comments or whitespace.
 */
// this remove the comments or whitespace
string clean_line(const string& line) {
    string result = line;
    // Find and remove comments
    size_t comment_pos = result.find("//");
    if (comment_pos != string::npos) {
        result = result.substr(0, comment_pos);
    }
    // Remove all whitespace characters
    result.erase(remove_if(result.begin(), result.end(), ::isspace), result.end());
    return result;
}

/**
 * @brief Checks if a string contains only digits.
 * @param s The string to check.
 * @return True if the string is a number, false otherwise.
 */
bool is_number(const string& s) {
    return !s.empty() && all_of(s.begin(), s.end(), ::isdigit);
}

/**
 * @brief Parses and translates an A-instruction into a 16-bit binary string.
 * @param symbol_str The symbol or immediate value from the A-instruction.
 * @return The 16-bit binary machine code.
 */ 
string parse_A_instruction(const string& symbol_str) {
    int value = 0;
    if (is_number(symbol_str)) {
        value = stoi(symbol_str);
    } else {
        // It's a symbol, look it up in the table.
        value = symbols[symbol_str];
    }
    // Convert the integer value to a 16-bit binary string
    return bitset<16>(value).to_string();
}

/**
 * @brief Parses and translates a C-instruction into a 16-bit binary string.
 * @param line The cleaned C-instruction line.
 * @return The 16-bit binary machine code.
 */
string parse_C_instruction(const string& line) {
    string dest_str = "null";
    string comp_str;
    string jump_str = "null";

    size_t eq_pos = line.find('=');
    size_t sc_pos = line.find(';');

    if (eq_pos != string::npos) { // Format: dest=comp or dest=comp;jump
        dest_str = line.substr(0, eq_pos);
        if (sc_pos != string::npos) {
            comp_str = line.substr(eq_pos + 1, sc_pos - (eq_pos + 1));
            jump_str = line.substr(sc_pos + 1);
        } else {
            comp_str = line.substr(eq_pos + 1);
        }
    } else if (sc_pos != string::npos) { // Format: comp;jump
        comp_str = line.substr(0, sc_pos);
        jump_str = line.substr(sc_pos + 1);
    }

    string dest_bin = dest_map[dest_str];
    string comp_bin = comp_map[comp_str];
    string jump_bin = jump_map[jump_str];

    return "111" + comp_bin + dest_bin + jump_bin;
}


// --- Main Execution ---

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input_file.asm>" << endl;
        return 1;
    }

    string import_file = argv[1];
    cout << "Assembler running on file " << import_file << endl;
    
    ifstream infile;
    string line;

    // --- Pass 1: Build Label Symbol Table ---
    // Scans for (LABEL) declarations and adds them to the symbol table.
    int program_position = 0;
    infile.open(import_file);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file " << import_file << endl;
        return 1;
    }
    while (getline(infile, line)) {
        string cleaned = clean_line(line);
        if (cleaned.empty()) continue;
        if (cleaned[0] == '(') {
            string label = cleaned.substr(1, cleaned.length() - 2);
            symbols[label] = program_position;
        } else {
            program_position++;
        }
    }
    infile.close();

    // --- Pass 2: Handle Variables & Assemble ---
    // Reads the file again, handles variables, and writes the machine code.
    string output_file = "final.hack";
    ofstream outfile(output_file);
    infile.open(import_file);
    if (!infile.is_open()) {
        cerr << "Error: Could not open file " << import_file << endl;
        return 1;
    }

    int start_memory = 16; // RAM addresses for new variables start at 16
    while (getline(infile, line)) {
        string cleaned = clean_line(line);
        if (cleaned.empty() || cleaned[0] == '(') {
            continue;
        }
        string binary_line;
        if (cleaned[0] == '@') {
            // A-Instruction
            string symbol = cleaned.substr(1);
            if (!is_number(symbol)) {
                // If it's a symbol, check if it's already in the table
                if (symbols.find(symbol) == symbols.end()) {
                    // If not, it's a new variable. Add it.
                    symbols[symbol] = start_memory;
                    start_memory++;
                }
            }
            binary_line = parse_A_instruction(symbol);
        } else {
            // C-Instruction
            binary_line = parse_C_instruction(cleaned);
        }
        outfile << binary_line << '\n';
    }
    infile.close();
    outfile.close();

    cout << "Assembler completed successfully: output is " << output_file << endl;

    return 0;
}