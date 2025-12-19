#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

using namespace std;
namespace fs = std::filesystem;

enum TokenType {
    T_KEYWORD, T_SYMBOL, T_IDENTIFIER, T_INT_CONST, T_STRING_CONST
};

enum KeywordType {
    K_CLASS, K_METHOD, K_FUNCTION, K_CONSTRUCTOR, K_INT, K_BOOLEAN, K_CHAR, K_VOID,
    K_VAR, K_STATIC, K_FIELD, K_LET, K_DO, K_IF, K_ELSE, K_WHILE, K_RETURN, K_TRUE,
    K_FALSE, K_NULL, K_THIS
};

class JackTokenizer {
public:
    explicit JackTokenizer(const string& filename);

    bool hasMoreTokens();
    void advance();

    TokenType tokenType() const { return tok_; }
    KeywordType keyword() const { return kw_; }
    char symbol() const { return curr_.empty() ? '\0' : curr_[0]; }
    string identifier() const { return curr_; }
    int intVal() const { return atoi(curr_.c_str()); }
    string stringVal() const { return curr_; }
    string getCurrentToken() const { return curr_; }

private:
    ifstream in_;
    string curr_;
    TokenType tok_;
    KeywordType kw_;
    unordered_map<string, KeywordType> kwMap_;
    unordered_set<char> sym_;

    void skipTrivia();
    inline bool isSym(char c) const { return sym_.find(c) != sym_.end(); }
};

JackTokenizer::JackTokenizer(const string& filename) : tok_(T_SYMBOL), kw_(K_CLASS) {
    in_.open(filename);
    kwMap_ = {
        {"class", K_CLASS}, {"constructor", K_CONSTRUCTOR}, {"function", K_FUNCTION},
        {"method", K_METHOD}, {"field", K_FIELD}, {"static", K_STATIC}, {"var", K_VAR},
        {"int", K_INT}, {"char", K_CHAR}, {"boolean", K_BOOLEAN}, {"void", K_VOID},
        {"true", K_TRUE}, {"false", K_FALSE}, {"null", K_NULL}, {"this", K_THIS},
        {"let", K_LET}, {"do", K_DO}, {"if", K_IF}, {"else", K_ELSE},
        {"while", K_WHILE}, {"return", K_RETURN}
    };
    sym_ = {'{','}','(',')','[',']','.',',',';','+','-','*','/','&','|','<','>','=','~'};
}

bool JackTokenizer::hasMoreTokens() {
    skipTrivia();
    return in_ && in_.peek() != EOF;
}

void JackTokenizer::advance() {
    if (!hasMoreTokens()) return;

    curr_.clear();
    int p = in_.peek();
    if (p == EOF) return;
    char c = static_cast<char>(p);

    if (isSym(c)) {
        tok_ = T_SYMBOL;
        curr_.push_back(static_cast<char>(in_.get()));
        return;
    }

    if (c == '"') {
        tok_ = T_STRING_CONST;
        in_.get();
        while (in_ && in_.peek() != '"' && in_.peek() != '\n' && in_.peek() != EOF) {
            curr_.push_back(static_cast<char>(in_.get()));
        }
        if (in_.peek() == '"') in_.get();
        return;
    }

    if (isdigit(static_cast<unsigned char>(c))) {
        tok_ = T_INT_CONST;
        while (in_ && isdigit(static_cast<unsigned char>(in_.peek()))) {
            curr_.push_back(static_cast<char>(in_.get()));
        }
        return;
    }

    if(isalpha(static_cast<unsigned char>(c)) || c == '_'){
        while (in_) {
            int q = in_.peek();
            if (q == EOF) break;
            char d = static_cast<char>(q);
            if (isalnum(static_cast<unsigned char>(d)) || d == '_') {
                curr_.push_back(static_cast<char>(in_.get()));
            } else {
                break;
            }
        }
        auto it = kwMap_.find(curr_);
        if (it != kwMap_.end()) {
            tok_ = T_KEYWORD;
            kw_ = it->second;
        } else {
            tok_ = T_IDENTIFIER;
        }
        return;
    }

    tok_ = T_SYMBOL;
    curr_.push_back(static_cast<char>(in_.get()));
}

void JackTokenizer::skipTrivia() {
    for (;;) {
        int p = in_.peek();
        if (p == EOF) return;
        char c = static_cast<char>(p);

        if (isspace(static_cast<unsigned char>(c))) {
            in_.get();
            continue;
        }

        if (c == '/') {
            in_.get();
            int q = in_.peek();
            if (q == EOF) { in_.unget(); return; }
            char d = static_cast<char>(q);

            if (d == '/') {
                string dummy;
                getline(in_, dummy);
                continue;
            }
            if (d == '*') {
                in_.get();
                char prev = '\0', now = '\0';
                bool closed = false;
                while (in_.get(now)) {
                    if (prev == '*' && now == '/') { closed = true; break; }
                    prev = now;
                }
                (void)closed;
                continue;
            }

            in_.unget();
            return;
        }

        return;
    }
}

static inline void writeSymbol(ostream& os, char s) {
    if (s == '<')      os << "<symbol> &lt; </symbol>\n";
    else if (s == '>') os << "<symbol> &gt; </symbol>\n";
    else if (s == '&') os << "<symbol> &amp; </symbol>\n";
    else               os << "<symbol> " << s << " </symbol>\n";
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " [fileName.jack | directoryName]\n";
        return 1;
    }

    fs::path input(argv[1]);
    vector<fs::path> toProcess;

    if (fs::is_regular_file(input)) {
        if (input.extension() == ".jack") toProcess.push_back(input);
    } else if (fs::is_directory(input)) {
        for (const auto& entry : fs::directory_iterator(input)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jack") {
                toProcess.push_back(entry.path());
            }
        }
    } else {
        cerr << "Invalid path: " << input << "\n";
        return 1;
    }

    for (const auto& jackPath : toProcess) {
        fs::path parent = jackPath.parent_path();
        string fname = jackPath.filename().string();

        fs::path xmlOut;
        if (fname == "Main.jack") {
            xmlOut = parent / "myMainT.xml";
        } else {
            xmlOut = parent / ("my" + jackPath.stem().string() + "T.xml");
        }

        JackTokenizer tz(jackPath.string());
        ofstream out(xmlOut);
        out << "<tokens>\n";

        while (tz.hasMoreTokens()) {
            tz.advance();
            switch (tz.tokenType()) {
                case T_KEYWORD:
                    out << "<keyword> " << tz.getCurrentToken() << " </keyword>\n";
                    break;
                case T_SYMBOL:
                    writeSymbol(out, tz.symbol());
                    break;
                case T_IDENTIFIER:
                    out << "<identifier> " << tz.identifier() << " </identifier>\n";
                    break;
                case T_INT_CONST:
                    out << "<integerConstant> " << tz.intVal() << " </integerConstant>\n";
                    break;
                case T_STRING_CONST:
                    out << "<stringConstant> " << tz.stringVal() << " </stringConstant>\n";
                    break;
            }
        }

        out << "</tokens>\n";
    }

    return 0;
}