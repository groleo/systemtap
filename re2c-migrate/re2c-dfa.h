/* $Id$ */
#ifndef _re2c_dfa_h
#define _re2c_dfa_h

#include <iosfwd>
#include <map>
#include "re2c-regex.h"

namespace re2c
{

extern void prtCh(std::ostream&, unsigned);
extern void prtHex(std::ostream&, unsigned);
extern void prtChOrHex(std::ostream&, unsigned);
extern void printSpan(std::ostream&, unsigned, unsigned);

class DFA;

class State;

class Action
{

public:
	State	*state;

public:
	Action(State*);
	virtual ~Action();

	virtual void emit(std::ostream&, unsigned, bool&, const std::string&) const = 0;
	virtual bool isRule() const;
	virtual bool isMatch() const;
	virtual bool isInitial() const;
	virtual bool readAhead() const;

#ifdef PEDANTIC
protected:
	Action(const Action& oth)
		: state(oth.state)
	{
	}
	Action& operator = (const Action& oth)
	{
		state = oth.state;
		return *this;
	}
#endif
};

class Match: public Action
{
public:
	Match(State*);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	bool isMatch() const;
};

class Enter: public Action
{
public:
	unsigned	label;

public:
	Enter(State*, unsigned);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
};

class Initial: public Enter
{
public:
	bool setMarker;

public:
	Initial(State*, unsigned, bool);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	bool isInitial() const;
};

class Save: public Match
{

public:
	unsigned	selector;

public:
	Save(State*, unsigned);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	bool isMatch() const;
};

class Move: public Action
{

public:
	Move(State*);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
};

class Accept: public Action
{

public:
	typedef std::map<unsigned, State*> RuleMap;

	unsigned	nRules;
	unsigned	*saves;
	State	**rules;
	RuleMap mapRules;

public:
	Accept(State*, unsigned, unsigned*, State**);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	void emitBinary(std::ostream &o, unsigned ind, unsigned l, unsigned r, bool &readCh) const;
	void genRuleMap();

#ifdef PEDANTIC
private:
	Accept(const Accept& oth)
		: Action(oth)
		, nRules(oth.nRules)
		, saves(oth.saves)
		, rules(oth.rules)
	{
	}
	Accept& operator=(const Accept& oth)
	{
		new(this) Accept(oth);
		return *this;
	}
#endif
};

class Rule: public Action
{

public:
	RuleOp	*rule;

public:
	Rule(State*, RuleOp*);
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	bool isRule() const;

#ifdef PEDANTIC
private:
	Rule (const Rule& oth)
		: Action(oth)
		, rule(oth.rule)
	{
	}
	Rule& operator=(const Rule& oth)
	{
		new(this) Rule(oth);
		return *this;
	}
#endif
};

class Span
{

public:
	unsigned	ub;
	State	*to;

public:
	unsigned show(std::ostream&, unsigned) const;
};

class Go
{
public:
	Go()
		: nSpans(0)
		, wSpans(~0u)
		, lSpans(~0u)
		, dSpans(~0u)
		, lTargets(~0u)
		, span(NULL)
	{
	}

public:
	unsigned	nSpans; // number of spans
	unsigned    wSpans; // number of spans in wide mode
	unsigned    lSpans; // number of low (non wide) spans
	unsigned    dSpans; // number of decision spans (decide between g and b mode)
	unsigned    lTargets;
	Span	*span;

public:
	void genGoto(  std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh);
	void genBase(  std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh, unsigned mask) const;
	void genLinear(std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh, unsigned mask) const;
	void genBinary(std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh, unsigned mask) const;
	void genSwitch(std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh, unsigned mask) const;
	void genCpGoto(std::ostream&, unsigned ind, const State *from, const State *next, bool &readCh) const;
	void compact();
	void unmap(Go*, const State*);
};

class State
{

public:
	unsigned	label;
	RuleOp	*rule;
	State	*next;
	State	*link;
	unsigned	depth;		// for finding SCCs
	unsigned	kCount;
	Ins 	**kernel;

	bool    isPreCtxt;
	bool    isBase;
	Go      go;
	Action  *action;

public:
	State();
	~State();
	void emit(std::ostream&, unsigned, bool&, const std::string&) const;
	friend std::ostream& operator<<(std::ostream&, const State&);
	friend std::ostream& operator<<(std::ostream&, const State*);

#ifdef PEDANTIC
private:
	State(const State& oth)
		: label(oth.label)
		, rule(oth.rule)
		, next(oth.next)
		, link(oth.link)
		, depth(oth.depth)
		, kCount(oth.kCount)
		, kernel(oth.kernel)
		, isBase(oth.isBase)
		, go(oth.go)
		, action(oth.action)
	{
	}
	State& operator = (const State& oth)
	{
		new(this) State(oth);
		return *this;
	}
#endif
};

class DFA
{

public:
	unsigned	lbChar;
	unsigned	ubChar;
	unsigned	nStates;
	State	*head, **tail;
	State	*toDo;
	const Ins     *free_ins;
	const Char    *free_rep;

protected:
	bool    bSaveOnHead;
	unsigned    *saves;
	State   **rules;

public:
	DFA(Ins*, unsigned, unsigned, unsigned, const Char*);
	~DFA();
	void addState(State**, State*);
	State *findState(Ins**, unsigned);
	void split(State*);

	void findSCCs();
	void findBaseState();
	void prepare();
	void emit(std::ostream&, unsigned&, const RegExpMap*, const std::string&, bool, bool&);

	friend std::ostream& operator<<(std::ostream&, const DFA&);
	friend std::ostream& operator<<(std::ostream&, const DFA*);

#ifdef PEDANTIC
	DFA(const DFA& oth)
		: lbChar(oth.lbChar)
		, ubChar(oth.ubChar)
		, nStates(oth.nStates)
		, head(oth.head)
		, tail(oth.tail)
		, toDo(oth.toDo)
	{
	}
	DFA& operator = (const DFA& oth)
	{
		new(this) DFA(oth);
		return *this;
	}
#endif
};

inline Action::Action(State *s) : state(s)
{
	delete s->action;
	s->action = this;
}

inline Action::~Action()
{
}

inline bool Action::isRule() const
{
	return false;
}

inline bool Action::isMatch() const
{
	return false;
}

inline bool Action::isInitial() const
{
	return false;
}

inline bool Action::readAhead() const
{
	return !isMatch() || (state && state->next && state->next->action && !state->next->action->isRule());
}

inline Match::Match(State *s) : Action(s)
{ }

inline bool Match::isMatch() const
{
	return true;
}

inline Enter::Enter(State *s, unsigned l) : Action(s), label(l)
{ }

inline Initial::Initial(State *s, unsigned l, bool b) : Enter(s, l), setMarker(b)
{ }

inline bool Initial::isInitial() const
{
	return true;
}

inline Save::Save(State *s, unsigned i) : Match(s), selector(i)
{ }

inline bool Save::isMatch() const
{
	return false;
}

inline bool Rule::isRule() const
{
	return true;
}

inline std::ostream& operator<<(std::ostream &o, const State *s)
{
	return o << *s;
}

inline std::ostream& operator<<(std::ostream &o, const DFA *dfa)
{
	return o << *dfa;
}

} // end namespace re2c

#endif
