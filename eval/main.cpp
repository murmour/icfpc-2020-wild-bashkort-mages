#include <cstdio>
#include <iostream>
#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <sstream>
#include <cstring>
#include <inttypes.h>
#include <array>
#include <stdlib.h>
#include <cstdarg>
#include <set>
#include <regex>
#include <chrono>
#include "botapi.h"
#include "bot.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(linker, "/STACK:96000000")
#pragma warning( disable : 4996 ) // disable deprecated warning
#endif // _WIN32

#ifdef HTTPLIB
#include "httplib.h"
#endif

auto start_t = chrono::high_resolution_clock::now();

void log_submission(const string s)
{
#ifdef SUBMISSION
	auto end_t = chrono::high_resolution_clock::now();
	double t = chrono::duration_cast<chrono::duration<double>>(end_t - start_t).count();
	fprintf(stderr, "%.3lf: %s\n", t, s.c_str());
#endif
}


using namespace std;

std::string
vformat(const char *fmt, va_list ap)
{
	// Allocate a buffer on the stack that's big enough for us almost
	// all the time.  Be prepared to allocate dynamically if it doesn't fit.
	size_t size = 1024;
	char stackbuf[1024];
	std::vector<char> dynamicbuf;
	char *buf = &stackbuf[0];

	while (1) {
		// Try to vsnprintf into our buffer.
		int needed = vsnprintf(buf, size, fmt, ap);
		// NB. C99 (which modern Linux and OS X follow) says vsnprintf
		// failure returns the length it would have needed.  But older
		// glibc and current Windows return -1 for failure, i.e., not
		// telling us how much was needed.

		if (needed <= (int)size && needed >= 0) {
			// It fit fine so we're done.
			return std::string(buf, (size_t)needed);
		}

		// vsnprintf reported that it wanted to write more characters
		// than we allotted.  So try again using a dynamic buffer.  This
		// doesn't happen very often if we chose our initial size well.
		size = (needed > 0) ? (needed + 1) : (size * 2);
		dynamicbuf.resize(size);
		buf = &dynamicbuf[0];
	}
}

// should compile on linux
std::string
format(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	std::string buf = vformat(fmt, ap);
	va_end(ap);
	return buf;
}

enum NodeType
{
	kAp,
	kInt,
	kTrue,
	kTrue1,
	kFalse,
	kFalse1,
	kVar,
	kAdd,
	kAdd1,
	kMul,
	kMul1,
	kDiv,
	kDiv1,
	kInc,
	kDec,
	kEq,
	kEq1,
	kLt,
	kLt1,
	kNeg,
	kS,
	kS1,
	kS2,
	kC,
	kC1,
	kC2,
	kB,
	kB1,
	kB2,
	kI,
	kCons,
	kCons1,
	kCons2,
	kCar,
	kCdr,
	kNil,
	kIsNil,
	kMultiDraw,
	kCOUNT,
};

bool is_no_arg[kCOUNT] = {};

struct Node
{
	NodeType ty;
	int64_t val = 0;
	Node* arg1 = nullptr;
	Node* arg2 = nullptr;
	Node* evaluated = nullptr;
	int64_t getval()
	{
		if (ty != kInt)
		{
			printf("ty = %d", ty);
			throw ty;
		}
		return val;
	}
	string getstr();
private:
	pair<bool, string> getliststr();
};

constexpr int kBlockSize = 10000;
vector<array<Node, kBlockSize>*> blocks;
int n_used = 0;
constexpr int kSmallNum = 100;
vector<Node*> sliterals, small_nums;

inline Node* alloc_node()
{
	if (blocks.empty() || n_used == kBlockSize)
	{
		blocks.push_back(new array<Node, kBlockSize>());
		n_used = 0;
	}
	auto res = &(*blocks.back())[n_used++];
	return res;
};

inline Node* MakeNode(NodeType ty, Node* arg1 = nullptr, Node *arg2 = nullptr)
{
	if (is_no_arg[ty])
		return sliterals[ty];
	auto res = alloc_node();
	res->ty = ty;
	res->arg1 = arg1;
	res->arg2 = arg2;
	return res;
}

inline Node* MakeNode(int64_t val)
{
	if (abs(val) <= kSmallNum)
		return small_nums[(int)val + kSmallNum];
	auto res = alloc_node();
	res->ty = kInt;
	res->val = val;
	return res;
}

inline Node* MakeNode(NodeType ty, int64_t val)
{
	auto res = alloc_node();
	res->ty = ty;
	res->val = val;
	return res;
}

Node* True;
Node* False;
Node* Nil;

void init_special()
{
	is_no_arg[kTrue] = true;
	is_no_arg[kFalse] = true;
	is_no_arg[kAdd] = true;
	is_no_arg[kMul] = true;
	is_no_arg[kDiv] = true;
	is_no_arg[kInc] = true;
	is_no_arg[kDec] = true;
	is_no_arg[kEq] = true;
	is_no_arg[kLt] = true;
	is_no_arg[kNeg] = true;
	is_no_arg[kS] = true;
	is_no_arg[kC] = true;
	is_no_arg[kB] = true;
	is_no_arg[kI] = true;
	is_no_arg[kCons] = true;
	is_no_arg[kCar] = true;
	is_no_arg[kCdr] = true;
	is_no_arg[kNil] = true;
	is_no_arg[kIsNil] = true;
	// literals
	sliterals.clear();
	for (int i = 0; i < kCOUNT; i++)
	{
		auto t = alloc_node();
		t->ty = (NodeType)i;
		sliterals.push_back(t);
	}
	// small numbers
	small_nums.clear();
	for (int i = -kSmallNum; i <= kSmallNum; i++)
	{
		auto t = alloc_node();
		t->ty = kInt;
		t->val = i;
		small_nums.push_back(t);
	}
	True = MakeNode(kTrue);
	False = MakeNode(kFalse);
	Nil = MakeNode(kNil);
}


