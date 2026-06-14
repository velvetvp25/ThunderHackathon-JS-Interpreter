#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <functional> // Added for std::function recursion

using namespace std;

// ============================================================================
// 1. TYPE SYSTEM & LEXICAL ENVIRONMENTS
// ============================================================================

struct Stmt;
struct Environment;

struct JSFunction {
    vector<string> params;
    shared_ptr<Stmt> body;
    shared_ptr<Environment> closure;
    bool hasRest = false;
};

struct JSValue {
    enum Type { UNDEFINED, NUMBER, STRING, BOOLEAN, ARRAY, FUNCTION, OBJECT } type;
    double numVal;
    string strVal;
    bool boolVal;
    shared_ptr<vector<JSValue>> arrVal;
    shared_ptr<JSFunction> funcVal;
    shared_ptr<unordered_map<string, JSValue>> objVal;

    JSValue() : type(UNDEFINED), numVal(0), boolVal(false) {}
    JSValue(double n) : type(NUMBER), numVal(n), boolVal(false) {}
    JSValue(string s) : type(STRING), strVal(s), numVal(0), boolVal(false) {}
    JSValue(bool b) : type(BOOLEAN), boolVal(b), numVal(0) {}
    JSValue(shared_ptr<vector<JSValue>> a) : type(ARRAY), arrVal(a), numVal(0), boolVal(false) {}
    JSValue(shared_ptr<JSFunction> f) : type(FUNCTION), funcVal(f), numVal(0), boolVal(false) {}
    JSValue(shared_ptr<unordered_map<string, JSValue>> o) : type(OBJECT), objVal(o), numVal(0), boolVal(false) {}

    string toString() const {
        if (type == NUMBER) {
            string s = to_string(numVal);
            s.erase(s.find_last_not_of('0') + 1, string::npos);
            if (s.back() == '.') s.pop_back();
            return s;
        }
        if (type == STRING) return strVal;
        if (type == BOOLEAN) return boolVal ? "true" : "false";
        if (type == ARRAY) return "[Array]";
        if (type == FUNCTION) return "[Function]";
        if (type == OBJECT) return "[object Object]";
        return "undefined";
    }

    bool isTruthy() const {
        if (type == BOOLEAN) return boolVal;
        if (type == NUMBER) return numVal != 0;
        if (type == STRING) return !strVal.empty();
        if (type == ARRAY || type == FUNCTION || type == OBJECT) return true;
        return false;
    }

    bool operator==(const JSValue &o) const {
        if (type != o.type) return false;
        if (type == NUMBER) return numVal == o.numVal;
        if (type == STRING) return strVal == o.strVal;
        if (type == BOOLEAN) return boolVal == o.boolVal;
        return false;
    }
};

struct Environment : public enable_shared_from_this<Environment> {
    unordered_map<string, JSValue> vars;
    shared_ptr<Environment> parent;

    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    void declare(const string &name, JSValue val) { vars[name] = val; }

    void assign(const string &name, JSValue val) {
        if (vars.count(name)) { vars[name] = val; return; }
        if (parent) { parent->assign(name, val); return; }
        throw runtime_error("ReferenceError: " + name + " is not defined");
    }

    JSValue get(const string &name) {
        if (vars.count(name)) return vars[name];
        if (parent) return parent->get(name);
        throw runtime_error("ReferenceError: " + name + " is not defined");
    }
};

struct ReturnException { JSValue value; };
struct BreakException {};
struct ContinueException {};

// ============================================================================
// 2. LEXER (TOKENIZER)
// ============================================================================

enum class TokenType {
    NUM, STR, IDENT, LET, IF, ELSE, FOR, WHILE, FUNCTION, RETURN, BREAK, CONTINUE,
    PLUS, MINUS, MUL, DIV, MOD, ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, MUL_ASSIGN, DIV_ASSIGN, 
    EQ, NEQ, LT, GT, LTE, GTE, AND, OR, NOT, LPAREN, RPAREN, LBRACE, RBRACE, 
    LBRACKET, RBRACKET, COMMA, SEMICOLON, DOT, COLON, ARROW, INC, DEC, SPREAD, EOF_TOK
};

struct Token { TokenType type; string value; };

class Lexer {
    string src; size_t pos = 0;
    
    char peek(int offset = 0) const { return pos + offset < src.size() ? src[pos + offset] : '\0'; }
    char advance() { return pos < src.size() ? src[pos++] : '\0'; }
    
    void skipWhitespaceAndComments() {
        while (true) {
            if (isspace(peek())) { advance(); }
            else if (peek() == '/' && peek(1) == '/') {
                while (peek() != '\n' && peek() != '\0') advance();
            }
            else if (peek() == '/' && peek(1) == '*') {
                advance(); advance();
                while (peek() != '\0' && !(peek() == '*' && peek(1) == '/')) advance();
                if (peek() != '\0') { advance(); advance(); }
            }
            else { break; }
        }
    }

public:
    Lexer(string source) : src(source) {}
    
