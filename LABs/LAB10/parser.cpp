#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

enum TokenType { T_KEYWORD, T_SYMBOL, T_IDENTIFIER, T_INT_CONST, T_STRING_CONST };

enum KeywordType {
    K_CLASS, K_METHOD, K_FUNCTION, K_CONSTRUCTOR, K_INT, K_BOOLEAN, K_CHAR, K_VOID,
    K_VAR, K_STATIC, K_FIELD, K_LET, K_DO, K_IF, K_ELSE, K_WHILE, K_RETURN, K_TRUE,
    K_FALSE, K_NULL, K_THIS
};

class Tokenizer {
public:
    ifstream curr;
    string currToken;
    TokenType token{};
    KeywordType keywordType{};

    Tokenizer(const string& filename);

    bool hasMoreTokens();
    void advance();

    string peekNextToken();

    TokenType tokenType() { return token; }
    KeywordType keyword() { return keywordType; }
    char symbol() { return currToken.empty() ? '\0' : currToken[0]; }
    string identifier() { return currToken; }
    int intVal() { return atoi(currToken.c_str()); }
    string stringVal() { return currToken; }
    string getCurrentToken() { return currToken; }

    string getNextXmlToken();

    static string unescapeXml(const string& s) {
        if (s == "&lt;") return "<";
        if (s == "&gt;") return ">";
        if (s == "&amp;") return "&";
        if (s == "&quot;") return "\"";
        return s;
    }

private:
    unordered_map<string, KeywordType> keywordMap;
    unordered_map<string, TokenType>   tagMap;

    static inline void skipWs(ifstream& in) {
        while (in && isspace(in.peek())) in.get();
    }

    static string readXmlChunk(ifstream& in) {
        skipWs(in);
        string out;
        if (!in) return out;
        int pk = in.peek();
        if (pk == EOF) return out;
        if (static_cast<char>(pk) == '<') {
            char c;
            while (in.get(c)) {
                out.push_back(c);
                if (c == '>') break;
            }
            return out;
        } else {
            char c;
            while (in && in.peek() != '<' && in.get(c)) out.push_back(c);
            size_t end = out.find_last_not_of(" \t\r\n");
            if (end != string::npos) out.erase(end + 1);
            while (!out.empty() && isspace(static_cast<unsigned char>(out.front())))
                out.erase(out.begin());
            return out;
        }
    }

    static string stripTagBrackets(const string& tag) {
        if (tag.size() >= 2 && tag.front() == '<' && tag.back() == '>')
            return tag.substr(1, tag.size() - 2);
        return tag;
    }

    string peekNextTag() {
        streampos pos = curr.tellg();
        string t = readXmlChunk(curr);
        curr.clear();
        curr.seekg(pos);
        return t;
    }
};

Tokenizer::Tokenizer(const string& filename) {
    curr.open(filename);
    keywordMap = {
        {"class", K_CLASS}, {"constructor", K_CONSTRUCTOR}, {"function", K_FUNCTION},
        {"method", K_METHOD}, {"field", K_FIELD}, {"static", K_STATIC}, {"var", K_VAR},
        {"int", K_INT}, {"char", K_CHAR}, {"boolean", K_BOOLEAN}, {"void", K_VOID},
        {"true", K_TRUE}, {"false", K_FALSE}, {"null", K_NULL}, {"this", K_THIS},
        {"let", K_LET}, {"do", K_DO}, {"if", K_IF}, {"else", K_ELSE},
        {"while", K_WHILE}, {"return", K_RETURN}
    };
    tagMap = {
        {"keyword", T_KEYWORD},
        {"symbol", T_SYMBOL},
        {"identifier", T_IDENTIFIER},
        {"integerConstant", T_INT_CONST},
        {"stringConstant", T_STRING_CONST}
    };
    (void)getNextXmlToken();
}

bool Tokenizer::hasMoreTokens() {
    if (!curr) return false;
    string nxt = peekNextTag();
    return nxt != "</tokens>" && !nxt.empty();
}

void Tokenizer::advance() {
    if (!hasMoreTokens()) return;
    string openTag = readXmlChunk(curr);
    string tagName = stripTagBrackets(openTag);
    currToken = readXmlChunk(curr);
    auto itType = tagMap.find(tagName);
    if (itType != tagMap.end()) token = itType->second;
    if (token == T_SYMBOL) currToken = unescapeXml(currToken);
    if (token == T_KEYWORD) {
        auto itKw = keywordMap.find(currToken);
        if (itKw != keywordMap.end()) keywordType = itKw->second;
    }
    (void)readXmlChunk(curr);
}

string Tokenizer::peekNextToken() {
    streampos pos = curr.tellg();
    string value;
    if (hasMoreTokens()) {
        string openTag = readXmlChunk(curr);
        value = readXmlChunk(curr);
        if (openTag.find("symbol") != string::npos) value = unescapeXml(value);
        (void)readXmlChunk(curr);
    }
    curr.clear();
    curr.seekg(pos);
    return value;
}