inline Node* Ap(Node *f, Node *x)
{
	return MakeNode(kAp, f, x);
}

map<int64_t, Node*> vars;

Node* parse_(const char *&s)
{
	string tok = "";
	while (*s != 0 && *s != ' ' && *s != '\n')
	{
		tok += *s;
		s++;
	}
	if (tok == "ap")
	{
		s++;
		auto a1 = parse_(s);
		s++;
		auto a2 = parse_(s);
		return MakeNode(kAp, a1, a2);
	}
	if (tok == "add")
		return MakeNode(kAdd);
	if (tok == "mul")
		return MakeNode(kMul);
	if (tok == "div")
		return MakeNode(kDiv);
	if (tok == "inc")
		return MakeNode(kInc);
	if (tok == "dec")
		return MakeNode(kDec);
	if (tok == "eq")
		return MakeNode(kEq);
	if (tok == "lt")
		return MakeNode(kLt);
	if (tok == "t")
		return MakeNode(kTrue);
	if (tok == "f")
		return MakeNode(kFalse);
	if (tok == "neg")
		return MakeNode(kNeg);
	if (tok == "s")
		return MakeNode(kS);
	if (tok == "c")
		return MakeNode(kC);
	if (tok == "b")
		return MakeNode(kB);
	if (tok == "i")
		return MakeNode(kI);
	if (tok == "cons" || tok == "vec")
		return MakeNode(kCons);
	if (tok == "car")
		return MakeNode(kCar);
	if (tok == "cdr")
		return MakeNode(kCdr);
	if (tok == "nil")
		return MakeNode(kNil);
	if (tok == "isnil")
		return MakeNode(kIsNil);
	int sign = 1;
	bool var = false;
	int i = 0;
	int64_t x = 0;
	if (tok[0] == '-')
	{
		sign = -1;
		i++;
	}
	else if (tok[0] == ':' || tok[0] == 'x')
	{
		var = true;
		i++;
	}
	while (i < (int)tok.size())
	{
		assert(isdigit(tok[i]));
		x = x * 10 + (tok[i] - '0');
		i++;
	}
	if (var)
		return MakeNode(kVar, x);
	return MakeNode(x * sign);
}

Node* parse_list_(const char *&s)
{
	if (*s == '(')
	{
		s++;
		auto a = parse_list_(s);
		assert(*s == ' ');
		s++;
		auto b = parse_list_(s);
		assert(*s == ')');
		s++;
		return MakeNode(kCons2, a, b);
	}
	if (*s == '[')
	{
		s++;
		vector<Node*> items;
		while (*s != '\n' && *s != 0 && *s != ']')
		{
			if (*s == ' ') s++;
			if (*s == ']') continue;
			auto t = parse_list_(s);
			if (!t)
				return nullptr;
			items.push_back(t);
		}
		reverse(items.begin(), items.end());
		Node *res = Nil;
		for (auto x : items)
			res = MakeNode(kCons2, x, res);
		if (*s != ']')
			return nullptr;
		s++;
		return res;
	}
	string tok = "";
	while (*s != 0 && *s != ' ' && *s != '\n' && *s != ']' && *s != ')')
	{
		tok += *s;
		s++;
	}
	if (tok == "nil")
		return Nil;
	int sign = 1;
	int i = 0;
	int64_t x = 0;
	if (tok[0] == '-')
	{
		sign = -1;
		i++;
	}
	while (i < (int)tok.size())
	{
		if (!isdigit(tok[i]))
			return nullptr;
		x = x * 10 + (tok[i] - '0');
		i++;
	}
	return MakeNode(x * sign);
}

Node* parse(const char *s)
{
	return parse_(s);
}

Node* parse_list(const char *s)
{
	return parse_list_(s);
}

#define RET(x) return ORIG->evaluated = x