    Token nextToken() {
        skipWhitespaceAndComments();
        if (pos >= src.size()) return {TokenType::EOF_TOK, ""};
        char c = peek();

        if (isalpha(c) || c == '_') {
            string ident = "";
            while (isalnum(peek()) || peek() == '_') ident += advance();
            if (ident == "let" || ident == "const" || ident == "var") return {TokenType::LET, ident};
            if (ident == "if") return {TokenType::IF, ident};
            if (ident == "else") return {TokenType::ELSE, ident};
            if (ident == "for") return {TokenType::FOR, ident};
            if (ident == "while") return {TokenType::WHILE, ident};
            if (ident == "function") return {TokenType::FUNCTION, ident};
            if (ident == "return") return {TokenType::RETURN, ident};
            if (ident == "break") return {TokenType::BREAK, ident};
            if (ident == "continue") return {TokenType::CONTINUE, ident};
            return {TokenType::IDENT, ident};
        }
        if (isdigit(c)) {
            string num = "";
            while (isdigit(peek()) || peek() == '.') num += advance();
            return {TokenType::NUM, num};
        }
        if (c == '"' || c == '\'') {
            char quote = advance(); string str = "";
            while (peek() != quote && peek() != '\0') str += advance();
            advance(); return {TokenType::STR, str};
        }

        advance(); string op(1, c);
        
        if (c == '.' && peek() == '.' && peek(1) == '.') { op += advance(); op += advance(); return {TokenType::SPREAD, op}; }
        
        if (c == '+' && peek() == '+') { op += advance(); return {TokenType::INC, op}; }
        if (c == '-' && peek() == '-') { op += advance(); return {TokenType::DEC, op}; }
        if (c == '+' && peek() == '=') { op += advance(); return {TokenType::PLUS_ASSIGN, op}; }
        if (c == '-' && peek() == '=') { op += advance(); return {TokenType::MINUS_ASSIGN, op}; }
        if (c == '*' && peek() == '=') { op += advance(); return {TokenType::MUL_ASSIGN, op}; }
        if (c == '/' && peek() == '=') { op += advance(); return {TokenType::DIV_ASSIGN, op}; }
        if (c == '=' && peek() == '>') { op += advance(); return {TokenType::ARROW, op}; }
        if (c == '=' && peek() == '=') { op += advance(); if (peek() == '=') op += advance(); return {TokenType::EQ, op}; }
        if (c == '!' && peek() == '=') { op += advance(); if (peek() == '=') op += advance(); return {TokenType::NEQ, op}; }
        if (c == '<' && peek() == '=') { op += advance(); return {TokenType::LTE, op}; }
        if (c == '>' && peek() == '=') { op += advance(); return {TokenType::GTE, op}; }
        if (c == '&' && peek() == '&') { op += advance(); return {TokenType::AND, op}; }
        if (c == '|' && peek() == '|') { op += advance(); return {TokenType::OR, op}; }

        switch (c) {
            case '+': return {TokenType::PLUS, op}; case '-': return {TokenType::MINUS, op};
            case '*': return {TokenType::MUL, op}; case '/': return {TokenType::DIV, op};
            case '%': return {TokenType::MOD, op}; case '=': return {TokenType::ASSIGN, op};
            case '<': return {TokenType::LT, op}; case '>': return {TokenType::GT, op};
            case '!': return {TokenType::NOT, op}; case '(': return {TokenType::LPAREN, op};
            case ')': return {TokenType::RPAREN, op}; case '{': return {TokenType::LBRACE, op};
            case '}': return {TokenType::RBRACE, op}; case '[': return {TokenType::LBRACKET, op};
            case ']': return {TokenType::RBRACKET, op}; case ',': return {TokenType::COMMA, op};
            case ';': return {TokenType::SEMICOLON, op}; case ':': return {TokenType::COLON, op};
            case '.': return {TokenType::DOT, op};
        }
        return {TokenType::EOF_TOK, ""};
    }
};

// ============================================================================
// 3. AST NODES
// ============================================================================

struct Expr { virtual JSValue eval(shared_ptr<Environment> env) = 0; virtual ~Expr() = default; };
struct Stmt { virtual void execute(shared_ptr<Environment> env) = 0; virtual ~Stmt() = default; };

struct LiteralExpr : Expr {
    JSValue value;
    LiteralExpr(JSValue v) : value(v) {}
    JSValue eval(shared_ptr<Environment> env) override { return value; }
};

struct VarExpr : Expr {
    string name;
    VarExpr(string n) : name(n) {}
    JSValue eval(shared_ptr<Environment> env) override { return env->get(name); }
};

struct UnaryExpr : Expr {
    TokenType op; shared_ptr<Expr> right;
    UnaryExpr(TokenType o, shared_ptr<Expr> r) : op(o), right(r) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue r = right->eval(env);
        if (op == TokenType::MINUS) return JSValue(-r.numVal);
        if (op == TokenType::NOT) return JSValue(!r.isTruthy());
        return JSValue();
    }
};

struct AssignExpr : Expr {
    string name; shared_ptr<Expr> value; TokenType op;
    AssignExpr(string n, shared_ptr<Expr> v, TokenType o = TokenType::ASSIGN) : name(n), value(v), op(o) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue val = value->eval(env);
        if (op != TokenType::ASSIGN) {
            JSValue curr = env->get(name);
            if (op == TokenType::PLUS_ASSIGN) {
                if (curr.type == JSValue::STRING || val.type == JSValue::STRING) val = JSValue(curr.toString() + val.toString());
                else val = JSValue(curr.numVal + val.numVal);
            } else if (op == TokenType::MINUS_ASSIGN) val = JSValue(curr.numVal - val.numVal);
            else if (op == TokenType::MUL_ASSIGN) val = JSValue(curr.numVal * val.numVal);
            else if (op == TokenType::DIV_ASSIGN) val = JSValue(curr.numVal / val.numVal);
        }
        env->assign(name, val); return val;
    }
};

struct ArrayAccessExpr : Expr {
    shared_ptr<Expr> arrayExpr, indexExpr;
    ArrayAccessExpr(shared_ptr<Expr> arr, shared_ptr<Expr> idx) : arrayExpr(arr), indexExpr(idx) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue arr = arrayExpr->eval(env); JSValue idx = indexExpr->eval(env);
        if (arr.type != JSValue::ARRAY) throw runtime_error("TypeError: Cannot index non-array");
        int i = idx.numVal;
        if (i >= 0 && i < arr.arrVal->size()) return (*arr.arrVal)[i];
        return JSValue();
    }
};

struct ArrayAssignExpr : Expr {
    shared_ptr<Expr> arrayExpr, indexExpr, valueExpr; TokenType op;
    ArrayAssignExpr(shared_ptr<Expr> arr, shared_ptr<Expr> idx, shared_ptr<Expr> val, TokenType o = TokenType::ASSIGN) 
        : arrayExpr(arr), indexExpr(idx), valueExpr(val), op(o) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue arr = arrayExpr->eval(env); JSValue idx = indexExpr->eval(env); JSValue val = valueExpr->eval(env);
        if (arr.type != JSValue::ARRAY) throw runtime_error("TypeError: Cannot index non-array");
        int i = idx.numVal;
        if (i >= arr.arrVal->size()) arr.arrVal->resize(i + 1, JSValue());
        
        if (op != TokenType::ASSIGN) {
            JSValue curr = (*arr.arrVal)[i];
            if (op == TokenType::PLUS_ASSIGN) {
                if (curr.type == JSValue::STRING || val.type == JSValue::STRING) val = JSValue(curr.toString() + val.toString());
                else val = JSValue(curr.numVal + val.numVal);
            } else if (op == TokenType::MINUS_ASSIGN) val = JSValue(curr.numVal - val.numVal);
            else if (op == TokenType::MUL_ASSIGN) val = JSValue(curr.numVal * val.numVal);
            else if (op == TokenType::DIV_ASSIGN) val = JSValue(curr.numVal / val.numVal);
        }
        (*arr.arrVal)[i] = val; return val;
    }
};

struct MemberAccessExpr : Expr {
    shared_ptr<Expr> object; string property;
    MemberAccessExpr(shared_ptr<Expr> obj, string prop) : object(obj), property(prop) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue obj = object->eval(env);
        if (obj.type == JSValue::ARRAY && property == "length") return JSValue((double)obj.arrVal->size());
        if (obj.type == JSValue::STRING && property == "length") return JSValue((double)obj.strVal.size());
        if (obj.type == JSValue::OBJECT) {
            if (obj.objVal->count(property)) return (*obj.objVal)[property];
        }
        return JSValue();
    }
};

