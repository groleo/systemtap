/* $Id$ */
#ifndef _re2c_regex_h
#define _re2c_regex_h

#include <iostream>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <iosfwd>
#include "re2c-globals.h"

namespace re2c
{

// moved here from ins.h

typedef unsigned short Char;

const unsigned CHAR = 0;
const unsigned GOTO = 1;
const unsigned FORK = 2;
const unsigned TERM = 3;
const unsigned CTXT = 4;
const unsigned INIT = 5; // for ^ operator
// TODOXXX const unsigned ENDI = 6; // for $ operator??

union Ins {

	struct
	{
		unsigned char	tag;
		unsigned char	marked;
		void	*link;
	}

	i;

	struct
	{
		unsigned short	value;
		unsigned short	bump;
		void	*link;
	}

	c;
};

inline bool isMarked(Ins *i)
{
	return i->i.marked != 0;
}

inline void mark(Ins *i)
{
	i->i.marked = true;
}

inline void unmark(Ins *i)
{
	i->i.marked = false;
}

// ---------------------------------------------------------------------

// moved from token.h

class Token
{
public:
	const Str          text;
	const std::string  newcond;
	const std::string  source;
	unsigned               line;
	const bool         autogen;

public:
	Token(const SubStr&, const std::string&, unsigned);
	Token(const Token*, const std::string&, unsigned, const Str*);
	Token(const Token& oth);
	~Token();
};

inline Token::Token(const SubStr& t, const std::string& s, unsigned l)
	: text(t)
	, newcond()
	, source(s)
	, line(l)
	, autogen(false)
{
	;
}

inline Token::Token(const Token* t, const std::string& s, unsigned l, const Str *c)
	: text(t ? t->text.to_string().c_str() : "")
	, newcond(c ? c->to_string() : "")
	, source(s)
	, line(l)
	, autogen(t == NULL)
{
	;
}

inline Token::Token(const Token& oth)
	: text(oth.text.to_string().c_str())
	, newcond(oth.newcond)
	, source(oth.source)
	, line(oth.line)
	, autogen(oth.autogen)
{
	;
}

inline Token::~Token()
{
}

// ---------------------------------------------------------------------

template<class _Ty>
class free_list: protected std::set<_Ty>
{
public:
	typedef typename std::set<_Ty>::iterator   iterator;
	typedef typename std::set<_Ty>::size_type  size_type;
	typedef typename std::set<_Ty>::key_type   key_type;
	
	free_list(): in_clear(false)
	{
	}
	
	using std::set<_Ty>::insert;

	size_type erase(const key_type& key)
	{
		if (!in_clear)
		{
			return std::set<_Ty>::erase(key);
		}
		return 0;
	}
	
	void clear()
	{
		in_clear = true;

		for(iterator it = this->begin(); it != this->end(); ++it)
		{
			delete *it;
		}
		std::set<_Ty>::clear();
		
		in_clear = false;
	}

	~free_list()
	{
		clear();
	}

protected:
	bool in_clear;
};

typedef struct extop
{
	char op;
	int	minsize;
	int	maxsize;
}

ExtOp;

struct CharPtn
{
	unsigned	card;
	CharPtn	*fix;
	CharPtn	*nxt;
};

typedef CharPtn *CharPtr;

struct CharSet
{
	CharSet();
	~CharSet();

	CharPtn	*fix;
	CharPtn	*freeHead, **freeTail;
	CharPtr	*rep;
	CharPtn	*ptn;
};

class Range
{

public:
	Range	*next;
	unsigned	lb, ub;		// [lb,ub)

	static free_list<Range*> vFreeList;

public:
	Range(unsigned l, unsigned u) : next(NULL), lb(l), ub(u)
	{
		vFreeList.insert(this);
	}

	Range(Range &r) : next(NULL), lb(r.lb), ub(r.ub)
	{
		vFreeList.insert(this);
	}

	~Range()
	{
		vFreeList.erase(this);
	}

	friend std::ostream& operator<<(std::ostream&, const Range&);
	friend std::ostream& operator<<(std::ostream&, const Range*);
};

inline std::ostream& operator<<(std::ostream &o, const Range *r)
{
	return r ? o << *r : o;
}

class RegExp
{

public:
	unsigned	size;
        bool    anchored; // optimization flag -- always safe to set to false
	
	static free_list<RegExp*> vFreeList;

public:
	RegExp() : size(0), anchored(false)
	{
		vFreeList.insert(this);
	}

	virtual ~RegExp()
	{
		vFreeList.erase(this);
	}

	virtual const char *typeOf() = 0;
	RegExp *isA(const char *t)
	{
		return typeOf() == t ? this : NULL;
	}