Node* eval(Node *v)
{
	Node* ORIG = v;
	if (v->evaluated)
		return v->evaluated;

	if (v->ty == kVar)
	{
		assert(vars.find(v->val) != vars.end());
		RET(eval(vars[v->val]));
	}
	if (v->ty == kCons2)
	{
		auto a1 = eval(v->arg1);
		auto a2 = eval(v->arg2);
		if (a1 != v->arg1 || a2 != v->arg2)
			RET(MakeNode(kCons2, a1, a2));
		RET(v);
	}
	if (v->ty != kAp)
		RET(v);
	auto f = eval(v->arg1);
	switch (f->ty)
	{
	case kInt:
		throw "Ap to Int";
	case kInc:
		RET(MakeNode(eval(v->arg2)->getval() + 1));
	case kDec:
		RET(MakeNode(eval(v->arg2)->getval() - 1));
	case kNeg:
		RET(MakeNode(-eval(v->arg2)->getval()));
	case kAdd:
		RET(MakeNode(kAdd1, v->arg2));
	case kAdd1:
		RET(MakeNode(eval(f->arg1)->getval() + eval(v->arg2)->getval()));
	case kMul:
		RET(MakeNode(kMul1, v->arg2));
	case kMul1:
		RET(MakeNode(eval(f->arg1)->getval() * eval(v->arg2)->getval()));
	case kDiv:
		RET(MakeNode(kDiv1, v->arg2));
	case kDiv1:
		RET(MakeNode(eval(f->arg1)->getval() / eval(v->arg2)->getval()));
	case kEq:
		RET(MakeNode(kEq1, v->arg2));
	case kEq1:
		RET(eval(f->arg1)->getval() == eval(v->arg2)->getval() ? True : False);
	case kLt:
		RET(MakeNode(kLt1, v->arg2));
	case kLt1:
		RET(eval(f->arg1)->getval() < eval(v->arg2)->getval() ? True : False);
	case kTrue:
		RET(MakeNode(kTrue1, v->arg2));
	case kTrue1:
		RET(eval(f->arg1));
	case kFalse:
		RET(MakeNode(kFalse1));
	case kFalse1:
		RET(eval(v->arg2));
	case kI:
		RET(eval(v->arg2));
	case kNil:
		RET(MakeNode(kTrue));
	case kCons:
		RET(MakeNode(kCons1, v->arg2));
	case kCar:
	{
		auto t = eval(v->arg2);
		if (t->ty == kCons2)
			RET(eval(t->arg1));
		RET(eval(Ap(t, True)));
		//assert(t->ty == kCons2);
	}
	case kCdr:
	{
		auto t = eval(v->arg2);
		//assert(t->ty == kCons2);
		if (t->ty == kCons2)
			RET(eval(t->arg2));
		RET(eval(Ap(t, False)));
	}
	case kIsNil:
	{
		auto t = eval(v->arg2);
		if (t->ty == kNil)
			RET(True);
		RET(False);
	}
	case kCons1:
		RET(MakeNode(kCons2, f->arg1, v->arg2));
	case kCons2:
	{
		auto x0 = f->arg1, x1 = f->arg2, x2 = v->arg2;
		RET(eval(Ap(Ap(x2, x0), x1)));
	}

	case kS:
		RET(MakeNode(kS1, v->arg2));
	case kS1:
		RET(MakeNode(kS2, f->arg1, v->arg2));
	case kS2:
	{
		auto x0 = f->arg1, x1 = f->arg2, x2 = v->arg2;
		RET(eval(Ap(Ap(x0, x2), Ap(x1, x2))));
	}

	case kC:
		RET(MakeNode(kC1, v->arg2));
	case kC1:
		RET(MakeNode(kC2, f->arg1, v->arg2));
	case kC2:
	{
		auto x0 = f->arg1, x1 = f->arg2, x2 = v->arg2;
		RET(eval(Ap(Ap(x0, x2), x1)));
	}

	case kB:
		RET(MakeNode(kB1, v->arg2));
	case kB1:
		RET(MakeNode(kB2, f->arg1, v->arg2));
	case kB2:
	{
		auto x0 = f->arg1, x1 = f->arg2, x2 = v->arg2;
		RET(eval(Ap(x0, Ap(x1, x2))));
	}
	case kAp:
	case kVar:
	case kMultiDraw:
	case kCOUNT:
		assert(false);
	}
	assert(false);
	return nullptr;
}

pair<bool, string> Node::getliststr()
{
	if (ty == kNil)
		return { true, "" };
	if (ty == kCons2)
	{
		auto s1 = eval(arg1)->getstr();
		auto[is_list, s2] = eval(arg2)->getliststr();
		if (is_list)
			return { true, s2.empty() ? s1 : s1 + ' ' + s2 };
		return { false, "(" + s1 + " " + s2 + ")" };
	}
	if (ty == kInt)
	{
		char buf[20];
		sprintf(buf, "%" PRId64, val);
		return { false, buf };
	}
	assert(false);
	return { false, "?" };
}

string Node::getstr()
{
	if (!evaluated)
		return eval(this)->getstr();
	if (ty == kMultiDraw)
		return "MultiDraw{" + eval(arg1)->getstr() + "}";
	auto[is_list, s] = getliststr();
	if (is_list)
		return s.empty() ? "nil" : "[" + s + "]";
	return s;
}

Node* n_th(Node *node, int n)
{
	assert(n > 0);
	while (n > 1)
	{
		assert(node->ty == kCons2);
		node = eval(node->arg2);
		n--;
	}
	assert(node->ty == kCons2);
	return node->arg1;
}

Node* modem(Node *node)
{
	if (node->ty == kNil || node->ty == kInt)
		return node;
	if (node->ty == kCons2)
	{
		auto a1 = modem(node->arg1);
		auto a2 = modem(node->arg2);
		if (a1 != node->arg1 || a2 != node->arg2)
			return MakeNode(kCons2, a1, a2);
		return node;
	}
	return modem(eval(node));
}

Node* make_list(vector<Node*> items)
{
	auto res = Nil;
	for (auto x : items)
		res = MakeNode(kCons2, x, res);
	return res;
}

Node* f38(Node *protocol, Node *data);

Node* interact(Node *protocol, Node *state, Node *vector)
{
	auto res = eval(Ap(Ap(protocol, state), vector));
	//printf("Protocol returned %s\n", res->getstr().c_str());
	return f38(protocol, res);
}

string modulate(Node *node)
{
	if (node->ty == kNil)
		return "00";
	if (node->ty == kInt)
	{
		auto x = node->val;
		if (x == 0)
			return "010";
		string pref = "01";
		if (x < 0)
		{
			pref = "10";
			x = -x;
		}
		int k = 0;
		auto t = x;
		while (t > 0)
		{
			k++;
			t /= 16;
		}
		string s = pref + string(k, '1') + "0";
		string s2;
		for (int i = 0; i < 4 * k; i++)
		{
			s2 += x % 2 == 0 ? '0' : '1';
			x /= 2;
		}
		reverse(s2.begin(), s2.end());
		return s + s2;
	}
	if (node->ty == kCons2)
	{
		return "11" + modulate(node->arg1) + modulate(node->arg2);
	}
	assert(false);
	return "?";
}

