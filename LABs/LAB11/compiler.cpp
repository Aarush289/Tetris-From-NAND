#include <bits/stdc++.h>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

enum class TokType { KW, SYM, ID, INTC, STRC, END };
enum class Kw {
  CLASS, METHOD, FUNCTION, CONSTRUCTOR, INT, BOOLEAN, CHAR, VOID, VAR,
  STATIC, FIELD, LET, DO, IF, ELSE, WHILE, RETURN, TRUE_, FALSE_, NULL_, THIS_
};

struct Token {
  TokType t{TokType::END};
  Kw      kw{};
  string  s;
  int     ival{0};
  char    ch{0};
};

class Tokenizer {
public:
  explicit Tokenizer(const string& path){ load(path); }
  bool hasMore() const { return pos_ < toks_.size(); }
  const Token& peek()  const { return toks_[pos_]; }
  Token advance()      { return toks_[pos_++]; }
  bool isSym(char c) const { return hasMore() && toks_[pos_].t==TokType::SYM && toks_[pos_].ch==c; }
  void expectSym(char c, const char* ctx) { if (!isSym(c)) fail(string("expected '")+c+"' "+ctx); ++pos_; }
  void expectKw(Kw k, const char* ctx) { if (!hasMore() || toks_[pos_].t!=TokType::KW || toks_[pos_].kw!=k) fail(string("expected keyword in ")+ctx); ++pos_; }
  string expectId(const char* ctx) { if (!hasMore() || toks_[pos_].t!=TokType::ID) fail(string("expected identifier: ")+ctx); return toks_[pos_++].s; }

private:
  vector<Token> toks_;
  size_t pos_{0};

  [[noreturn]] static void fail(const string& m){ throw runtime_error(m); }
  static bool isIdStart(char c){ return isalpha((unsigned char)c) || c=='_'; }
  static bool isIdChar(char c){ return isalnum((unsigned char)c) || c=='_'; }
  static optional<Kw> kwOf(const string& w){
    static const unordered_map<string,Kw> M = {
      {"class",Kw::CLASS},{"method",Kw::METHOD},{"function",Kw::FUNCTION},
      {"constructor",Kw::CONSTRUCTOR},{"int",Kw::INT},{"boolean",Kw::BOOLEAN},
      {"char",Kw::CHAR},{"void",Kw::VOID},{"var",Kw::VAR},{"static",Kw::STATIC},
      {"field",Kw::FIELD},{"let",Kw::LET},{"do",Kw::DO},{"if",Kw::IF},
      {"else",Kw::ELSE},{"while",Kw::WHILE},{"return",Kw::RETURN},
      {"true",Kw::TRUE_},{"false",Kw::FALSE_},{"null",Kw::NULL_},{"this",Kw::THIS_}
    };
    auto it=M.find(w); if(it==M.end()) return nullopt; return it->second;
  }
  void emitSym(char c){ Token t; t.t=TokType::SYM; t.ch=c; toks_.push_back(t); }
  void emitKw(Kw k){ Token t; t.t=TokType::KW; t.kw=k; toks_.push_back(t); }
  void emitId(const string& s){ Token t; t.t=TokType::ID; t.s=s; toks_.push_back(t); }
  void emitInt(int v){ Token t; t.t=TokType::INTC; t.ival=v; toks_.push_back(t); }
  void emitStr(const string& s){ Token t; t.t=TokType::STRC; t.s=s; toks_.push_back(t); }
  void load(const string& path){
    ifstream in(path);
    if(!in) fail("cannot open input: "+path);
    string src((istreambuf_iterator<char>(in)), {});
    size_t i=0, n=src.size();
    auto skipSpace=[&]{ while(i<n && isspace((unsigned char)src[i])) ++i; };
    auto skipComment=[&]{
      if (i+1<n && src[i]=='/' && src[i+1]=='/') { i+=2; while(i<n && src[i]!='\n') ++i; return true; }
      if (i+1<n && src[i]=='/' && src[i+1]=='*') { i+=2; while(i+1<n && !(src[i]=='*' && src[i+1]=='/')) ++i; if(i+1<n) i+=2; return true; }
      return false;
    };
    const string sym="{}()[].,;+-*/&|<>=~";
    while(i<n){
      skipSpace();
      if (skipComment()) continue;
      if (i>=n) break;
      char c=src[i];
      if (sym.find(c)!=string::npos){ emitSym(c); ++i; continue; }
      if (c=='"'){ ++i; string s; while(i<n && src[i]!='"'){ s.push_back(src[i++]); } if(i<n) ++i; emitStr(s); continue; }
      if (isdigit((unsigned char)c)){
        int v=0; while(i<n && isdigit((unsigned char)src[i])) v=v*10+(src[i++]-'0');
        emitInt(v); continue;
      }
      if (isIdStart(c)){
        string w; while(i<n && isIdChar(src[i])) w.push_back(src[i++]);
        if (auto k=kwOf(w)) emitKw(*k); else emitId(w);
        continue;
      }
      ++i;
    }
  }
};