struct MemberAssignExpr : Expr {
    shared_ptr<Expr> object; string property; shared_ptr<Expr> value; TokenType op;
    MemberAssignExpr(shared_ptr<Expr> obj, string prop, shared_ptr<Expr> val, TokenType o = TokenType::ASSIGN) 
        : object(obj), property(prop), value(val), op(o) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue obj = object->eval(env); JSValue val = value->eval(env);
        if (obj.type == JSValue::OBJECT) { 
            if (op != TokenType::ASSIGN) {
                JSValue curr = obj.objVal->count(property) ? (*obj.objVal)[property] : JSValue(0.0);
                if (op == TokenType::PLUS_ASSIGN) {
                    if (curr.type == JSValue::STRING || val.type == JSValue::STRING) val = JSValue(curr.toString() + val.toString());
                    else val = JSValue(curr.numVal + val.numVal);
                } else if (op == TokenType::MINUS_ASSIGN) val = JSValue(curr.numVal - val.numVal);
                else if (op == TokenType::MUL_ASSIGN) val = JSValue(curr.numVal * val.numVal);
                else if (op == TokenType::DIV_ASSIGN) val = JSValue(curr.numVal / val.numVal);
            }
            (*obj.objVal)[property] = val; return val; 
        }
        throw runtime_error("TypeError: Cannot assign to property of non-object");
    }
};

struct BinaryExpr : Expr {
    shared_ptr<Expr> left, right; TokenType op;
    BinaryExpr(shared_ptr<Expr> l, TokenType o, shared_ptr<Expr> r) : left(l), op(o), right(r) {}
    JSValue eval(shared_ptr<Environment> env) override {
        JSValue l = left->eval(env); JSValue r = right->eval(env);
        if (op == TokenType::PLUS) {
            if (l.type == JSValue::STRING || r.type == JSValue::STRING) return JSValue(l.toString() + r.toString());
            return JSValue(l.numVal + r.numVal);
        }
        if (op == TokenType::MINUS) return JSValue(l.numVal - r.numVal);
        if (op == TokenType::MUL) return JSValue(l.numVal * r.numVal);
        if (op == TokenType::DIV) return JSValue(l.numVal / r.numVal);
        if (op == TokenType::MOD) return JSValue((double)((long long)l.numVal % (long long)r.numVal));
        if (op == TokenType::LT) return JSValue(l.numVal < r.numVal);
        if (op == TokenType::GT) return JSValue(l.numVal > r.numVal);
        if (op == TokenType::LTE) return JSValue(l.numVal <= r.numVal);
        if (op == TokenType::GTE) return JSValue(l.numVal >= r.numVal);
        if (op == TokenType::EQ) return JSValue(l == r);
        if (op == TokenType::NEQ) return JSValue(!(l == r));
        if (op == TokenType::AND) return JSValue(l.isTruthy() && r.isTruthy());
        if (op == TokenType::OR) return JSValue(l.isTruthy() || r.isTruthy());
        return JSValue();
    }
};

struct ArrayLiteralExpr : Expr {
    vector<pair<bool, shared_ptr<Expr>>> elements;
    ArrayLiteralExpr(vector<pair<bool, shared_ptr<Expr>>> el) : elements(el) {}
    JSValue eval(shared_ptr<Environment> env) override {
        auto arr = make_shared<vector<JSValue>>();
        for (auto &el : elements) {
            if (el.first) {
                JSValue spreadObj = el.second->eval(env);
                if (spreadObj.type == JSValue::ARRAY) {
                    for (auto &v : *spreadObj.arrVal) arr->push_back(v);
                } else {
                    throw runtime_error("TypeError: Spread target is not iterable");
                }
            } else {
                arr->push_back(el.second->eval(env));
            }
        }
        return JSValue(arr);
    }
};

struct ObjectLiteralExpr : Expr {
    vector<pair<string, shared_ptr<Expr>>> properties;
    ObjectLiteralExpr(vector<pair<string, shared_ptr<Expr>>> prop) : properties(prop) {}
    JSValue eval(shared_ptr<Environment> env) override {
        auto obj = make_shared<unordered_map<string, JSValue>>();
        for (auto &p : properties) (*obj)[p.first] = p.second->eval(env);
        return JSValue(obj);
    }
};

struct CallExpr : Expr {
    shared_ptr<Expr> callee; vector<pair<bool, shared_ptr<Expr>>> args;
    CallExpr(shared_ptr<Expr> c, vector<pair<bool, shared_ptr<Expr>>> a) : callee(c), args(a) {}
    