Node* demodulate_rec(const char *&s)
{
	if ((s[0] == '0' && s[1] == '1') || (s[0] == '1' && s[1] == '0'))
	{
		bool neg = s[0] == '1';
		s += 2;
		int k = 0;
		while (*s != '0') { s++; k++; }
		int n = k * 4;
		s++;
		if (n == 0)
			return MakeNode(0);
		int64_t res = 0;
		for (int i = 0; i < n; i++)
		{
			res = 2 * res + (*s - '0');
			s++;
		}
		if (neg) res = -res;
		return MakeNode(res);
	}
	if (s[0] == '0')
	{
		s += 2;
		return Nil;
	}
	s += 2;
	auto a = demodulate_rec(s);
	auto b = demodulate_rec(s);
	return MakeNode(kCons2, a, b);
}

Node* demodulate(const char *s)
{
	return demodulate_rec(s);
}

const string kAPIKey = "d3efa45ecf1044c4832921884ea217a4";
const string url_suffix = "/aliens/send?apiKey=d3efa45ecf1044c4832921884ea217a4";
string arg_url;

constexpr int alien_buffer_size = 1000000;
char alien_buffer[alien_buffer_size];


#ifndef SUBMISSION
FILE *server_log;
#endif

#ifdef HTTPLIB
Node* send_inner(Node *data)
{
	data = modem(data);
	auto mdata = modulate(data);
	log_submission("Sending:" + data->getstr());

	log_submission("start inner send");
	const std::regex urlRegexp("http://(.+):(\\d+)");
	std::smatch urlMatches;
	if (!std::regex_search(arg_url, urlMatches, urlRegexp) || urlMatches.size() != 3) {
		printf("%s\n", "Unexpected server response:\nBad server URL\n");
#ifndef SUBMISSION
		fprintf(server_log, "%s\n", "Unexpected server response:\nBad server URL\n");
		fflush(server_log);
#endif
		exit(1);
	}
	const std::string serverName = urlMatches[1];
	const int serverPort = std::stoi(urlMatches[2]);
	httplib::Client client(serverName, serverPort);
	string url = arg_url + url_suffix;
	const std::shared_ptr<httplib::Response> serverResponse =
	client.Post(url.c_str(), mdata, "text/plain");

	if (!serverResponse) {
		printf("%s\n", "Unexpected server response:\nNo response from server\n");
#ifndef SUBMISSION
		fprintf(server_log, "%s\n", "Unexpected server response:\nNo response from server\n");
		fflush(server_log);
#endif
		exit(1);
	}

	if (serverResponse->status != 200) {
		printf("Unexpected server response:\nHTTP code: %d\nResponse body: %s\n", serverResponse->status, serverResponse->body.c_str());
#ifndef SUBMISSION
		fprintf(server_log, "Unexpected server response:\nHTTP code: %d\nResponse body: %s\n", serverResponse->status, serverResponse->body.c_str());
		fflush(server_log);
#endif
		exit(2);
	}
	log_submission("end inner send");
	auto res = demodulate(serverResponse->body.c_str());
	log_submission("Got:" + res->getstr());
	return res;
}
#else
Node* send_inner(Node *data)
{
	data = modem(data);
	auto mdata = modulate(data);
	log_submission("Sending:" + data->getstr());
#ifndef HTTPLIB
	auto f = fopen("send.txt", "wt");
	fprintf(f, "%s", mdata.c_str());
	fclose(f);
#endif
#ifndef SUBMISSION
	auto ff = fopen("sendlog.txt", "at");
	fprintf(ff, "%s\n", data->getstr().c_str());
	fclose(ff);
#endif
#ifdef _WIN32
	STARTUPINFOA si;
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	string params = "python.exe alien.py";
	if (CreateProcessA(NULL, &params[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0)
	{
		printf("failed to create process (%d)\n", GetLastError());
		exit(13);
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#else
	string url = arg_url + url_suffix;
	const int code = system(format("./alien '%s'", url.c_str()).c_str());
	if (code != 0) {
		printf("failed to create process (%d)\n", code);
		exit(13);
	}
#endif
	auto resp = fopen("alien.txt", "rt");
	assert(resp);
	if (!fgets(alien_buffer, alien_buffer_size, resp))
		assert(false);
	fclose(resp);
	auto res = demodulate(alien_buffer);
	log_submission("Got:" + res->getstr());
	return res;
}
#endif

void soft_reset();
bool was_send = false;

Node* send(Node *data)
{
	log_submission("begin send");
	was_send = true;
#ifndef SUBMISSION
	fprintf(server_log, "TimeStamp: %d\n", (int)time(nullptr));
	fprintf(server_log, "Input:\n%s\n", data->getstr().c_str());
	fflush(server_log);
#endif
	auto res = send_inner(data);
#ifndef SUBMISSION
	fprintf(server_log, "Output:\n%s\n", res->getstr().c_str());
	fflush(server_log);
#endif
	log_submission("end send");
	for (auto b : blocks)
		for (auto &node : *b)
			node.evaluated = nullptr;
	log_submission("cache cleared");
	return res;
}

Node* f38(Node *protocol, Node *args)
{
	Node* flag = n_th(args, 1);
	Node* new_state = n_th(args, 2);
	Node* data = n_th(args, 3);
	if (flag->getval() == 0)
		return make_list({ modem(new_state), MakeNode(kMultiDraw, data) });
	return interact(protocol, modem(new_state), send(data));
}

constexpr int buf_size = 200000;
char buf[buf_size];

void load_vars(const char *fname)
{
	auto f = fopen(fname, "rt");
	assert(f);
	while (fgets(buf, buf_size, f))
	{
        if (memcmp(buf, "galaxy = ", 9) == 0) {
            vars[0] = parse(&buf[9]);
            continue;
        }
		int var = 0;
		int i;
		for (i = 0; buf[i] != '=' && buf[i] != 0; i++)
		{
			if (buf[i] == ' ' || buf[i] == ':') continue;
			var = 10 * var + buf[i] - '0';
		}
		if (buf[i] != '=') continue;
		vars[var] = parse(&buf[i + 2]);
	}
	fclose(f);
}


void test_proto()
{
	// stateless draw
	auto protocol = parse("ap ap c ap ap b b ap ap b ap b ap cons 0 ap ap c ap ap b b cons ap ap c cons nil ap ap c ap ap b cons ap ap c cons nil nil");

	// stateful draw
	//auto protocol = parse("ap ap b ap b ap ap s ap ap b ap b ap cons 0 ap ap c ap ap b b cons ap ap c cons nil ap ap c cons nil ap c cons");

	auto state = parse("nil");
	auto vector = parse("ap ap vec 0 0");
	auto res = interact(protocol, state, vector);
	cout << "got " << res->getstr() << endl;

	state = n_th(res, 2);
	auto vector2 = parse("ap ap vec 7 7");
	auto res2 = interact(protocol, state, vector2);
	cout << "got " << res2->getstr() << endl;
}

vector<Node*> extract_list(Node *n)
{
	vector<Node*> res;
	while (n->ty != kNil)
	{
		assert(n->ty == kCons2);
		res.push_back(n->arg1);
		n = n->arg2;
	}
	return res;
}

pair<int, int> extract_point(Node *n)
{
	assert(n->ty == kCons2);
	return { (int)n->arg1->getval(), (int)n->arg2->getval() };
}

void draw_image(FILE *f, Node *node)
{
	int minx = numeric_limits<int>::max();
	int miny = minx;
	int maxx = numeric_limits<int>::min();
	int maxy = maxx;
	vector<pair<int, int>> pts;
	for (auto p : extract_list(node)) pts.push_back(extract_point(p));
	for (auto [x, y] : pts)
	{
		minx = min(minx, x);
		miny = min(miny, y);
		maxx = max(maxx, x);
		maxy = max(maxy, y);
	}
	if (minx > maxx) return;
	int n = maxy - miny + 1;
	int m = maxx - minx + 1;
	vector<vector<char>> mat(n, vector<char>(m+1, '.'));
	for (int i = 0; i < n; i++) mat[i][m] = 0;
	for (int i = 0; i < m; i++) fprintf(f, "%c", '-');
	fprintf(f, "@(%d, %d)\n", minx, miny);
	for (auto[x, y] : pts)
		mat[y - miny][x - minx] = '#';
	for (auto &v : mat)
		fprintf(f, "%s\n", &v[0]);
}

const int buf_len = 1000000;
char state_buf[buf_len];

template<typename T>
bool operator << (const T &t, const std::set<T> &s)
{
	return s.find(t) != s.end();
}

const int dx[] = { -1, -1, -1,  0,  0,  1,  1,  1 };
const int dy[] = { -1,  0,  1, -1,  1, -1,  0,  1 };
const int inf = 100000000;

struct Image
{
	typedef pair<int, int> P;

	bool get(int x, int y) const
	{
		return pts.find({ x, y }) != pts.end();
	}
	bool get(const Point &p) const
	{
		return get(p.x, p.y);
	}
	int width() const
	{
		return max(0, x1 - x0 + 1);
	}
	int height() const
	{
		return max(0, y1 - y0 + 1);
	}
	Point top_left() const
	{
		return { x0, y0 };
	}
	Point bottom_right() const
	{
		return { x1, y1 };
	}
	Point get_center() const
	{
		return { (x0 + x1) / 2, (y0 + y1) / 2 };
	}

	static Image FromFile(const char *fname)
	{
		auto f = fopen(fname, "rt");
		assert(f);
		vector<string> strs;
		while (fgets(state_buf, buf_len, f))
			if (state_buf[0])
				strs.push_back(state_buf);
		string hdr = strs[0];
		int p0 = hdr.find('(');
		int p1 = hdr.find(')');
		string nums = hdr.substr(p0 + 1, p1 - p0 - 1);
		for (char &c : nums) if (c == ',') c = ' ';
		stringstream ss(nums);
		int x, y;
		ss >> x >> y;
		vector<P> pts;
		for (int i = 1; i < (int)strs.size(); i++)
		{
			auto &s = strs[i];
			for (int j = 0; j < (int)s.size(); j++)
				if (s[j] == '#')
					pts.push_back({ x + j, y + i - 1 });
		}
		fclose(f);
		return Image(pts);
	}

	string ToString(Point c = { 1000, 1000 }) const
	{
		int n = height();
		int m = width();
		vector<string> mat(n, string(m, '.'));
		for (int i = 0; i < n; i++) mat[i][m] = 0;
		for (auto[x, y] : pts)
			mat[y - y0][x - x0] = '#';
		if (x0 <= c.x && c.x <= x1 && y0 <= c.y && c.y <= y1)
		{
			char &q = mat[c.y - y0][c.x - x0];
			if (q == '#') q = '*'; else q = '+';
		}
		string res;
		for (int i = 0; i < n; i++)
			res += mat[i] + "\n";
		return res;
	}

	Image(const vector<P> &points)
	{
		x0 = inf;
		x1 = -inf;
		y0 = inf;
		y1 = -inf;
		for (auto p : points)
		{
			pts.insert(p);
			x0 = min(x0, p.first);
			x1 = max(x1, p.first);
			y0 = min(y0, p.second);
			y1 = max(y1, p.second);
		}
	}

	vector<Image> get_subimages() const
	{
		vector<P> cur;
		set<P> seen;
		vector<Image> res;
		for (auto p : pts) if (!(p << seen))
		{
			cur.clear();
			dfs(p, seen, cur);
			res.push_back(cur);
		}
		return res;
	}

	bool is_galaxy() const
	{
		if (!get(-1, -1)) return false;
		if (!get(-1, 0)) return false;
		if (get(-1, 1)) return false;
		if (!get(0, -1)) return false;
		if (get(0, 0)) return false;
		if (!get(0, 1)) return false;
		if (get(1, -1)) return false;
		if (!get(1, 0)) return false;
		if (!get(1, 1)) return false;
		return true;
	}
private:

	void dfs(P p, set<P> &seen, vector<P> &pts) const
	{
		if (p << seen) return;
		pts.push_back(p);
		seen.insert(p);
		for (int dir = 0; dir < 8; dir++)
		{
			auto t = make_pair(p.first + dx[dir], p.second + dy[dir]);
			if (get(t.first, t.second))
				dfs(t, seen, pts);
		}
	}
	set<pair<int, int>> pts;
	int x0, x1, y0, y1;
};

// sz includes border
bool check_number(const Image &img, Point p, int sz)
{
	if (img.get(p)) return false;
	for (int i = 1; i < sz; i++)
		if (!img.get({ p.x + i, p.y }) || !img.get({ p.x, p.y + i }))
			return false;
	return true;
}

pair<int, int> get_number(const Image &img, Point p)
{
	if (img.get(p.x, p.y))
		return { 0, 0 };
	int sz = 0;
	while (img.get(p.x + sz + 1, p.y)) sz++;
	bool neg = img.get(p.x, p.y + sz + 1);
	int res = 0, t = 1;
	for (int i = 0; i < sz; i++)
		for (int j = 0; j < sz; j++)
		{
			if (img.get(p.x + j + 1, p.y + i + 1)) res += t;
			t *= 2;
		}
	if (neg) res = -res;
	if (!check_number(img, p, sz + 1))
		return { 0, 0 };
	return { res, sz + 1 };
}

pair<int, int> get_number_rt(const Image &img, Point p)
{
	while (img.get(p.x, p.y)) p.x--;
	return get_number(img, p);
}

// max_size includes border
pair<int, int> get_number_rb(const Image &img, Point p, int max_size)
{
	for (int sz = max_size; sz >= 2; sz--)
		if (check_number(img, { p.x - sz + 1, p.y - sz + 1 }, sz))
			return get_number(img, { p.x - sz + 1, p.y - sz + 1 });
	return { 0, 0 };
}

struct Rect
{
	Point topLeft, bottomRight;
};

const pair<int, int> bad_num = { 0, 0 };

Ship bad_ship(const Image &img, Point c)
{
	log_submission(format("bad ship at %d, %d (img %d, %d)", c.x, c.y, img.top_left().x, img.top_left().y));
	log_submission("\n");
	log_submission(img.ToString(c));
	Ship res;
	res.id = -1;
	return res;
}

Ship get_ship_params(const Image &img, Point c)
{
	// refine center
	int best = 1000;
	Point newc = c;
	for (auto &sub : img.get_subimages())
	{
		auto sc = sub.get_center();
		int d = max(abs(sc.x - c.x), abs(sc.y - c.y));
		if (d < best)
		{
			best = d;
			newc = sc;
		}
	}
	c = newc;

	Ship res;
	res.pos = c;
	Point t = c;
	t.y -= 2;
	t.x += 5;
	auto fuel = get_number(img, t);
	if (fuel == bad_num) return bad_ship(img, c);
	res.fuel = fuel.first;
	t.x += fuel.second + 2;
	auto laser = get_number(img, t);
	if (laser == bad_num) return bad_ship(img, c);
	res.laser = laser.first;
	t.x += laser.second + 2;
	auto cooler = get_number(img, t);
	if (laser == bad_num) return bad_ship(img, c);
	res.cooler = cooler.first;
	t.x += cooler.second + 2;
	if (cooler == bad_num) return bad_ship(img, c);
	auto cores = get_number(img, t);
	res.cores = cores.first;
	if (cores == bad_num) return bad_ship(img, c);
	auto temperature = get_number_rt(img, { c.x - 5, c.y + 5 });
	if (temperature == bad_num) return bad_ship(img, c);
	res.temperature = temperature.first;
	auto id = get_number_rb(img, { c.x - 5, c.y - 5 }, 5);
	if (id == bad_num) return bad_ship(img, c);
	res.id = id.first;
	res.is_owned = img.get(c.x - 5, c.y);
	return res;
}

vector<Point> get_ships(const Image &img)
{
	vector<Point> res;
	for (auto &img : img.get_subimages())
		res.push_back(img.get_center());
	return res;
}

void load_stuff();
Node* protocol;

bool is_defender;

struct Computation
{
	vector<vector<pair<int, int>>> images;
	Node* state;

	Computation(const string &init_state)
	{
		load_stuff();
		state = parse_list(init_state.c_str());
		assert(state);
	}

	void set_state(const string &s)
	{
		state = parse_list(s.c_str());
		assert(state);
	}

	string get_state() const
	{
		return state->getstr();
	}

	void update(Point p)
	{
		auto t = state->getstr();
		soft_reset();
		state = parse_list(t.c_str());
		assert(state);
		auto v = parse(format("ap ap vec %d %d", p.x, p.y).c_str());
		auto res = interact(protocol, state, v);
		auto imgs = modem(n_th(res, 1)->arg1);
		images.clear();
		int n_image = 0;
		for (auto img : extract_list(imgs))
		{
#ifndef SUBMISSION
			auto ifname = format("image_%d.log", n_image);
			auto fimg = fopen(ifname.c_str(), "wt");
			draw_image(fimg, img);
			fclose(fimg);
#endif
			n_image++;
			vector<pair<int, int>> pts;
			for (auto p : extract_list(img))
				pts.push_back(extract_point(p));
			images.push_back(pts);
		}
		state = n_th(res, 2);
	}

	vector<Image> getImages() const
	{
		vector<Image> res;
		for (auto &v : images) res.push_back(v);
		return res;
	}
};

GameState get_full_game_state(Computation *comp)
{
	GameState state;
	auto images = comp->getImages();
	//assert(images[0].is_galaxy());
	if (!images[0].is_galaxy())
		return state;
	if (images.size() < 6)
		return state;
	state.ticks_left = get_number(images[0], { 6, -3 }).first;
	vector<Point> defenders;
	for (auto p : get_ships(images[0]))
		if (abs(p.x) > 10 || abs(p.y) > 10)
			defenders.push_back(p);
	auto attackers = get_ships(images[2]);
	auto &planet = images[4];
	state.planet_size.x = planet.width();
	state.planet_size.y = planet.height();
	auto &arena = images[5];
	state.arena_top_left = arena.top_left();
	state.arena_bottom_right = arena.bottom_right();

	vector<Ship> def_ships, att_ships;
	for (auto p : defenders)
	{
		comp->update(p);
		if (comp->images.size() != 7)
			continue;
		auto img = Image(comp->images[0]);
		img.ToString();
		auto params = get_ship_params(img, p);
		if (params.id != -1)
			def_ships.push_back(params);
		comp->update(p);
	}
	for (auto p : attackers)
	{
		comp->update(p);
		if (comp->images.size() != 7)
			continue;
		auto img = Image(comp->images[0]);
		auto params = get_ship_params(img, p);
		if (params.id != -1)
			att_ships.push_back(params);
		while (true)
		{
			comp->update(p);
			if (Image(comp->images[0]).is_galaxy())
				break;
		}
	}
	state.role = is_defender ? kDefender : kAttacker;
	if (def_ships[0].is_owned)
	{
		state.my_ships = def_ships;
		state.enemy_ships = att_ships;
	}
	else
	{
		state.my_ships = att_ships;
		state.enemy_ships = def_ships;
	}
	log_submission(format("extracted state (%d, %d)", (int)state.my_ships.size(), (int)state.enemy_ships.size()));
	return state;
}

void SendCommands(Computation *comp, const vector<Ship> &ships, const vector<Command> &cmds)
{
	assert(ships.size() == cmds.size());
	assert(Image(comp->images[0]).is_galaxy());
	for (int i = 0; i < (int)ships.size(); i++)
	{
		auto &ship = ships[i];
		auto &cmd = cmds[i];
		if (cmd.explode)
		{
			comp->update(ship.pos);
			comp->update({ ship.pos.x - 6, ship.pos.y });
		}
		if (abs(cmd.acceleration.x) + abs(cmd.acceleration.y) > 0)
		{
			if (ship.fuel <= 0)
			{
				log_submission("WARNING: trying to move with no fuel");
			}
			else
			{
				comp->update(ship.pos);
				Point diode = { ship.pos.x, ship.pos.y - 7 };
				comp->update(diode);
				comp->update({ diode.x - cmd.acceleration.x * 5, diode.y - cmd.acceleration.y * 5 });
			}
		}
		if (cmd.shoot)
		{
			if (ship.laser <= 0)
			{
				log_submission("WARNING: trying to shoot with no laser");
			}
			else
			{
				comp->update(ship.pos);
				Point shooter = { ship.pos.x, ship.pos.y + 7 };
				comp->update(shooter);
				comp->update(cmd.shoot_target);
			}
		}
	}
}

GameState MakeAMove(Computation *comp, bot_func bot, const GameState &old)
{
	auto state = get_full_game_state(comp);
	state.ticks_elapsed = old.ticks_elapsed + 1;
	if (state.role == kUndefined)
		return state;
	auto commands = bot(old, state);
	log_submission("sending commands");
	SendCommands(comp, state.my_ships, commands);
	comp->update({ 0, 0 });
	log_submission("made a move");
	return state;
}

vector<Command> empty_bot(const GameState &old_state, const GameState &state)
{
	vector<Command> res(state.my_ships.size());
	for (auto &c : res)
		c.acceleration.x = 1;
	return res;
}

void replaceAll(string &s, const string &search, const string &replace) {
	for (size_t pos = 0; ; pos += replace.length()) {
		// Locate the substring to replace
		pos = s.find(search, pos);
		if (pos == string::npos) break;
		// Replace by erasing and inserting
		s.erase(pos, search.length());
		s.insert(pos, replace);
	}
}

void PlayGame(const string &url, const string &key)
{
#ifndef SUBMISSION
#ifndef HTTPLIB
	auto f = fopen("url.txt", "wt");
	fprintf(f, "%s", (url + url_suffix).c_str());
	fclose(f);
#endif
#endif

	// join the game
	const char *cheats = "[103652820 192496425430]";
	auto state = format("[5 [4 %s nil nil nil nil (36 0) 666] 9 %s]", key.c_str(), cheats);
	Computation comp(state);
	comp.update({ 27, 0 });
	log_submission("joined game");

	auto ns = comp.get_state();
	if (ns.find("[446 0 0 1]") != string::npos)
	{
		// defender
		replaceAll(ns, "[446 0 0 1]", "[62 0 32 1]");
		is_defender = true;
	}
	else
	{
		// attacker
		replaceAll(ns, "[510 0 0 1]", "[126 48 16 1]");
		is_defender = false;
	}
	comp.set_state(ns);
	// try defender
	if (is_defender)
		comp.update({ 75, -20 });
	else
		comp.update({ 75, 20 });
	log_submission("started game");
	GameState s;
	while (true)
	{
		// auto new_st = MakeAMove(&comp, bot_func_combine, s);
		// auto new_st = MakeAMove(&comp, bot_func_kamikaze, s);
		GameState new_st;
		new_st = MakeAMove(&comp, bot_func_combine3, s);
		// if (is_defender) {
		// 	new_st = MakeAMove(&comp, bot_func_combine3, s);
		// } else {
		// 	new_st = MakeAMove(&comp, bot_func_combine, s);
		// }
		if (new_st.ticks_left <= 1)
			break;
		s = new_st;
	};
	log_submission("game over");
}

int n_init_blocks;
int n_init_used;

void load_stuff()
{
	for (auto b : blocks)
		delete b;
	n_used = 0;
	blocks.clear();
	vars.clear();

	init_special();
	load_vars("../galaxy.txt");
	n_init_blocks = (int)blocks.size();
	n_init_used = n_used;
	protocol = vars[0];
	log_submission("stuff loaded");
}

void soft_reset()
{
	if (!was_send)
		return;
	log_submission("soft reset");
	was_send = false;
	for (int i = n_init_blocks; i < (int)blocks.size(); i++)
		delete blocks[i];
	blocks.resize(n_init_blocks);
	n_used = n_init_used;
	for (auto b : blocks)
		for (auto &node : *b)
			node.evaluated = nullptr;
}

int main(int argc, char** argv)
{
#ifndef SUBMISSION
	char fname[1000];
	sprintf(fname, "server_%d.log", (int)time(nullptr));
	server_log = fopen(fname, "wt");
#endif

	if (argc > 2)
	{
		auto url = string(argv[1]);
		arg_url = url;
		auto key = string(argv[2]);
		log_submission(format("Url is %s", url.c_str()));
		log_submission(format("Key is %s", key.c_str()));
		PlayGame(url, key);
		return 0;
	}
	arg_url = "https://icfpc2020-api.testkontur.ru:443";

#ifndef SUBMISSION
#ifndef HTTPLIB
	{
		auto f = fopen("url.txt", "wt");
		string url = arg_url + url_suffix;
		fprintf(f, "%s", url.c_str());
		fclose(f);
	}
#endif
#endif
	load_stuff();

	bool interactive = true;
	bool py = argc > 1 && string(argv[1]) == "loop";

#ifndef SUBMISSION
	sprintf(fname, "interaction_%d.log", (int)time(nullptr));
	auto log = fopen(fname, "wt");
#endif
	if (py)
	{
		while (true)
		{
			soft_reset();

			do {
				char *c = fgets(state_buf, buf_len, stdin);
				(void)c;
			} while (strlen(state_buf) == 0);
			auto vector = parse_list(state_buf);
			if (!vector) exit(3);

			do {
				char *c = fgets(state_buf, buf_len, stdin);
				(void)c;
			} while (strlen(state_buf) == 0);
			auto state = parse_list(state_buf);
			if (!state) exit(3);

			auto res = interact(protocol, state, vector);
#ifndef SUBMISSION
			auto images = modem(n_th(res, 1)->arg1);
			int n_image = 0;
			for (auto img : extract_list(images))
			{
				draw_image(log, img);
				char ifname[1000];
				sprintf(ifname, "image_%d.log", n_image);
				auto fimg = fopen(ifname, "wt");
				draw_image(fimg, img);
				fclose(fimg);
				n_image++;
			}
			fflush(log);
#endif
			state = n_th(res, 2);
#ifndef SUBMISSION
			printf("%d\n", n_image);
			printf("%s\n", state->getstr().c_str());
			fflush(stdout);
			fflush(log);
#endif
		}
#ifndef SUBMISSION
		fclose(log);
#endif
		return 0;
	}

	//printf("%d blocks\n", (int)blocks.size());
	auto state = parse("nil");
	while (true)
	{
		auto vector = parse("ap ap vec 1 1");
		if (interactive)
		{
			char *c = fgets(state_buf, buf_len, stdin);
			(void)c;
			if (strlen(state_buf) == 0)
				continue;
			if (state_buf[0] == 'S')
			{
				auto t = parse_list(state_buf + 1);
				if (!t)
				{
					printf("Invalid state");
					continue;
				}
				state = t;
				printf("Set state %s\n", state->getstr().c_str());
				continue;
			}
			else
			{
				stringstream ss(state_buf);
				int x, y;
				ss >> x >> y;
				char buf[100];
				sprintf(buf, "ap ap vec %d %d", x, y);
				vector = parse(buf);
			}
		}
#ifndef SUBMISSION
		printf("Vector is %s\n", eval(vector)->getstr().c_str());
		fprintf(log, "Vector is %s\n", eval(vector)->getstr().c_str());
		fflush(log);
#endif
		auto res = interact(protocol, state, vector);
#ifndef SUBMISSION
		//printf("got %s\n", res->getstr().c_str());
		auto images = modem(n_th(res, 1)->arg1);
		int n_image = 0;
		for (auto img : extract_list(images))
		{
			draw_image(stdout, img);
			draw_image(log, img);
			if (interactive)
			{
				char ifname[1000];
				sprintf(ifname, "image_%d.log", n_image);
				auto fimg = fopen(ifname, "wt");
				draw_image(fimg, img);
				fclose(fimg);
			}
			n_image++;
		}
		fflush(log);
#endif
		state = n_th(res, 2);
#ifndef SUBMISSION
		printf("New state is %s\n", state->getstr().c_str());
		fprintf(log, "New state is %s\n", state->getstr().c_str());
		fflush(log);
#endif
	}
#ifndef SUBMISSION
	fclose(log);
#endif
	return 0;
}