enum class Kind { STATIC, FIELD, ARG, VAR, NONE };
struct Sym { string type; Kind k{Kind::NONE}; int idx{-1}; };

class SymbolTable {
public:
  void startClass(){ c_.clear(); cStat_=cField_=0; }
  void startSub(){ s_.clear(); sArg_=sVar_=0; }
  void define(const string& name, const string& type, Kind k){
    int idx = nextIndex(k);
    Sym v{type,k,idx};
    (isClassKind(k)? c_: s_)[name]=v;
    bump(k);
  }
  int varCount(Kind k) const { return count(k); }
  Kind kindOf(const string& name) const {
    if (auto it=s_.find(name); it!=s_.end()) return it->second.k;
    if (auto it=c_.find(name); it!=c_.end()) return it->second.k;
    return Kind::NONE;
  }
  string typeOf(const string& name) const {
    if (auto it=s_.find(name); it!=s_.end()) return it->second.type;
    if (auto it=c_.find(name); it!=c_.end()) return it->second.type;
    return "";
  }
  int indexOf(const string& name) const {
    if (auto it=s_.find(name); it!=s_.end()) return it->second.idx;
    if (auto it=c_.find(name); it!=c_.end()) return it->second.idx;
    return -1;
  }

private:
  unordered_map<string,Sym> c_, s_;
  int cStat_{0}, cField_{0}, sArg_{0}, sVar_{0};
  static bool isClassKind(Kind k){ return k==Kind::STATIC || k==Kind::FIELD; }
  int nextIndex(Kind k) const {
    switch(k){
      case Kind::STATIC: return cStat_;
      case Kind::FIELD:  return cField_;
      case Kind::ARG:    return sArg_;
      case Kind::VAR:    return sVar_;
      default:           return 0;
    }
  }
  void bump(Kind k){
    switch(k){
      case Kind::STATIC: ++cStat_; break;
      case Kind::FIELD:  ++cField_; break;
      case Kind::ARG:    ++sArg_;   break;
      case Kind::VAR:    ++sVar_;   break;
      default: break;
    }
  }
  int count(Kind k) const {
    switch(k){
      case Kind::STATIC: return cStat_;
      case Kind::FIELD:  return cField_;
      case Kind::ARG:    return sArg_;
      case Kind::VAR:    return sVar_;
      default:           return 0;
    }
  }
};

enum class VMSeg { CONST, ARG, LOCAL, STATIC, THIS_, THAT, POINTER, TEMP };
enum class VMOp  { ADD, SUB, NEG, EQ, GT, LT, AND, OR, NOT };

class VMWriter {
public:
  explicit VMWriter(const string& out){ out_.open(out); if(!out_) throw runtime_error("open vm failed"); }
  void push(VMSeg s, int i){ ln("push "+seg(s)+" "+to_string(i)); }
  void pop (VMSeg s, int i){ ln("pop "+seg(s)+" "+to_string(i)); }
  void op  (VMOp  a)      { ln(opname(a)); }
  void label(const string& L){ ln("label "+L); }
  void go   (const string& L){ ln("goto "+L); }
  void ifgo (const string& L){ ln("if-goto "+L); }
  void call (const string& f, int n){ ln("call "+f+" "+to_string(n)); }
  void func (const string& f, int nLoc){ ln("function "+f+" "+to_string(nLoc)); }
  void ret  (){ ln("return"); }
  void close(){ out_.close(); }

private:
  ofstream out_;
  void ln(const string& s){ out_<<s<<'\n'; }
  static string seg(VMSeg s){
    switch(s){
      case VMSeg::CONST: return "constant"; case VMSeg::ARG: return "argument";
      case VMSeg::LOCAL: return "local";    case VMSeg::STATIC: return "static";
      case VMSeg::THIS_: return "this";     case VMSeg::THAT: return "that";
      case VMSeg::POINTER:return "pointer"; case VMSeg::TEMP: return "temp";
    } return "constant";
  }
  static string opname(VMOp a){
    switch(a){
      case VMOp::ADD: return "add";   case VMOp::SUB: return "sub";
      case VMOp::NEG: return "neg";   case VMOp::EQ:  return "eq";
      case VMOp::GT:  return "gt";    case VMOp::LT:  return "lt";
      case VMOp::AND: return "and";   case VMOp::OR:  return "or";
      case VMOp::NOT: return "not";
    } return "add";
  }
};