    JSValue eval(shared_ptr<Environment> env) override {
        vector<JSValue> evalArgs;
        for (auto &a : args) {
            if (a.first) {
                JSValue arr = a.second->eval(env);
                if (arr.type == JSValue::ARRAY) {
                    for (auto &v : *arr.arrVal) evalArgs.push_back(v);
                }
            } else {
                evalArgs.push_back(a.second->eval(env));
            }
        }

        if (auto mem = dynamic_cast<MemberAccessExpr *>(callee.get())) {
            if (auto var = dynamic_cast<VarExpr *>(mem->object.get())) {
                if (var->name == "console" && mem->property == "log") {
                    for (size_t i = 0; i < evalArgs.size(); i++) cout << evalArgs[i].toString() << (i == evalArgs.size() - 1 ? "" : " ");
                    cout << "\n"; return JSValue();
                }
                if (var->name == "Math") {
                    if (mem->property == "random") return JSValue((double)rand() / RAND_MAX);
                    if (evalArgs.empty()) return JSValue(0.0);
                    double val = evalArgs[0].numVal;
                    if (mem->property == "floor") return JSValue(floor(val));
                    if (mem->property == "ceil") return JSValue(ceil(val));
                    if (mem->property == "round") return JSValue(round(val));
                    if (mem->property == "abs") return JSValue(abs(val));
                    if (mem->property == "max") {
                        for (size_t i = 1; i < evalArgs.size(); i++) val = max(val, evalArgs[i].numVal);
                        return JSValue(val);
                    }
                }
            }

            JSValue obj = mem->object->eval(env);

            if (obj.type == JSValue::ARRAY) {
                if (mem->property == "push") {
                    for (auto &a : evalArgs) obj.arrVal->push_back(a);
                    return JSValue((double)obj.arrVal->size());
                }
                if (mem->property == "pop") {
                    if (obj.arrVal->empty()) return JSValue();
                    JSValue back = obj.arrVal->back(); obj.arrVal->pop_back(); return back;
                }
                if (mem->property == "shift") {
                    if (obj.arrVal->empty()) return JSValue();
                    JSValue front = obj.arrVal->front(); obj.arrVal->erase(obj.arrVal->begin()); return front;
                }
                if (mem->property == "unshift") {
                    obj.arrVal->insert(obj.arrVal->begin(), evalArgs.begin(), evalArgs.end());
                    return JSValue((double)obj.arrVal->size());
                }
                if (mem->property == "reverse") {
                    std::reverse(obj.arrVal->begin(), obj.arrVal->end()); return obj;
                }
                if (mem->property == "join") {
                    string sep = evalArgs.size() > 0 ? evalArgs[0].toString() : ","; string res = "";
                    for (size_t i = 0; i < obj.arrVal->size(); i++) {
                        res += (*obj.arrVal)[i].toString();
                        if (i < obj.arrVal->size() - 1) res += sep;
                    }
                    return JSValue(res);
                }
                if (mem->property == "concat") {
                    auto resArr = make_shared<vector<JSValue>>(*obj.arrVal);
                    for (auto &argVal : evalArgs) {
                        if (argVal.type == JSValue::ARRAY) {
                            for (auto &v : *argVal.arrVal) resArr->push_back(v);
                        } else {
                            resArr->push_back(argVal);
                        }
                    }
                    return JSValue(resArr);
                }
                if (mem->property == "includes" || mem->property == "indexOf") {
                    JSValue target = evalArgs.size() > 0 ? evalArgs[0] : JSValue();
                    for (size_t i = 0; i < obj.arrVal->size(); i++) {
                        if ((*obj.arrVal)[i] == target) {
                            if (mem->property == "indexOf") return JSValue((double)i);
                            return JSValue(true);
                        }
                    }
                    if (mem->property == "indexOf") return JSValue(-1.0);
                    return JSValue(false);
                }
                if (mem->property == "slice") {
                    int len = obj.arrVal->size();
                    int start = evalArgs.size() > 0 ? evalArgs[0].numVal : 0;
                    if (start < 0) start = max(0, len + start); else start = min(len, start);
                    int end = evalArgs.size() > 1 ? evalArgs[1].numVal : len;
                    if (end < 0) end = max(0, len + end); else end = min(len, end);
                    auto resArr = make_shared<vector<JSValue>>();
                    for (int i = start; i < end; i++) resArr->push_back((*obj.arrVal)[i]);
                    return JSValue(resArr);
                }
                if (mem->property == "splice") {
                    int len = obj.arrVal->size();
                    int start = evalArgs.size() > 0 ? evalArgs[0].numVal : 0;
                    if (start < 0) start = max(0, len + start); else start = min(len, start);
                    int deleteCount = evalArgs.size() > 1 ? evalArgs[1].numVal : len - start;
                    deleteCount = max(0, min(len - start, deleteCount));
                    auto resArr = make_shared<vector<JSValue>>();
                    for(int i = start; i < start + deleteCount; i++) resArr->push_back((*obj.arrVal)[i]);
                    obj.arrVal->erase(obj.arrVal->begin() + start, obj.arrVal->begin() + start + deleteCount);
                    vector<JSValue> inserts;
                    for (size_t i = 2; i < evalArgs.size(); i++) inserts.push_back(evalArgs[i]);
                    obj.arrVal->insert(obj.arrVal->begin() + start, inserts.begin(), inserts.end());
                    return JSValue(resArr);
                }
                
                // NEW: Array.prototype.flat()
                if (mem->property == "flat") {
                    int depth = evalArgs.size() > 0 ? evalArgs[0].numVal : 1;
                    auto resArr = make_shared<vector<JSValue>>();
                    
                    std::function<void(const shared_ptr<vector<JSValue>>&, int)> flatten;
                    flatten = [&](const shared_ptr<vector<JSValue>>& arr, int d) {
                        for (const auto& item : *arr) {
                            if (item.type == JSValue::ARRAY && d > 0) {
                                flatten(item.arrVal, d - 1);
                            } else {
                                resArr->push_back(item);
                            }
                        }
                    };
                    flatten(obj.arrVal, depth);
                    return JSValue(resArr);
                }

                if (mem->property == "sort") {
                    if (evalArgs.empty()) {
                        std::sort(obj.arrVal->begin(), obj.arrVal->end(), [](const JSValue& a, const JSValue& b) {
                            return a.toString() < b.toString();
                        });
                    } else {
                        JSValue cb = evalArgs[0];
                        if (cb.type == JSValue::FUNCTION) {
                            std::sort(obj.arrVal->begin(), obj.arrVal->end(), [&](const JSValue& a, const JSValue& b) {
                                auto closureEnv = make_shared<Environment>(cb.funcVal->closure);
                                vector<JSValue> cbArgs = {a, b};
                                size_t pSize = cb.funcVal->params.size();
                                for (size_t i = 0; i < pSize; i++) {
                                    if (i == pSize - 1 && cb.funcVal->hasRest) {
                                        auto restArr = make_shared<vector<JSValue>>();
                                        for (size_t j = i; j < cbArgs.size(); j++) restArr->push_back(cbArgs[j]);
                                        closureEnv->declare(cb.funcVal->params[i], JSValue(restArr));
                                    } else {
                                        closureEnv->declare(cb.funcVal->params[i], i < cbArgs.size() ? cbArgs[i] : JSValue());
                                    }
                                }
                                try { cb.funcVal->body->execute(closureEnv); }
                                catch (const ReturnException& ret) { return ret.value.numVal < 0; }
                                return false;
                            });
                        }
                    }
                    return obj;
                }

                if (mem->property == "map" || mem->property == "filter" || mem->property == "find" ||
                    mem->property == "some" || mem->property == "every" || mem->property == "reduce" ||
                    mem->property == "forEach" || mem->property == "flatMap") {
                    if (evalArgs.empty()) throw runtime_error("TypeError: Callback required");
                    JSValue callback = evalArgs[0];
                    if (callback.type != JSValue::FUNCTION) throw runtime_error("TypeError: Callback must be a function");

                    auto runCallback = [&](const vector<JSValue>& cbArgs) -> JSValue {
                        auto closureEnv = make_shared<Environment>(callback.funcVal->closure);
                        size_t pSize = callback.funcVal->params.size();
                        for (size_t i = 0; i < pSize; i++) {
                            if (i == pSize - 1 && callback.funcVal->hasRest) {
                                auto restArr = make_shared<vector<JSValue>>();
                                for (size_t j = i; j < cbArgs.size(); j++) restArr->push_back(cbArgs[j]);
                                closureEnv->declare(callback.funcVal->params[i], JSValue(restArr));
                            } else {
                                closureEnv->declare(callback.funcVal->params[i], i < cbArgs.size() ? cbArgs[i] : JSValue());
                            }
                        }
                        try { callback.funcVal->body->execute(closureEnv); }
                        catch (const ReturnException& ret) { return ret.value; }
                        return JSValue();
                    };

                    if (mem->property == "forEach") {
                        for (size_t i = 0; i < obj.arrVal->size(); i++) runCallback({(*obj.arrVal)[i], JSValue((double)i), obj});
                        return JSValue();
                    }
                    if (mem->property == "map") {
                        auto resArr = make_shared<vector<JSValue>>();
                        for (size_t i = 0; i < obj.arrVal->size(); i++) resArr->push_back(runCallback({(*obj.arrVal)[i], JSValue((double)i), obj}));
                        return JSValue(resArr);
                    }
                    // NEW: Array.prototype.flatMap()
                    if (mem->property == "flatMap") {
                        auto resArr = make_shared<vector<JSValue>>();
                        for (size_t i = 0; i < obj.arrVal->size(); i++) {
                            JSValue mapped = runCallback({(*obj.arrVal)[i], JSValue((double)i), obj});
                            if (mapped.type == JSValue::ARRAY) {
                                for (const auto& v : *mapped.arrVal) resArr->push_back(v);
                            } else {
                                resArr->push_back(mapped);
                            }
                        }
                        return JSValue(resArr);
                    }
                    if (mem->property == "filter") {
                        auto resArr = make_shared<vector<JSValue>>();
                        for (size_t i = 0; i < obj.arrVal->size(); i++) {
                            if (runCallback({(*obj.arrVal)[i], JSValue((double)i), obj}).isTruthy()) resArr->push_back((*obj.arrVal)[i]);
                        }
                        return JSValue(resArr);
                    }
                    if (mem->property == "find") {
                        for (size_t i = 0; i < obj.arrVal->size(); i++) {
                            if (runCallback({(*obj.arrVal)[i], JSValue((double)i), obj}).isTruthy()) return (*obj.arrVal)[i];
                        }
                        return JSValue();
                    }
                    if (mem->property == "some") {
                        for (size_t i = 0; i < obj.arrVal->size(); i++) {
                            if (runCallback({(*obj.arrVal)[i], JSValue((double)i), obj}).isTruthy()) return JSValue(true);
                        }
                        return JSValue(false);
                    }
                    if (mem->property == "every") {
                        for (size_t i = 0; i < obj.arrVal->size(); i++) {
                            if (!runCallback({(*obj.arrVal)[i], JSValue((double)i), obj}).isTruthy()) return JSValue(false);
                        }
                        return JSValue(true);
                    }
                    if (mem->property == "reduce") {
                        size_t startIdx = 0; JSValue acc;
                        if (evalArgs.size() > 1) { acc = evalArgs[1]; } 
                        else {
                            if (obj.arrVal->empty()) throw runtime_error("TypeError: Reduce of empty array");
                            acc = (*obj.arrVal)[0]; startIdx = 1;
                        }
                        for (size_t i = startIdx; i < obj.arrVal->size(); i++) acc = runCallback({acc, (*obj.arrVal)[i], JSValue((double)i), obj});
                        return acc;
                    }
                }
            }

            if (obj.type == JSValue::STRING) {
                if (mem->property == "trim") {
                    int left = 0, right = obj.strVal.size() - 1;
                    while (left < obj.strVal.size() && isspace(obj.strVal[left])) left++;
                    while (right >= left && isspace(obj.strVal[right])) right--;
                    if (left > right) return JSValue("");
                    return JSValue(obj.strVal.substr(left, right - left + 1));
                }
                if (mem->property == "toUpperCase") {
                    string res = obj.strVal; for (char &c : res) c = toupper(c); return JSValue(res);
                }
                if (mem->property == "toLowerCase") {
                    string res = obj.strVal; for (char &c : res) c = tolower(c); return JSValue(res);
                }
                if (mem->property == "split") {
                    string sep = evalArgs.size() > 0 ? evalArgs[0].toString() : "";
                    auto arr = make_shared<vector<JSValue>>();
                    if (sep == "") {
                        for (char c : obj.strVal) arr->push_back(JSValue(string(1, c)));
                    } else {
                        size_t pos = 0, found;
                        while ((found = obj.strVal.find(sep, pos)) != string::npos) {
                            arr->push_back(JSValue(obj.strVal.substr(pos, found - pos))); pos = found + sep.length();
                        }
                        arr->push_back(JSValue(obj.strVal.substr(pos)));
                    }
                    return JSValue(arr);
                }
                if (mem->property == "replace" || mem->property == "replaceAll") {
                    string target = evalArgs.size() > 0 ? evalArgs[0].toString() : "";
                    string rep = evalArgs.size() > 1 ? evalArgs[1].toString() : "";
                    string res = obj.strVal; size_t pos = 0;
                    while ((pos = res.find(target, pos)) != string::npos) {
                        res.replace(pos, target.length(), rep); pos += rep.length();
                        if (mem->property == "replace") break;
                    }
                    return JSValue(res);
                }
                if (mem->property == "indexOf" || mem->property == "includes" || mem->property == "startsWith" || mem->property == "endsWith") {
                    string target = evalArgs.size() > 0 ? evalArgs[0].toString() : "undefined";
                    if (mem->property == "indexOf") {
                        size_t pos = obj.strVal.find(target);
                        return JSValue(pos == string::npos ? -1.0 : (double)pos);
                    }
                    if (mem->property == "includes") {
                        return JSValue(obj.strVal.find(target) != string::npos);
                    }
                    if (mem->property == "startsWith") {
                        return JSValue(obj.strVal.rfind(target, 0) == 0);
                    }
                    if (mem->property == "endsWith") {
                        if (target.size() > obj.strVal.size()) return JSValue(false);
                        return JSValue(std::equal(target.rbegin(), target.rend(), obj.strVal.rbegin()));
                    }
                }
                if (mem->property == "slice" || mem->property == "substring") {
                    int len = obj.strVal.size();
                    int start = evalArgs.size() > 0 ? evalArgs[0].numVal : 0;
                    int end = evalArgs.size() > 1 ? evalArgs[1].numVal : len;
                    
                    if (mem->property == "substring") {
                        start = max(0, min(len, start));
                        end = max(0, min(len, end));
                        if (start > end) swap(start, end);
                    } else {
                        if (start < 0) start = max(0, len + start); else start = min(len, start);
                        if (end < 0) end = max(0, len + end); else end = min(len, end);
                    }
                    if (start >= end) return JSValue("");
                    return JSValue(obj.strVal.substr(start, end - start));
                }
            }
        }

        JSValue func = callee->eval(env);
        if (func.type != JSValue::FUNCTION) throw runtime_error("TypeError: not a function");

        auto closureEnv = make_shared<Environment>(func.funcVal->closure);
        size_t pSize = func.funcVal->params.size();
        for (size_t i = 0; i < pSize; i++) {
            if (i == pSize - 1 && func.funcVal->hasRest) {
                auto restArr = make_shared<vector<JSValue>>();
                for (size_t j = i; j < evalArgs.size(); j++) restArr->push_back(evalArgs[j]);
                closureEnv->declare(func.funcVal->params[i], JSValue(restArr));
            } else {
                closureEnv->declare(func.funcVal->params[i], i < evalArgs.size() ? evalArgs[i] : JSValue());
            }
        }
        
        try { func.funcVal->body->execute(closureEnv); }
        catch (const ReturnException &ret) { return ret.value; }
        return JSValue();
    }
};

