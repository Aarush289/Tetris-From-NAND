#include <bits/stdc++.h>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

enum Cmd {
    T_ARITH, T_PUSH, T_POP, T_LABEL, T_GOTO,
    T_IF, T_FUNCTION, T_RETURN, T_CALL
};

struct VMParser {
    ifstream in;
    string line;

    static Cmd mapType(const string& s) {
        if (s == "push") return T_PUSH;
        if (s == "pop") return T_POP;
        if (s == "label") return T_LABEL;
        if (s == "goto") return T_GOTO;
        if (s == "if-goto") return T_IF;
        if (s == "function") return T_FUNCTION;
        if (s == "return") return T_RETURN;
        if (s == "call") return T_CALL;
        return T_ARITH;
    }

    explicit VMParser(const string& file) { in.open(file); }

    static void trim(string& s) {
        size_t p = s.find("//");
        if (p != string::npos) s.erase(p);
        size_t l = s.find_first_not_of(" \t\r\n");
        if (l == string::npos) { s.clear(); return; }
        size_t r = s.find_last_not_of(" \t\r\n");
        s = s.substr(l, r - l + 1);
    }

    bool next() {
        while (getline(in, line)) {
            trim(line);
            if (!line.empty()) return true;
        }
        return false;
    }

    Cmd type() const {
        size_t sp = line.find(' ');
        string tok = (sp == string::npos ? line : line.substr(0, sp));
        return mapType(tok);
    }

    string a1() const {
        if (type() == T_ARITH) return line;
        size_t s1 = line.find(' ');
        size_t s2 = line.find(' ', s1 + 1);
        if (s1 == string::npos) return "";
        if (s2 == string::npos) return line.substr(s1 + 1);
        return line.substr(s1 + 1, s2 - s1 - 1);
    }

    int a2() const {
        size_t sp = line.rfind(' ');
        return stoi(line.substr(sp + 1));
    }
};

struct AsmWriter {
    ofstream out;
    string moduleTag;
    string funcTag = "null";
    int jcnt = 0;
    int ccnt = 0;

    explicit AsmWriter(const string& fout) { out.open(fout); }

    void setModule(const string& path) { moduleTag = fs::path(path).stem().string();}

    void bootstrap() {
        out << "@256\nD=A\n@SP\nM=D\n";
        writeCall("Sys.init", 0);
    }

    void writeArithmetic(const string& op) {
        if (op=="add"||op=="sub"||op=="and"||op=="or") {
            out << "@SP\nAM=M-1\nD=M\nA=A-1\n";
            if (op=="add") out << "M=M+D\n";
            else if (op=="sub") out << "M=M-D\n";
            else if (op=="and") out << "M=M&D\n";
            else out << "M=M|D\n";
        } else if (op=="neg"||op=="not") {
            out << "@SP\nA=M-1\n";
            if (op=="neg") out << "M=-M\n";
            else out << "M=!M\n";
        } else {
            string t = "T" + to_string(jcnt);
            string e = "E" + to_string(jcnt++);
            out << "@SP\nAM=M-1\nD=M\nA=A-1\nD=M-D\n@" << t << "\n";
            if (op=="eq") out << "D;JEQ\n";
            else if (op=="gt") out << "D;JGT\n";
            else out << "D;JLT\n";
            out << "@SP\nA=M-1\nM=0\n@" << e << "\n0;JMP\n(" << t << ")\n@SP\nA=M-1\nM=-1\n(" << e << ")\n";
        }
    }

    void pushD() {
        out << "@SP\nA=M\nM=D\n@SP\nM=M+1\n";
    }

    void segmentAddr(const string& seg, int idx, bool forPush) {
        if (seg=="constant") { out << "@" << idx << "\nD=A\n"; return; }
        if (seg=="static") { out << "@" << moduleTag << "." << idx << "\n"; if (forPush) out << "D=M\n"; else out << "D=A\n"; return; }
        if (seg=="temp") { out << "@" << (5+idx) << "\n"; if (forPush) out << "D=M\n"; else out << "D=A\n"; return; }
        if (seg=="pointer") { out << "@" << (3+idx) << "\n"; if (forPush) out << "D=M\n"; else out << "D=A\n"; return; }
        static const unordered_map<string,string> base = {{"local","LCL"},{"argument","ARG"},{"this","THIS"},{"that","THAT"}};
        out << "@" << base.at(seg) << "\nD=M\n@" << idx << "\n";
        if (forPush) out << "A=D+A\nD=M\n";
        else out << "D=D+A\n";
    }