string Tokenizer::getNextXmlToken() {
    return readXmlChunk(curr);
}

class Parser {
public:
    Tokenizer* tokenizer;
    ofstream* outFile;
    int indentLevel{0};

    Parser(Tokenizer* t, ofstream* o) : tokenizer(t), outFile(o) {}

    void compileClass() {
        openNonTerm("class");
        tokenizer->advance();
        expect("class");
        parseIdentifier();
        expect("{");
        while (isOneOf(tokenizer->getCurrentToken(), {"static","field"})) {
            compileClassVarDec();
        }
        while (isOneOf(tokenizer->getCurrentToken(), {"constructor","function","method"})) {
            compileSubroutine();
        }
        expect("}");
        if (tokenizer->hasMoreTokens()) tokenizer->advance();
        closeNonTerm("class");
    }

private:
    static bool isOneOf(const string& s, initializer_list<const char*> alts) {
        for (auto a : alts) if (s == a) return true;
        return false;
    }

    static bool isBinaryOp(const string& s) {
        return s.size() == 1 && string("+-*/&|<>= ").find(s[0]) != string::npos;
    }

    static bool isUnaryOp(const string& s) {
        return s == "-" || s == "~";
    }

    void indent() { (*outFile) << string(indentLevel * 2, ' '); }

    void openNonTerm(const string& tag) {
        indent(); (*outFile) << "<" << tag << ">\n";
        ++indentLevel;
    }

    void closeNonTerm(const string& tag) {
        --indentLevel;
        indent(); (*outFile) << "</" << tag << ">\n";
    }

    void emitCurrentToken() {
        indent();
        switch (tokenizer->tokenType()) {
            case T_KEYWORD:
                (*outFile) << "<keyword> " << tokenizer->getCurrentToken() << " </keyword>\n";
                break;
            case T_SYMBOL: {
                char s = tokenizer->symbol();
                if (s == '<')      (*outFile) << "<symbol> &lt; </symbol>\n";
                else if (s == '>') (*outFile) << "<symbol> &gt; </symbol>\n";
                else if (s == '&') (*outFile) << "<symbol> &amp; </symbol>\n";
                else               (*outFile) << "<symbol> " << s << " </symbol>\n";
                break;
            }
            case T_IDENTIFIER:
                (*outFile) << "<identifier> " << tokenizer->identifier() << " </identifier>\n";
                break;
            case T_INT_CONST:
                (*outFile) << "<integerConstant> " << tokenizer->intVal() << " </integerConstant>\n";
                break;
            case T_STRING_CONST:
                (*outFile) << "<stringConstant> " << tokenizer->stringVal() << " </stringConstant>\n";
                break;
        }
        if (tokenizer->hasMoreTokens()) tokenizer->advance();
    }

    void expect(const string& literal) {
        if (tokenizer->getCurrentToken() == literal) emitCurrentToken();
    }

    void parseIdentifier() {
        if (tokenizer->tokenType() == T_IDENTIFIER) emitCurrentToken();
    }

    void parseType() {
        if (tokenizer->tokenType() == T_KEYWORD &&
            isOneOf(tokenizer->getCurrentToken(), {"int","char","boolean"})) {
            emitCurrentToken();
        } else if (tokenizer->tokenType() == T_IDENTIFIER) {
            emitCurrentToken();
        }
    }

    void compileClassVarDec() {
        openNonTerm("classVarDec");
        emitCurrentToken();
        parseType();
        parseIdentifier();
        while (tokenizer->getCurrentToken() == ",") {
            expect(",");
            parseIdentifier();
        }
        expect(";");
        closeNonTerm("classVarDec");
    }

    void compileSubroutine() {
        openNonTerm("subroutineDec");
        emitCurrentToken();
        if (tokenizer->getCurrentToken() == "void") expect("void");
        else parseType();
        parseIdentifier();
        expect("(");
        compileParameterList();
        expect(")");
        compileSubroutineBody();
        closeNonTerm("subroutineDec");
    }

    void compileParameterList() {
        openNonTerm("parameterList");
        if (tokenizer->getCurrentToken() != ")") {
            parseType();
            parseIdentifier();
            while (tokenizer->getCurrentToken() == ",") {
                expect(",");
                parseType();
                parseIdentifier();
            }
        }
        closeNonTerm("parameterList");
    }

    void compileSubroutineBody() {
        openNonTerm("subroutineBody");
        expect("{");
        while (tokenizer->getCurrentToken() == "var") {
            compileVarDec();
        }
        compileStatements();
        expect("}");
        closeNonTerm("subroutineBody");
    }

    void compileVarDec() {
        openNonTerm("varDec");
        expect("var");
        parseType();
        parseIdentifier();
        while (tokenizer->getCurrentToken() == ",") {
            expect(",");
            parseIdentifier();
        }
        expect(";");
        closeNonTerm("varDec");
    }