struct FuncExpr : Expr {
    vector<string> params; shared_ptr<Stmt> body; bool hasRest;
    FuncExpr(vector<string> p, shared_ptr<Stmt> b, bool rest = false) : params(p), body(b), hasRest(rest) {}
    JSValue eval(shared_ptr<Environment> env) override {
        auto fn = make_shared<JSFunction>();
        fn->params = params; fn->body = body; fn->closure = env; fn->hasRest = hasRest;
        return JSValue(fn);
    }
};

// Statements
struct BlockStmt : Stmt {
    vector<shared_ptr<Stmt>> statements;
    void execute(shared_ptr<Environment> env) override {
        auto blockEnv = make_shared<Environment>(env);
        for (auto &stmt : statements) stmt->execute(blockEnv);
    }
};

struct ExprStmt : Stmt {
    shared_ptr<Expr> expr;
    ExprStmt(shared_ptr<Expr> e) : expr(e) {}
    void execute(shared_ptr<Environment> env) override { expr->eval(env); }
};

struct LetStmt : Stmt {
    string name; shared_ptr<Expr> initializer;
    LetStmt(string n, shared_ptr<Expr> i) : name(n), initializer(i) {}
    void execute(shared_ptr<Environment> env) override { env->declare(name, initializer ? initializer->eval(env) : JSValue()); }
};

