#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

// Enum for command types, used by Parser and CodeWriter
enum CommandType {
    C_ARITHMETIC,
    C_PUSH,
    C_POP,
    C_LABEL,
    C_GOTO,
    C_IF,
    C_FUNCTION,
    C_RETURN,
    C_CALL
};

// --- Class Definitions ---

// CodeWriter class handles writing assembly code to the output file.
class CodeWriter {
public:
    CodeWriter(const std::string& filename);
    void writeArithmetic(const std::string& command);
    void writePushPop(CommandType command, const std::string& segment, int index);
    void close();

private:
    std::ofstream output_file;
    int jump_label_count;
    std::string file_name;
};

// Parser class handles reading and parsing a single .vm file.
class Parser {
public:
    Parser(const std::string& filename);
    bool hasMoreCommands();
    void advance();
    CommandType commandType();
    std::string arg1();
    int arg2();

private:
    std::ifstream input_file;
    std::string current_command;
    std::vector<std::string> commands;
    int command_index;
};


// --- Class Implementations ---

// CodeWriter Implementation
CodeWriter::CodeWriter(const std::string& filename) : jump_label_count(0) {
    output_file.open(filename);
    file_name = fs::path(filename).stem().string();
}

void CodeWriter::writeArithmetic(const std::string& command) {
    output_file << "// " << command << std::endl;
    if (command == "add") {
        output_file << "@SP" << std::endl;
        output_file << "AM=M-1" << std::endl;
        output_file << "D=M" << std::endl;
        output_file << "A=A-1" << std::endl;
        output_file << "M=D+M" << std::endl;
    } else if (command == "sub") {
        output_file << "@SP" << std::endl;
        output_file << "AM=M-1" << std::endl;
        output_file << "D=M" << std::endl;
        output_file << "A=A-1" << std::endl;
        output_file << "M=M-D" << std::endl;
    } else if (command == "neg") {
        output_file << "@SP" << std::endl;
        output_file << "A=M-1" << std::endl;
        output_file << "M=-M" << std::endl;
    } else if (command == "eq" || command == "gt" || command == "lt") {
        output_file << "@SP" << std::endl;
        output_file << "AM=M-1" << std::endl;
        output_file << "D=M" << std::endl;
        output_file << "A=A-1" << std::endl;
        output_file << "D=M-D" << std::endl;
        output_file << "@TRUE" << jump_label_count << std::endl;
        if (command == "eq") output_file << "D;JEQ" << std::endl;
        if (command == "gt") output_file << "D;JGT" << std::endl;
        if (command == "lt") output_file << "D;JLT" << std::endl;
        output_file << "@SP" << std::endl;
        output_file << "A=M-1" << std::endl;
        output_file << "M=0" << std::endl;
        output_file << "@END" << jump_label_count << std::endl;
        output_file << "0;JMP" << std::endl;
        output_file << "(TRUE" << jump_label_count << ")" << std::endl;
        output_file << "@SP" << std::endl;
        output_file << "A=M-1" << std::endl;
        output_file << "M=-1" << std::endl;
        output_file << "(END" << jump_label_count << ")" << std::endl;
        jump_label_count++;
    } else if (command == "and") {
        output_file << "@SP" << std::endl;
        output_file << "AM=M-1" << std::endl;
        output_file << "D=M" << std::endl;
        output_file << "A=A-1" << std::endl;
        output_file << "M=D&M" << std::endl;
    } else if (command == "or") {
        output_file << "@SP" << std::endl;
        output_file << "AM=M-1" << std::endl;
        output_file << "D=M" << std::endl;
        output_file << "A=A-1" << std::endl;
        output_file << "M=D|M" << std::endl;
    } else if (command == "not") {
        output_file << "@SP" << std::endl;
        output_file << "A=M-1" << std::endl;
        output_file << "M=!M" << std::endl;
    }
}