    void compileStatements() {
        openNonTerm("statements");
        for (;;) {
            const string& t = tokenizer->getCurrentToken();
            if (t == "let")      compileLet();
            else if (t == "if")  compileIf();
            else if (t == "while") compileWhile();
            else if (t == "do")  compileDo();
            else if (t == "return") compileReturn();
            else break;
        }
        closeNonTerm("statements");
    }

    void compileLet() {
        openNonTerm("letStatement");
        expect("let");
        parseIdentifier();
        if (tokenizer->getCurrentToken() == "[") {
            expect("[");
            compileExpression();
            expect("]");
        }
        expect("=");
        compileExpression();
        expect(";");
        closeNonTerm("letStatement");
    }

    void compileIf() {
        openNonTerm("ifStatement");
        expect("if");
        expect("(");
        compileExpression();
        expect(")");
        expect("{");
        compileStatements();
        expect("}");
        if (tokenizer->getCurrentToken() == "else") {
            expect("else");
            expect("{");
            compileStatements();
            expect("}");
        }
        closeNonTerm("ifStatement");
    }

    void compileWhile() {
        openNonTerm("whileStatement");
        expect("while");
        expect("(");
        compileExpression();
        expect(")");
        expect("{");
        compileStatements();
        expect("}");
        closeNonTerm("whileStatement");
    }

    void compileDo() {
        openNonTerm("doStatement");
        expect("do");
        parseIdentifier();
        if (tokenizer->getCurrentToken() == "(") {
            expect("(");
            compileExpressionList();
            expect(")");
        } else if (tokenizer->getCurrentToken() == ".") {
            expect(".");
            parseIdentifier();
            expect("(");
            compileExpressionList();
            expect(")");
        }
        expect(";");
        closeNonTerm("doStatement");
    }

    void compileReturn() {
        openNonTerm("returnStatement");
        expect("return");
        if (tokenizer->getCurrentToken() != ";") {
            compileExpression();
        }
        expect(";");
        closeNonTerm("returnStatement");
    }

    void compileExpression() {
        openNonTerm("expression");
        compileTerm();
        while (isBinaryOp(tokenizer->getCurrentToken())) {
            emitCurrentToken();
            compileTerm();
        }
        closeNonTerm("expression");
    }

    void compileTerm() {
        openNonTerm("term");
        string t = tokenizer->getCurrentToken();
        if (tokenizer->tokenType() == T_INT_CONST ||
            tokenizer->tokenType() == T_STRING_CONST ||
            (tokenizer->tokenType() == T_KEYWORD &&
             (t == "true" || t == "false" || t == "null" || t == "this"))) {
            emitCurrentToken();
        }
        else if (t == "(") {
            expect("(");
            compileExpression();
            expect(")");
        }
        else if (isUnaryOp(t)) {
            emitCurrentToken();
            compileTerm();
        }
        else if (tokenizer->tokenType() == T_IDENTIFIER) {
            string look = tokenizer->peekNextToken();
            if (look == "[") {
                parseIdentifier();
                expect("[");
                compileExpression();
                expect("]");
            } else if (look == "(" || look == ".") {
                parseIdentifier();
                if (tokenizer->getCurrentToken() == "(") {
                    expect("(");
                    compileExpressionList();
                    expect(")");
                } else {
                    expect(".");
                    parseIdentifier();
                    expect("(");
                    compileExpressionList();
                    expect(")");
                }
            } else {
                parseIdentifier();
            }
        }
        closeNonTerm("term");
    }

    void compileExpressionList() {
        openNonTerm("expressionList");
        if (tokenizer->getCurrentToken() != ")") {
            compileExpression();
            while (tokenizer->getCurrentToken() == ",") {
                expect(",");
                compileExpression();
            }
        }
        closeNonTerm("expressionList");
    }
};

static bool endsWith(const string& s, const string& suf) {
    return s.size() >= suf.size() &&
           equal(suf.rbegin(), suf.rend(), s.rbegin());
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " [fileT.xml | directoryName]\n";
        return 1;
    }
    fs::path inputPath(argv[1]);
    vector<string> filesToProcess;
    if (fs::is_directory(inputPath)) {
        for (const auto& entry : fs::directory_iterator(inputPath)) {
            if (!entry.is_regular_file()) continue;
            string name = entry.path().filename().string();
            if (endsWith(name, "T.xml")) filesToProcess.push_back(entry.path().string());
        }
    } else {
        string name = inputPath.filename().string();
        if (endsWith(name, "T.xml")) filesToProcess.push_back(inputPath.string());
    }
    for (const auto& xmlTokenFile : filesToProcess) {
        fs::path p(xmlTokenFile);
        fs::path parent = p.parent_path();
        string stem = p.stem().string();
        string finalStem = (stem.empty() ? stem : stem.substr(0, stem.size() - 1));
        string outPath = (parent / (finalStem + ".xml")).string();
        Tokenizer tokenizer(xmlTokenFile);
        ofstream out(outPath);
        Parser parser(&tokenizer, &out);
        parser.compileClass();
        out.close();
    }
    return 0;
}