struct IfStmt : Stmt {
    shared_ptr<Expr> condition; shared_ptr<Stmt> thenBranch, elseBranch;
    IfStmt(shared_ptr<Expr> c, shared_ptr<Stmt> t, shared_ptr<Stmt> e) : condition(c), thenBranch(t), elseBranch(e) {}
    void execute(shared_ptr<Environment> env) override {
        if (condition->eval(env).isTruthy()) thenBranch->execute(env);
        else if (elseBranch) elseBranch->execute(env);
    }
};

struct WhileStmt : Stmt {
    shared_ptr<Expr> condition; shared_ptr<Stmt> body;
    WhileStmt(shared_ptr<Expr> c, shared_ptr<Stmt> b) : condition(c), body(b) {}
    void execute(shared_ptr<Environment> env) override {
        while (condition->eval(env).isTruthy()) {
            try { body->execute(env); } 
            catch (const BreakException &) { break; }
            catch (const ContinueException &) { continue; }
        }
    }
};

struct ForStmt : Stmt {
    shared_ptr<Stmt> init; shared_ptr<Expr> cond, update; shared_ptr<Stmt> body;
    ForStmt(shared_ptr<Stmt> i, shared_ptr<Expr> c, shared_ptr<Expr> u, shared_ptr<Stmt> b) : init(i), cond(c), update(u), body(b) {}
    void execute(shared_ptr<Environment> env) override {
        auto forEnv = make_shared<Environment>(env);
        if (init) init->execute(forEnv);
        while (!cond || cond->eval(forEnv).isTruthy()) {
            auto iterationEnv = make_shared<Environment>(forEnv->parent);
            for (auto &pair : forEnv->vars) iterationEnv->declare(pair.first, pair.second);
            
            try { 
                body->execute(iterationEnv); 
            } catch (const BreakException &) { 
                for (auto &pair : iterationEnv->vars) {
                    if (forEnv->vars.count(pair.first)) forEnv->vars[pair.first] = pair.second;
                }
                break; 
            } catch (const ContinueException &) {
                for (auto &pair : iterationEnv->vars) {
                    if (forEnv->vars.count(pair.first)) forEnv->vars[pair.first] = pair.second;
                }
                if (update) update->eval(forEnv);
                continue;
            }
            
            for (auto &pair : iterationEnv->vars) {
                if (forEnv->vars.count(pair.first)) forEnv->vars[pair.first] = pair.second;
            }
            if (update) update->eval(forEnv);
        }
    }
};

struct ReturnStmt : Stmt {
    shared_ptr<Expr> value;
    ReturnStmt(shared_ptr<Expr> v) : value(v) {}
    void execute(shared_ptr<Environment> env) override { throw ReturnException{value ? value->eval(env) : JSValue()}; }
};

struct BreakStmt : Stmt {
    void execute(shared_ptr<Environment> env) override { throw BreakException(); }
};

struct ContinueStmt : Stmt {
    void execute(shared_ptr<Environment> env) override { throw ContinueException(); }
};

struct FuncDeclStmt : Stmt {
    string name; vector<string> params; shared_ptr<Stmt> body; bool hasRest;
    FuncDeclStmt(string n, vector<string> p, shared_ptr<Stmt> b, bool rest = false) : name(n), params(p), body(b), hasRest(rest) {}
    void execute(shared_ptr<Environment> env) override {
        auto fn = make_shared<JSFunction>();
        fn->params = params; fn->body = body; fn->closure = env; fn->hasRest = hasRest;
        env->declare(name, JSValue(fn));
    }
};

// ============================================================================
// 4. PARSER
// ============================================================================

class Parser {
    vector<Token> tokens;
    int pos = 0;

    Token peek(int offset = 0) {
        if (pos + offset < tokens.size()) return tokens[pos + offset];
        return {TokenType::EOF_TOK, ""};
    }

    void advance() { if (pos < tokens.size()) pos++; }

    bool match(TokenType type) {
        if (peek().type == type) { advance(); return true; }
        return false;
    }

    bool isArrowFunctionAhead() {
        if (peek().type != TokenType::LPAREN) return false;
        int level = 0;
        int temp = pos;
        while (temp < tokens.size()) {
            if (tokens[temp].type == TokenType::LPAREN) {
                level++;
            } else if (tokens[temp].type == TokenType::RPAREN) {
                level--;
                if (level == 0) {
                    if (temp + 1 < tokens.size() && tokens[temp + 1].type == TokenType::ARROW) return true;
                    return false;
                }
            }
            temp++;
        }
        return false;
    }

public:
    Parser(string src) {
        Lexer lexer(src);
        Token t = lexer.nextToken();
        while (t.type != TokenType::EOF_TOK) {
            tokens.push_back(t);
            t = lexer.nextToken();
        }
        tokens.push_back({TokenType::EOF_TOK, ""});
    }