void CodeWriter::writePushPop(CommandType commandType, const std::string& segment, int index) {
    output_file << "// " << (commandType == C_PUSH ? "push" : "pop") << " " << segment << " " << index << std::endl;
    if (segment == "constant") {
        output_file << "@" << index << std::endl;
        output_file << "D=A" << std::endl;
        output_file << "@SP" << std::endl;
        output_file << "A=M" << std::endl;
        output_file << "M=D" << std::endl;
        output_file << "@SP" << std::endl;
        output_file << "M=M+1" << std::endl;
    } else if (segment == "local" || segment == "argument" || segment == "this" || segment == "that") {
        std::string seg_symbol;
        if (segment == "local") seg_symbol = "LCL";
        if (segment == "argument") seg_symbol = "ARG";
        if (segment == "this") seg_symbol = "THIS";
        if (segment == "that") seg_symbol = "THAT";

        if (commandType == C_PUSH) {
            output_file << "@" << seg_symbol << std::endl;
            output_file << "D=M" << std::endl;
            output_file << "@" << index << std::endl;
            output_file << "A=D+A" << std::endl;
            output_file << "D=M" << std::endl;
            output_file << "@SP" << std::endl;
            output_file << "A=M" << std::endl;
            output_file << "M=D" << std::endl;
            output_file << "@SP" << std::endl;
            output_file << "M=M+1" << std::endl;
        } else { // C_POP
            output_file << "@" << seg_symbol << std::endl;
            output_file << "D=M" << std::endl;
            output_file << "@" << index << std::endl;
            output_file << "D=D+A" << std::endl;
            output_file << "@R13" << std::endl;
            output_file << "M=D" << std::endl;
            output_file << "@SP" << std::endl;
            output_file << "AM=M-1" << std::endl;
            output_file << "D=M" << std::endl;
            output_file << "@R13" << std::endl;
            output_file << "A=M" << std::endl;
            output_file << "M=D" << std::endl;
        }
    } else if (segment == "static" || segment == "temp" || segment == "pointer") {
        std::string base_address;
        if (segment == "static") base_address = std::to_string(16 + index);
        if (segment == "temp") base_address = std::to_string(5 + index);
        if (segment == "pointer") base_address = (index == 0 ? "THIS" : "THAT");

        if (commandType == C_PUSH) {
            if (segment == "pointer") output_file << "@" << (index == 0 ? "3" : "4") << std::endl;
            else output_file << "@" << base_address << std::endl;
            output_file << "D=M" << std::endl;
            output_file << "@SP" << std::endl;
            output_file << "A=M" << std::endl;
            output_file << "M=D" << std::endl;
            output_file << "@SP" << std::endl;
            output_file << "M=M+1" << std::endl;
        } else { // C_POP
             output_file << "@SP" << std::endl;
             output_file << "AM=M-1" << std::endl;
             output_file << "D=M" << std::endl;
             if (segment == "pointer") output_file << "@" << (index == 0 ? "3" : "4") << std::endl;
             else output_file << "@" << base_address << std::endl;
             output_file << "M=D" << std::endl;
        }
    }
}

void CodeWriter::close() {
    output_file << "(END)" << std::endl;
    output_file << "@END" << std::endl;
    output_file << "0;JMP" << std::endl;
    output_file.close();
}

// Parser Implementation
Parser::Parser(const std::string& filename) : command_index(-1) {
    input_file.open(filename);
    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(input_file, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (!line.empty()) {
            commands.push_back(line);
        }
    }
}

bool Parser::hasMoreCommands() {
    return command_index < (int)commands.size() - 1;
}

void Parser::advance() {
    if (hasMoreCommands()) {
        command_index++;
        current_command = commands[command_index];
    }
}

CommandType Parser::commandType() {
    if (current_command.find("push") != std::string::npos) return C_PUSH;
    if (current_command.find("pop") != std::string::npos) return C_POP;
    return C_ARITHMETIC;
}

std::string Parser::arg1() {
    if (commandType() == C_ARITHMETIC) {
        return current_command;
    }
    size_t first_space = current_command.find(" ");
    size_t second_space = current_command.find(" ", first_space + 1);
    return current_command.substr(first_space + 1, second_space - first_space - 1);
}

int Parser::arg2() {
    size_t last_space = current_command.rfind(" ");
    return std::stoi(current_command.substr(last_space + 1));
}


// --- Main Function ---

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.vm or directory>" << std::endl;
        return 1;
    }

    std::vector<std::string> vm_files;
    std::string output_file;
    std::string input_path = argv[1];

    if (fs::is_directory(input_path)) {
        fs::path dir_path(input_path);
        output_file = (dir_path / dir_path.filename()).string() + ".asm";
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.path().extension() == ".vm") {
                vm_files.push_back(entry.path().string());
            }
        }
    } else {
        vm_files.push_back(input_path);
        output_file = fs::path(input_path).replace_extension(".asm").string();
    }

    CodeWriter code_writer(output_file);

    for (const auto& file : vm_files) {
        Parser parser(file);
        while (parser.hasMoreCommands()) {
            parser.advance();
            if (parser.commandType() == C_ARITHMETIC) {
                code_writer.writeArithmetic(parser.arg1());
            } else if (parser.commandType() == C_PUSH || parser.commandType() == C_POP) {
                code_writer.writePushPop(parser.commandType(), parser.arg1(), parser.arg2());
            }
        }
    }

    code_writer.close();

    return 0;
}