    void writePushPop(Cmd t, const string& seg, int idx) {
        if (t == T_PUSH) {
            segmentAddr(seg, idx, true);
            pushD();
        } else {
            segmentAddr(seg, idx, false);
            out << "@R13\nM=D\n@SP\nAM=M-1\nD=M\n@R13\nA=M\nM=D\n";
        }
    }

    void writeLabel(const string& L) { out << "(" << funcTag << "$" << L << ")\n"; }

    void writeGoto(const string& L) { out << "@" << funcTag << "$" << L << "\n0;JMP\n"; }

    void writeIf(const string& L) {
        out << "@SP\nAM=M-1\nD=M\n@" << funcTag << "$" << L << "\nD;JNE\n";
    }

    void writeFunction(const string& name, int k) {
        funcTag = name;
        out << "(" << name << ")\n";
        for (int i=0;i<k;++i) out << "@SP\nA=M\nM=0\n@SP\nM=M+1\n";
    }

    void writeCall(const string& name, int nargs) {
        string ret = funcTag + "$RET." + to_string(ccnt++);
        out << "@" << ret << "\nD=A\n"; pushD();
        const char* regs[] = {"@LCL","@ARG","@THIS","@THAT"};
        for (auto r: regs) { out << r << "\nD=M\n"; pushD(); }
        out << "@SP\nD=M\n@5\nD=D-A\n@" << nargs << "\nD=D-A\n@ARG\nM=D\n";
        out << "@SP\nD=M\n@LCL\nM=D\n";
        out << "@" << name << "\n0;JMP\n(" << ret << ")\n";
    }

    void writeReturn() {
        out << "@LCL\nD=M\n@R13\nM=D\n@5\nA=D-A\nD=M\n@R14\nM=D\n";
        out << "@SP\nAM=M-1\nD=M\n@ARG\nA=M\nM=D\n@ARG\nD=M+1\n@SP\nM=D\n";
        const char* regs[] = {"@THAT","@THIS","@ARG","@LCL"};
        for (auto r: regs) { out << "@R13\nAM=M-1\nD=M\n" << r << "\nM=D\n"; }
        out << "@R14\nA=M\n0;JMP\n";
    }

    void close() { out.close(); }
};

int main(int argc, char* argv[]) {
    string inPath = argv[1];
    vector<string> files;
    string outPath;
    bool isDir = fs::is_directory(inPath);

    if (isDir) {
        fs::path p(inPath);
        outPath = (p / p.filename()).string() + ".asm";
        for (auto& e : fs::directory_iterator(inPath)) {
            if (e.path().extension() == ".vm") files.push_back(e.path().string());
        }
        sort(files.begin(), files.end());
    } else {
        files.push_back(inPath);
        outPath = fs::path(inPath).replace_extension(".asm").string();
    }

    AsmWriter W(outPath);
    if (isDir) W.bootstrap();

    for (const auto& f : files) {
        W.setModule(f);
        VMParser P(f);
        while (P.next()) {
            Cmd t = P.type();
            switch (t) {
                case T_ARITH:    W.writeArithmetic(P.a1()); break;
                case T_PUSH:     W.writePushPop(T_PUSH, P.a1(), P.a2()); break;
                case T_POP:      W.writePushPop(T_POP, P.a1(), P.a2()); break;
                case T_LABEL:    W.writeLabel(P.a1()); break;
                case T_GOTO:     W.writeGoto(P.a1()); break;
                case T_IF:       W.writeIf(P.a1()); break;
                case T_FUNCTION: W.writeFunction(P.a1(), P.a2()); break;
                case T_CALL:     W.writeCall(P.a1(), P.a2()); break;
                case T_RETURN:   W.writeReturn(); break;
            }
        }
    }

    W.close();
    return 0;
}