    shared_ptr<Stmt> parseArrowBody() {
        if (peek().type == TokenType::LBRACE) return parseStatement();
        return make_shared<ReturnStmt>(parseExpression()); 
    }

    shared_ptr<Expr> parsePrimary() {
        shared_ptr<Expr> expr = nullptr;

        if (match(TokenType::MINUS)) return make_shared<UnaryExpr>(TokenType::MINUS, parsePrimary());
        if (match(TokenType::NOT)) return make_shared<UnaryExpr>(TokenType::NOT, parsePrimary());
        
        if (match(TokenType::FUNCTION)) {
            if (peek().type == TokenType::IDENT) advance(); 
            match(TokenType::LPAREN);
            vector<string> params; bool hasRest = false;
            if (peek().type != TokenType::RPAREN) {
                do { 
                    if (match(TokenType::SPREAD)) {
                        params.push_back(peek().value); advance();
                        hasRest = true; break;
                    }
                    params.push_back(peek().value); advance(); 
                } while (match(TokenType::COMMA));
            }
            match(TokenType::RPAREN);
            return make_shared<FuncExpr>(params, parseStatement(), hasRest);
        }

        if (peek().type == TokenType::IDENT && peek(1).type == TokenType::ARROW) {
            string param = peek().value; advance(); advance(); 
            return make_shared<FuncExpr>(vector<string>{param}, parseArrowBody(), false);
        }

        if (isArrowFunctionAhead()) {
            advance(); 
            vector<string> params; bool hasRest = false;
            if (peek().type != TokenType::RPAREN) {
                do {
                    if (match(TokenType::SPREAD)) {
                        params.push_back(peek().value); advance();
                        hasRest = true; break;
                    }
                    params.push_back(peek().value); advance();
                } while (match(TokenType::COMMA));
            }
            match(TokenType::RPAREN); match(TokenType::ARROW);
            return make_shared<FuncExpr>(params, parseArrowBody(), hasRest);
        }

        if (peek().type == TokenType::NUM) {
            string val = peek().value; advance();
            try { expr = make_shared<LiteralExpr>(JSValue(stod(val))); } catch (...) { throw runtime_error("SyntaxError: Invalid number"); }
        }
        else if (peek().type == TokenType::STR) {
            string val = peek().value; advance();
            expr = make_shared<LiteralExpr>(JSValue(val));
        }
        else if (match(TokenType::LBRACKET)) {
            vector<pair<bool, shared_ptr<Expr>>> elements;
            if (peek().type != TokenType::RBRACKET) {
                do { 
                    bool isSpread = match(TokenType::SPREAD);
                    elements.push_back({isSpread, parseExpression()}); 
                } while (match(TokenType::COMMA));
            }
            match(TokenType::RBRACKET);
            expr = make_shared<ArrayLiteralExpr>(elements);
        }
        else if (match(TokenType::LBRACE)) {
            vector<pair<string, shared_ptr<Expr>>> properties;
            if (peek().type != TokenType::RBRACE) {
                do {
                    string key = peek().value; advance();
                    if (match(TokenType::ASSIGN) || match(TokenType::COLON));
                    properties.push_back({key, parseExpression()});
                } while (match(TokenType::COMMA));
            }
            match(TokenType::RBRACE);
            expr = make_shared<ObjectLiteralExpr>(properties);
        }
        else if (peek().type == TokenType::IDENT) {
            string name = peek().value; advance();
            if (name == "true") expr = make_shared<LiteralExpr>(JSValue(true));
            else if (name == "false") expr = make_shared<LiteralExpr>(JSValue(false));
            else if (name == "undefined" || name == "null") expr = make_shared<LiteralExpr>(JSValue());
            else expr = make_shared<VarExpr>(name);
        }
        else if (match(TokenType::LPAREN)) {
            expr = parseExpression();
            match(TokenType::RPAREN);
        }
        else { return nullptr; }

        while (true) {
            if (match(TokenType::LBRACKET)) {
                auto index = parseExpression(); match(TokenType::RBRACKET);
                expr = make_shared<ArrayAccessExpr>(expr, index);
            }
            else if (match(TokenType::DOT)) {
                string prop = peek().value; advance();
                expr = make_shared<MemberAccessExpr>(expr, prop);
            }
            else if (match(TokenType::LPAREN)) {
                vector<pair<bool, shared_ptr<Expr>>> args;
                if (peek().type != TokenType::RPAREN) {
                    do { 
                        bool isSpread = match(TokenType::SPREAD);
                        args.push_back({isSpread, parseExpression()}); 
                    } while (match(TokenType::COMMA));
                }
                match(TokenType::RPAREN);
                expr = make_shared<CallExpr>(expr, args);
            }
            else { break; }
        }

        TokenType opType = peek().type;
        if (match(TokenType::ASSIGN) || match(TokenType::PLUS_ASSIGN) || match(TokenType::MINUS_ASSIGN) || match(TokenType::MUL_ASSIGN) || match(TokenType::DIV_ASSIGN)) {
            if (auto varExpr = dynamic_cast<VarExpr *>(expr.get())) {
                expr = make_shared<AssignExpr>(varExpr->name, parseExpression(), opType);
            } else if (auto arrAcc = dynamic_cast<ArrayAccessExpr *>(expr.get())) {
                expr = make_shared<ArrayAssignExpr>(arrAcc->arrayExpr, arrAcc->indexExpr, parseExpression(), opType);
            } else if (auto memAcc = dynamic_cast<MemberAccessExpr *>(expr.get())) {
                expr = make_shared<MemberAssignExpr>(memAcc->object, memAcc->property, parseExpression(), opType);
            }
        }

        if (match(TokenType::INC)) {
            if (auto varExpr = dynamic_cast<VarExpr *>(expr.get())) {
                auto add = make_shared<BinaryExpr>(make_shared<VarExpr>(varExpr->name), TokenType::PLUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<AssignExpr>(varExpr->name, add, TokenType::ASSIGN);
            } else if (auto arrAcc = dynamic_cast<ArrayAccessExpr *>(expr.get())) {
                auto add = make_shared<BinaryExpr>(expr, TokenType::PLUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<ArrayAssignExpr>(arrAcc->arrayExpr, arrAcc->indexExpr, add, TokenType::ASSIGN);
            } else if (auto memAcc = dynamic_cast<MemberAccessExpr *>(expr.get())) {
                auto add = make_shared<BinaryExpr>(expr, TokenType::PLUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<MemberAssignExpr>(memAcc->object, memAcc->property, add, TokenType::ASSIGN);
            }
        }
        if (match(TokenType::DEC)) {
            if (auto varExpr = dynamic_cast<VarExpr *>(expr.get())) {
                auto sub = make_shared<BinaryExpr>(make_shared<VarExpr>(varExpr->name), TokenType::MINUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<AssignExpr>(varExpr->name, sub, TokenType::ASSIGN);
            } else if (auto arrAcc = dynamic_cast<ArrayAccessExpr *>(expr.get())) {
                auto add = make_shared<BinaryExpr>(expr, TokenType::MINUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<ArrayAssignExpr>(arrAcc->arrayExpr, arrAcc->indexExpr, add, TokenType::ASSIGN);
            } else if (auto memAcc = dynamic_cast<MemberAccessExpr *>(expr.get())) {
                auto add = make_shared<BinaryExpr>(expr, TokenType::MINUS, make_shared<LiteralExpr>(JSValue(1.0)));
                expr = make_shared<MemberAssignExpr>(memAcc->object, memAcc->property, add, TokenType::ASSIGN);
            }
        }

        return expr;
    }

    shared_ptr<Expr> parseMultiplicative() {
        auto left = parsePrimary();
        while (peek().type == TokenType::MUL || peek().type == TokenType::DIV || peek().type == TokenType::MOD) {
            TokenType op = peek().type; advance();
            left = make_shared<BinaryExpr>(left, op, parsePrimary());
        }
        return left;
    }

    shared_ptr<Expr> parseAdditive() {
        auto left = parseMultiplicative();
        while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
            TokenType op = peek().type; advance();
            left = make_shared<BinaryExpr>(left, op, parseMultiplicative());
        }
        return left;
    }

    shared_ptr<Expr> parseComparison() {
        auto left = parseAdditive();
        while (peek().type == TokenType::LT || peek().type == TokenType::GT ||
               peek().type == TokenType::LTE || peek().type == TokenType::GTE ||
               peek().type == TokenType::EQ || peek().type == TokenType::NEQ) {
            TokenType op = peek().type; advance();
            left = make_shared<BinaryExpr>(left, op, parseAdditive());
        }
        return left;
    }

    shared_ptr<Expr> parseLogical() {
        auto left = parseComparison();
        while (peek().type == TokenType::AND || peek().type == TokenType::OR) {
            TokenType op = peek().type; advance();
            left = make_shared<BinaryExpr>(left, op, parseComparison());
        }
        return left;
    }

    shared_ptr<Expr> parseExpression() { 
        auto e = parseLogical(); 
        if (!e) throw runtime_error("SyntaxError: Unexpected expression token");
        return e;
    }

    shared_ptr<Stmt> parseStatement() {
        if (match(TokenType::LBRACE)) {
            auto block = make_shared<BlockStmt>();
            while (peek().type != TokenType::RBRACE && peek().type != TokenType::EOF_TOK) {
                block->statements.push_back(parseStatement());
            }
            match(TokenType::RBRACE);
            return block;
        }
        if (match(TokenType::FUNCTION)) {
            string name = peek().value; advance();
            match(TokenType::LPAREN);
            vector<string> params; bool hasRest = false;
            if (peek().type != TokenType::RPAREN) {
                do { 
                    if (match(TokenType::SPREAD)) {
                        params.push_back(peek().value); advance();
                        hasRest = true; break;
                    }
                    params.push_back(peek().value); advance(); 
                } while (match(TokenType::COMMA));
            }
            match(TokenType::RPAREN);
            return make_shared<FuncDeclStmt>(name, params, parseStatement(), hasRest);
        }
        if (match(TokenType::LET)) {
            string name = peek().value; advance();
            shared_ptr<Expr> init = nullptr;
            if (match(TokenType::ASSIGN)) init = parseExpression();
            match(TokenType::SEMICOLON);
            return make_shared<LetStmt>(name, init);
        }
        if (match(TokenType::IF)) {
            match(TokenType::LPAREN); auto cond = parseExpression(); match(TokenType::RPAREN);
            auto thenBranch = parseStatement();
            shared_ptr<Stmt> elseBranch = nullptr;
            if (match(TokenType::ELSE)) elseBranch = parseStatement();
            return make_shared<IfStmt>(cond, thenBranch, elseBranch);
        }
        if (match(TokenType::FOR)) {
            match(TokenType::LPAREN);
            shared_ptr<Stmt> init = nullptr;
            if (peek().type == TokenType::LET) init = parseStatement();
            else { init = make_shared<ExprStmt>(parseExpression()); match(TokenType::SEMICOLON); }

            shared_ptr<Expr> cond = nullptr;
            if (peek().type != TokenType::SEMICOLON) cond = parseLogical();
            match(TokenType::SEMICOLON);

            shared_ptr<Expr> update = nullptr;
            if (peek().type != TokenType::RPAREN) update = parseLogical();
            match(TokenType::RPAREN);

            return make_shared<ForStmt>(init, cond, update, parseStatement());
        }
        if (match(TokenType::WHILE)) {
            match(TokenType::LPAREN); auto cond = parseExpression(); match(TokenType::RPAREN);
            return make_shared<WhileStmt>(cond, parseStatement());
        }
        if (match(TokenType::RETURN)) {
            shared_ptr<Expr> val = nullptr;
            if (peek().type != TokenType::SEMICOLON) val = parseExpression();
            match(TokenType::SEMICOLON);
            return make_shared<ReturnStmt>(val);
        }
        if (match(TokenType::BREAK)) {
            match(TokenType::SEMICOLON);
            return make_shared<BreakStmt>();
        }
        if (match(TokenType::CONTINUE)) {
            match(TokenType::SEMICOLON);
            return make_shared<ContinueStmt>();
        }

        auto expr = parseLogical();
        if (expr) {
            auto exprStmt = make_shared<ExprStmt>(expr);
            match(TokenType::SEMICOLON);
            return exprStmt;
        }
        advance(); return nullptr;
    }

    vector<shared_ptr<Stmt>> parse() {
        vector<shared_ptr<Stmt>> stmts;
        while (peek().type != TokenType::EOF_TOK) {
            auto stmt = parseStatement();
            if (stmt) stmts.push_back(stmt);
        }
        return stmts;
    }
};

int main() {
    srand(time(NULL));
    string src = ""; string line;

    while (getline(cin, line)) { src += line + "\n"; }
    if (src.empty()) return 0;

    try {
        Parser parser(src);
        auto ast = parser.parse();
        auto globalEnv = make_shared<Environment>();

        for (auto &stmt : ast) {
            if (stmt) stmt->execute(globalEnv);
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
    }
    return 0;
}