	virtual void split(CharSet&) = 0;
	virtual void calcSize(Char*) = 0;
	virtual unsigned fixedLength();
	virtual void compile(Char*, Ins*) = 0;
	virtual void display(std::ostream&) const = 0;
	friend std::ostream& operator<<(std::ostream&, const RegExp&);
	friend std::ostream& operator<<(std::ostream&, const RegExp*);
};

inline std::ostream& operator<<(std::ostream &o, const RegExp &re)
{
	re.display(o);
	return o;
}

inline std::ostream& operator<<(std::ostream &o, const RegExp *re)
{
	return o << *re;
}

class NullOp: public RegExp
{

public:
	static const char *type;

public:
	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	unsigned fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << "_";
	}
};

class MatchOp: public RegExp
{

public:
	static const char *type;
	Range	*match;

public:
	MatchOp(Range *m) : match(m)
	{
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	unsigned fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream&) const;

#ifdef PEDANTIC
private:
	MatchOp(const MatchOp& oth)
		: RegExp(oth)
		, match(oth.match)
	{
	}
	
	MatchOp& operator = (const MatchOp& oth)
	{
		new(this) MatchOp(oth);
		return *this;
	}
#endif
};

class AnchorOp: public RegExp
{

public:
	static const char *type;
        char atype;

public:
        AnchorOp(char a): atype(a) {}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	unsigned fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << atype;
	}
};

class RuleOp: public RegExp
{
public:
	static const char *type;

private:
	RegExp   *exp;

public:
	RegExp   *ctx;
	Ins      *ins;
	unsigned     accept;
	Token    *code;
	unsigned     line;

public:
	RuleOp(RegExp*, RegExp*, Token*, unsigned);

	~RuleOp()
	{
		delete code;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "/" << ctx << ";";
	}
	RuleOp* copy(unsigned) const;

#ifdef PEDANTIC
private:
	RuleOp(const RuleOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
		, ctx(oth.ctx)
		, ins(oth.ins)
		, accept(oth.accept)
		, code(oth.code)
		, line(oth.line)
	{
	}

	RuleOp& operator = (const RuleOp& oth)
	{
		new(this) RuleOp(oth);
		return *this;
	}
#endif
};

RegExp *mkAlt(RegExp*, RegExp*);

class AltOp: public RegExp
{

private:
	RegExp	*exp1, *exp2;

public:
	static const char *type;

public:
	AltOp(RegExp *e1, RegExp *e2)
		: exp1(e1)
		, exp2(e2)
	{
        	// anchored enabled iff e1 enables it
        	anchored = e1->anchored;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	unsigned fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp1 << "|" << exp2;
	}

	friend RegExp *mkAlt(RegExp*, RegExp*);

#ifdef PEDANTIC
private:
	AltOp(const AltOp& oth)
		: RegExp(oth)
		, exp1(oth.exp1)
		, exp2(oth.exp2)
	{
	}
	AltOp& operator = (const AltOp& oth)
	{
		new(this) AltOp(oth);
		return *this;
	}
#endif
};

class CatOp: public RegExp
{

private:
	RegExp	*exp1, *exp2;

public:
	static const char *type;

public:
	CatOp(RegExp *e1, RegExp *e2)
		: exp1(e1)
		, exp2(e2)
	{
        	// anchored enabled only when both alternatives enable it
        	anchored = e1->anchored && e2->anchored;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	unsigned fixedLength();
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp1 << exp2;
	}

#ifdef PEDANTIC
private:
	CatOp(const CatOp& oth)
		: RegExp(oth)
		, exp1(oth.exp1)
		, exp2(oth.exp2)
	{
	}
	CatOp& operator = (const CatOp& oth)
	{
		new(this) CatOp(oth);
		return *this;
	}
#endif
};

class CloseOp: public RegExp
{

private:
	RegExp	*exp;

public:
	static const char *type;

public:
	CloseOp(RegExp *e)
		: exp(e)
	{
        	// anchored enabled iff e enables it
        	anchored = e->anchored;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "+";
	}

#ifdef PEDANTIC
private:
	CloseOp(const CloseOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
	{
	}
	CloseOp& operator = (const CloseOp& oth)
	{
		new(this) CloseOp(oth);
		return *this;
	}
#endif
};

class CloseVOp: public RegExp
{

private:
	RegExp	*exp;
	int	min;
	int	max;

public:
	static const char *type;

public:
	CloseVOp(RegExp *e, int lb, int ub)
		: exp(e)
		, min(lb)
		, max(ub)
	{
        	// anchored enabled iff we *have* to match an anchored e
        	anchored = e->anchored && lb > 0;
	}

	const char *typeOf()
	{
		return type;
	}