static inline VMSeg kindToSeg(Kind k){
  switch(k){
    case Kind::STATIC: return VMSeg::STATIC;
    case Kind::FIELD:  return VMSeg::THIS_;
    case Kind::ARG:    return VMSeg::ARG;
    case Kind::VAR:    return VMSeg::LOCAL;
    default:           return VMSeg::TEMP;
  }
}

struct LabelGen { int n=0; string get(const string& base){ return base+"_"+to_string(n++); } };

class Engine {
public:
  Engine(Tokenizer& tz, VMWriter& vm, SymbolTable& st)
  : tz_(tz), vm_(vm), st_(st) {}
  void compileClass(){
    st_.startClass();
    tz_.expectKw(Kw::CLASS, "class");
    className_ = tz_.expectId("class name");
    tz_.expectSym('{', "after class name");
    while (isKw({Kw::STATIC, Kw::FIELD})) compileClassVarDec();
    while (isKw({Kw::CONSTRUCTOR, Kw::FUNCTION, Kw::METHOD})) compileSubroutine();
    tz_.expectSym('}', "end of class");
  }

private:
  Tokenizer& tz_;
  VMWriter&  vm_;
  SymbolTable& st_;
  LabelGen   L_;
  string     className_;

  bool isKw(std::initializer_list<Kw> set){
    if(!tz_.hasMore()) return false;
    const Token& t = tz_.peek();
    if (t.t!=TokType::KW) return false;
    for (auto k: set) if (t.kw==k) return true;
    return false;
  }
  string readType(){
    const Token& t = tz_.peek();
    if (t.t==TokType::KW){
      switch(t.kw){
        case Kw::INT:     tz_.advance(); return "int";
        case Kw::CHAR:    tz_.advance(); return "char";
        case Kw::BOOLEAN: tz_.advance(); return "boolean";
        case Kw::VOID:    tz_.advance(); return "void";
        default: break;
      }
    }
    return tz_.expectId("type");
  }
  void compileClassVarDec(){
    Kind k = (tz_.peek().kw==Kw::STATIC)? Kind::STATIC : Kind::FIELD;
    tz_.advance();
    string type = readType();
    string name = tz_.expectId("class var");
    st_.define(name, type, k);
    while (tz_.isSym(',')){ tz_.advance(); st_.define(tz_.expectId("class var"), type, k); }
    tz_.expectSym(';', "classVarDec ';'");
  }
  void compileSubroutine(){
    Kw stype = tz_.advance().kw;
    string rtype = readType(); (void)rtype;
    string name  = tz_.expectId("subroutine name");
    st_.startSub();
    if (stype==Kw::METHOD) st_.define("this", className_, Kind::ARG);
    tz_.expectSym('(', "param '('");
    compileParameterList();
    tz_.expectSym(')', "param ')'");
    tz_.expectSym('{', "subroutine '{'");
    while (isKw({Kw::VAR})) compileVarDec();
    int nLocals = st_.varCount(Kind::VAR);
    vm_.func(className_+"."+name, nLocals);
    if (stype==Kw::CONSTRUCTOR){
      int nFields = st_.varCount(Kind::FIELD);
      vm_.push(VMSeg::CONST, nFields);
      vm_.call("Memory.alloc", 1);
      vm_.pop(VMSeg::POINTER, 0);
    } else if (stype==Kw::METHOD){
      vm_.push(VMSeg::ARG, 0);
      vm_.pop(VMSeg::POINTER, 0);
    }
    compileStatements();
    tz_.expectSym('}', "subroutine '}'");
  }
  void compileParameterList(){
    if (tz_.isSym(')')) return;
    while(true){
      string type = readType();
      string name = tz_.expectId("param");
      st_.define(name, type, Kind::ARG);
      if (!tz_.isSym(',')) break;
      tz_.advance();
    }
  }
  void compileVarDec(){
    tz_.advance();
    string type = readType();
    st_.define(tz_.expectId("var name"), type, Kind::VAR);
    while (tz_.isSym(',')){
      tz_.advance();
      st_.define(tz_.expectId("var name"), type, Kind::VAR);
    }
    tz_.expectSym(';', "var ';'");
  }
  void compileStatements(){
    while (tz_.hasMore() && tz_.peek().t==TokType::KW){
      switch(tz_.peek().kw){
        case Kw::LET:    compileLet();    break;
        case Kw::IF:     compileIf();     break;
        case Kw::WHILE:  compileWhile();  break;
        case Kw::DO:     compileDo();     break;
        case Kw::RETURN: compileReturn(); break;
        default: return;
      }
    }
  }
  void compileLet(){
    tz_.advance();
    string name = tz_.expectId("let var");
    Kind  k = st_.kindOf(name); VMSeg sg = kindToSeg(k); int ix = st_.indexOf(name);
    bool isArray=false;
    if (tz_.isSym('[')){
      tz_.advance();
      compileExpression();
      tz_.expectSym(']', "']'");
      vm_.push(sg, ix);
      vm_.op(VMOp::ADD);
      isArray=true;
    }
    tz_.expectSym('=', "'='");
    compileExpression();
    tz_.expectSym(';', "';'");
    if (isArray){
      vm_.pop(VMSeg::TEMP, 0);
      vm_.pop(VMSeg::POINTER, 1);
      vm_.push(VMSeg::TEMP, 0);
      vm_.pop(VMSeg::THAT, 0);
    } else {
      vm_.pop(sg, ix);
    }
  }
  void compileIf(){
    tz_.advance();
    string Lfalse = L_.get("IF_FALSE");
    string Lend  = L_.get("IF_END");
    tz_.expectSym('(', "(");
    compileExpression();
    tz_.expectSym(')', ")");
    vm_.op(VMOp::NOT);
    vm_.ifgo(Lfalse);
    tz_.expectSym('{', "{");
    compileStatements();
    tz_.expectSym('}', "}");
    if (tz_.hasMore() && tz_.peek().t==TokType::KW && tz_.peek().kw==Kw::ELSE){
      vm_.go(Lend);
      vm_.label(Lfalse);
      tz_.advance();
      tz_.expectSym('{', "{");
      compileStatements();
      tz_.expectSym('}', "}");
      vm_.label(Lend);
    } else {
      vm_.label(Lfalse);
    }
  }
  void compileWhile(){
    tz_.advance();
    string Ltop = L_.get("WHILE_EXP");
    string Lend = L_.get("WHILE_END");
    vm_.label(Ltop);
    tz_.expectSym('(', "(");
    compileExpression();
    tz_.expectSym(')', ")");
    vm_.op(VMOp::NOT);
    vm_.ifgo(Lend);
    tz_.expectSym('{', "{");
    compileStatements();
    tz_.expectSym('}', "}");
    vm_.go(Ltop);
    vm_.label(Lend);
  }
  void compileDo(){
    tz_.advance();
    compileSubroutineCall();
    tz_.expectSym(';', "';'");
    vm_.pop(VMSeg::TEMP, 0);
  }
  void compileReturn(){
    tz_.advance();
    if (!tz_.isSym(';')) {
      compileExpression();
    } else {
      vm_.push(VMSeg::CONST, 0);
    }
    tz_.expectSym(';', "';'");
    vm_.ret();
  }
  void compileExpression(){
    compileTerm();
    while (tz_.hasMore() && tz_.peek().t==TokType::SYM){
      char op = tz_.peek().ch;
      if (string("+-*/&|<=>").find(op)==string::npos) break;
      tz_.advance();
      compileTerm();
      switch(op){
        case '+': vm_.op(VMOp::ADD); break;
        case '-': vm_.op(VMOp::SUB); break;
        case '*': vm_.call("Math.multiply", 2); break;
        case '/': vm_.call("Math.divide", 2); break;
        case '&': vm_.op(VMOp::AND); break;
        case '|': vm_.op(VMOp::OR);  break;
        case '<': vm_.op(VMOp::LT);  break;
        case '>': vm_.op(VMOp::GT);  break;
        case '=': vm_.op(VMOp::EQ);  break;
      }
    }
  }
  void compileTerm(){
    if (!tz_.hasMore()) throw runtime_error("term expected");
    const Token& t = tz_.peek();
    if (t.t==TokType::INTC){
      vm_.push(VMSeg::CONST, t.ival); tz_.advance(); return;
    }
    if (t.t==TokType::STRC){
      string s=t.s; tz_.advance();
      vm_.push(VMSeg::CONST, (int)s.size());
      vm_.call("String.new", 1);
      for (unsigned char c: s){
        vm_.push(VMSeg::CONST, (int)c);
        vm_.call("String.appendChar", 2);
      }
      return;
    }
    if (t.t==TokType::KW){
      switch(t.kw){
        case Kw::TRUE_:  vm_.push(VMSeg::CONST, 0); vm_.op(VMOp::NOT); tz_.advance(); return;
        case Kw::FALSE_: case Kw::NULL_: vm_.push(VMSeg::CONST, 0); tz_.advance(); return;
        case Kw::THIS_:  vm_.push(VMSeg::POINTER, 0); tz_.advance(); return;
        default: throw runtime_error("unsupported keyword in term");
      }
    }
    if (t.t==TokType::SYM && t.ch=='('){
      tz_.advance(); compileExpression(); tz_.expectSym(')', "')'"); return;
    }
    if (t.t==TokType::SYM && (t.ch=='-' || t.ch=='~')){
      char u = tz_.advance().ch;
      compileTerm();
      vm_.op(u=='-'?VMOp::NEG:VMOp::NOT);
      return;
    }
    if (t.t==TokType::ID){
      string id = tz_.advance().s;
      if (tz_.isSym('[')){
        Kind k=st_.kindOf(id); VMSeg sg=kindToSeg(k); int ix=st_.indexOf(id);
        tz_.advance(); compileExpression(); tz_.expectSym(']', "']'");
        vm_.push(sg, ix);
        vm_.op(VMOp::ADD);
        vm_.pop(VMSeg::POINTER, 1);
        vm_.push(VMSeg::THAT, 0);
        return;
      }
      if (tz_.isSym('(') || tz_.isSym('.')){
        subCallWithFirst(id);
        return;
      }
      Kind k=st_.kindOf(id); VMSeg sg=kindToSeg(k); int ix=st_.indexOf(id);
      vm_.push(sg, ix);
      return;
    }
    throw runtime_error("unrecognized term");
  }
  int compileExpressionList(){
    if (tz_.isSym(')')) return 0;
    int n=1; compileExpression();
    while (tz_.isSym(',')){ tz_.advance(); compileExpression(); ++n; }
    return n;
  }
  void compileSubroutineCall(){
    string first = tz_.expectId("call first");
    subCallWithFirst(first);
  }
  void subCallWithFirst(const string& first){
    string callee;
    int nArgs=0;
    if (tz_.isSym('.')){
      tz_.advance();
      string name2 = tz_.expectId("subName");
      Kind k = st_.kindOf(first);
      if (k!=Kind::NONE){
        vm_.push(kindToSeg(k), st_.indexOf(first));
        callee = st_.typeOf(first)+"."+name2;
        nArgs = 1;
      } else {
        callee = first+"."+name2;
      }
    } else {
      vm_.push(VMSeg::POINTER, 0);
      callee = className_+"."+first;
      nArgs = 1;
    }
    tz_.expectSym('(', "(");
    nArgs += compileExpressionList();
    tz_.expectSym(')', ")");
    vm_.call(callee, nArgs);
  }
};

static void compileOne(const fs::path& jack){
  fs::path out = jack; out.replace_extension(".vm");
  Tokenizer tz(jack.string());
  VMWriter  vm(out.string());
  SymbolTable st;
  Engine eng(tz, vm, st);
  eng.compileClass();
  vm.close();
}

int main(int argc, char** argv){
  if (argc!=2) return 1;
  fs::path p(argv[1]);
  vector<fs::path> files;
  if (fs::is_directory(p)){
    for (auto& e: fs::directory_iterator(p))
      if (e.is_regular_file() && e.path().extension()==".jack")
        files.push_back(e.path());
  } else if (fs::is_regular_file(p) && p.extension()==".jack"){
    files.push_back(p);
  } else {
    return 1;
  }
  for (auto& f: files) compileOne(f);
  return 0;
}