	void split(CharSet&);
	void calcSize(Char*);
	void compile(Char*, Ins*);
	void display(std::ostream &o) const
	{
		o << exp << "+";
	}
#ifdef PEDANTIC
private:
	CloseVOp(const CloseVOp& oth)
		: RegExp(oth)
		, exp(oth.exp)
		, min(oth.min)
		, max(oth.max)
	{
	}
	CloseVOp& operator = (const CloseVOp& oth)
	{
		new(this) CloseVOp(oth);
		return *this;
	}
#endif
};

typedef std::set<std::string>           CondList;
typedef std::pair<unsigned, RegExp*>    NRegExp;
typedef std::map<std::string, NRegExp>  RegExpMap;
typedef std::vector<std::string>        RegExpIndices;
typedef std::list<RuleOp*>              RuleOpList;
typedef std::pair<unsigned, std::string>    LineCode;
typedef std::map<std::string, LineCode> SetupMap;

class DFA;

extern DFA* genCode(RegExp*);
extern void genGetStateGoto(std::ostream&, unsigned&, unsigned);
extern void genCondTable(std::ostream&, unsigned, const RegExpMap&);
extern void genCondGoto(std::ostream&, unsigned, const RegExpMap&);
extern void genTypes(std::string&, unsigned, const RegExpMap&);
extern void genHeader(std::ostream&, unsigned, const RegExpMap&);

extern RegExp *mkDiff(RegExp*, RegExp*);
extern RegExp *mkAlt(RegExp*, RegExp*);


// ---------------------------------------------------------------------

// moved from scanner.h

struct ScannerState
{
	ScannerState();

	char	*tok, *ptr, *cur, *pos, *ctx;  // positioning
	char    *bot, *lim, *top, *eof;        // buffer
	unsigned	tchar, tline, cline, iscfg, buf_size;
	bool    in_parse;
};

class Scanner: private ScannerState
{
private:
	std::istream&	in;
	std::ostream&   out;

private:
	char *fill(char*, unsigned);
	Scanner(const Scanner&); //unimplemented
	Scanner& operator=(const Scanner&); //unimplemented
	void set_sourceline(char *& cursor);

public:
	Scanner(std::istream&, std::ostream&);
	~Scanner();

	enum ParseMode {
		Stop,
		Parse,
		Reuse,
		Rules
	};

	ParseMode echo();
	int scan();
	void reuse();
	
	size_t get_pos() const;
	void save_state(ScannerState&) const;
	void restore_state(const ScannerState&);

	unsigned get_cline() const;
	void set_in_parse(bool new_in_parse);
	void fatal_at(unsigned line, unsigned ofs, const char *msg) const;
	void fatalf_at(unsigned line, const char*, ...) const;
	void fatalf(const char*, ...) const;
	void fatal(const char*) const;
	void fatal(unsigned, const char*) const;

	void config(const Str&, int);
	void config(const Str&, const Str&);

	void check_token_length(char *pos, unsigned len) const;
	SubStr token() const;
	SubStr token(unsigned start, unsigned len) const;
	Str raw_token(std::string enclosure) const;
	virtual unsigned get_line() const;	
	unsigned xlat(unsigned c) const;

	unsigned unescape(SubStr &s) const;
	std::string& unescape(SubStr& str_in, std::string& str_out) const;

	Range * getRange(SubStr &s) const;
	RegExp * matchChar(unsigned c) const;
	RegExp * strToName(SubStr s) const;
	RegExp * strToRE(SubStr s) const;
	RegExp * strToCaseInsensitiveRE(SubStr s) const;
	RegExp * ranToRE(SubStr s) const;
	RegExp * getAnyRE() const;
	RegExp * invToRE(SubStr s) const;
	RegExp * mkDot() const;
};

inline size_t Scanner::get_pos() const
{
	return cur - bot;
}

inline unsigned Scanner::get_line() const
{
	return cline;
}

inline unsigned Scanner::get_cline() const
{
	return cline;
}

inline void Scanner::save_state(ScannerState& state) const
{
	state = *this;
}

inline void Scanner::fatal(const char *msg) const
{
	fatal(0, msg);
}

inline SubStr Scanner::token() const
{
	check_token_length(tok, cur - tok);
	return SubStr(tok, cur - tok);
}

inline SubStr Scanner::token(unsigned start, unsigned len) const
{
	check_token_length(tok + start, len);
	return SubStr(tok + start, len);
}

inline unsigned Scanner::xlat(unsigned c) const
{
	return re2c::wFlag ? c : re2c::xlat[c & 0xFF];
}

// ---------------------------------------------------------------------

// moved from parser.h

class Symbol
{
public:

	RegExp*   re;

	static Symbol *find(const SubStr&);
	static void ClearTable();

	typedef std::map<std::string, Symbol*> SymbolTable;
	
	const Str& GetName() const
	{
		return name;
	}

protected:

	Symbol(const SubStr& str)
		: re(NULL)
		, name(str)
	{
	}

private:

	static SymbolTable symbol_table;

	Str	name;

#if PEDANTIC
	Symbol(const Symbol& oth)
		: re(oth.re)
		, name(oth.name)
	{
	}
	Symbol& operator = (const Symbol& oth)
	{
		new(this) Symbol(oth);
		return *this;
	}
#endif
};

extern void parse(Scanner&, std::ostream&, std::ostream*);
extern void parse_cleanup();


} // end namespace re2c

#endif
