#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "re2c-globals.h"
#include "re2c-dfa.h"

namespace re2c
{

void prtChOrHex(std::ostream& o, unsigned c)
{
	if (eFlag)
	{
		if (DFlag) o << '"';
		prtHex(o, c);
		if (DFlag) o << '"';
	}
	else if ((c < 256u) && (isprint(c) || isspace(c)))
	{
		o << (DFlag ? '"' : '\'');
		prtCh(o, c);
		o << (DFlag ? '"' : '\'');
	}
	else
	{
		if (DFlag) o << '"';
		prtHex(o, c);
		if (DFlag) o << '"';
	}
}

void prtHex(std::ostream& o, unsigned c)
{
	int oc = (int)(c);

	if (re2c::uFlag)
	{
		o << "0x"
		  << hexCh(oc >> 28)
		  << hexCh(oc >> 24)
		  << hexCh(oc >> 20)
		  << hexCh(oc >> 16)
		  << hexCh(oc >> 12)
		  << hexCh(oc >>  8)
		  << hexCh(oc >>  4)
		  << hexCh(oc);
	}
	else if (re2c::wFlag)
	{
		o << "0x"
		  << hexCh(oc >> 12)
		  << hexCh(oc >>  8)
		  << hexCh(oc >>  4)
		  << hexCh(oc);
	}
	else
	{
		o << "0x"
		  << hexCh(oc >>  4) 
		  << hexCh(oc);
	}
}

void prtCh(std::ostream& o, unsigned c)
{
	if (eFlag)
	{
		prtHex(o, c);
		return;
	}

	int oc = (int)(c);

	switch (oc)
	{
		case '\'':
		o << (DFlag ? "'" : "\\'");
		break;

		case '"':
		o << (DFlag ? "\\\"" : "\"");
		break;

		case '\n':
		o << "\\n";
		break;

		case '\t':
		o << "\\t";
		break;

		case '\v':
		o << "\\v";
		break;

		case '\b':
		o << "\\b";
		break;

		case '\r':
		o << "\\r";
		break;

		case '\f':
		o << "\\f";
		break;

		case '\a':
		o << "\\a";
		break;

		case '\\':
		o << "\\\\";
		break;

		default:

		if ((oc < 256) && isprint(oc))
		{
			o << (char) oc;
		}
		else if (re2c::uFlag)
		{
			o << "0x"
			  << hexCh(oc >> 20)
			  << hexCh(oc >> 16)
			  << hexCh(oc >> 12)
			  << hexCh(oc >>  8)
			  << hexCh(oc >>  4)
			  << hexCh(oc);
		}
		else if (re2c::wFlag)
		{
			o << "0x"
			  << hexCh(oc >> 12)
			  << hexCh(oc >>  8)
			  << hexCh(oc >>  4)
			  << hexCh(oc);
		}
		else
		{
			o << '\\' << octCh(oc / 64) << octCh(oc / 8) << octCh(oc);
		}
	}
}

void printSpan(std::ostream& o, unsigned lb, unsigned ub)
{
	if (lb > ub)
	{
		o << "*";
	}

	o << "[";

	if ((ub - lb) == 1)
	{
		prtCh(o, lb);
	}
	else
	{
		prtCh(o, lb);
		o << "-";
		prtCh(o, ub - 1);
	}

	o << "]";
}

unsigned Span::show(std::ostream &o, unsigned lb) const
{
	if (to)
	{
		printSpan(o, lb, ub);
		o << " " << to->label << "; ";
	}

	return ub;
}

std::ostream& operator<<(std::ostream &o, const State &s)
{
	o << "state " << s.label;

	if (s.rule)
	{
		o << " accepts " << s.rule->accept;
	}

	o << "\n";

	unsigned lb = 0;

	for (unsigned i = 0; i < s.go.nSpans; ++i)
	{
		lb = s.go.span[i].show(o, lb);
	}

	return o;
}

std::ostream& operator<<(std::ostream &o, const DFA &dfa)
{
	for (State *s = dfa.head; s; s = s->next)
	{
		o << s << "\n\n";
	}

	return o;
}

State::State()
	: label(0)
	, rule(NULL)
	, next(0)
	, link(NULL)
	, depth(0)
	, kCount(0)
	, kernel(NULL)
	, isPreCtxt(false)
	, isBase(false)
	, go()
	, action(NULL)
{
}

State::~State()
{
	delete action;
	delete [] kernel;
	delete [] go.span;
}

/* Mark all Ins reachable from a given point without slurping a
   character. The list of Ins locations gets written out to a given
   work array starting at cP. */
static Ins **closure(Ins **cP, Ins *i, bool isInitial)
{
	while (!isMarked(i))
	{
		mark(i);
		*(cP++) = i;

		if (i->i.tag == FORK)
		{
                        cP = closure(cP, i + 1, isInitial);
			i = (Ins*) i->i.link;
		}
		else if (i->i.tag == GOTO || i->i.tag == CTXT)
		{
			i = (Ins*) i->i.link;
		}
                else if (i->i.tag == INIT && isInitial)
                {
                        /* TODOXXX The kernel of an initial vs. a
                           non-initial state is distinguished not by
                           the presence of the INIT itself, but by the
                           presence of additional kernel Ins on the
                           other side of the INIT. */
			i = (Ins*) i->i.link;
                }
		else
			break;
	}

	return cP;
}

struct GoTo
{
	Char	ch;
	void	*to;
};

DFA::DFA(Ins *ins, unsigned ni, unsigned lb, unsigned ub, const Char *rep)
	: lbChar(lb)
	, ubChar(ub)
	, nStates(0)
	, head(NULL)
	, tail(&head)
	, toDo(NULL)
	, free_ins(ins)
	, free_rep(rep)
{
	Ins **work = new Ins * [ni + 1];
	unsigned nc = ub - lb;
	GoTo *goTo = new GoTo[nc];
	Span *span = new Span[nc];
	memset((char*) goTo, 0, nc*sizeof(GoTo));

        /* Build the initial state, based on the set of Ins
           that can be reached from &ins[0], including Ins
           reachable only through ^/INIT operations. */
	findState(work, closure(work, &ins[0], true) - work);

	while (toDo)
	{
		State *s = toDo;
		toDo = s->link;

		Ins **cP, **iP, *i;
		unsigned nGoTos = 0;
		unsigned j;

		s->rule = NULL;

		for (iP = s->kernel; (i = *iP); ++iP)
		{
			if (i->i.tag == CHAR)
			{
				for (Ins *j = i + 1; j < (Ins*) i->i.link; ++j)
				{
					if (!(j->c.link = goTo[j->c.value - lb].to))
						goTo[nGoTos++].ch = j->c.value;

					goTo[j->c.value - lb].to = j;
				}
			}
			else if (i->i.tag == TERM)
			{
				if (!s->rule || ((RuleOp*) i->i.link)->accept < s->rule->accept)
					s->rule = (RuleOp*) i->i.link;
			}
			else if (i->i.tag == CTXT)
			{
				s->isPreCtxt = true;
			}
		}

		for (j = 0; j < nGoTos; ++j)
		{
			GoTo *go = &goTo[goTo[j].ch - lb];
			i = (Ins*) go->to;

			for (cP = work; i; i = (Ins*) i->c.link)
                          cP = closure(cP, i + i->c.bump, false);

			go->to = findState(work, cP - work);
		}

		s->go.nSpans = 0;

		for (j = 0; j < nc;)
		{
			State *to = (State*) goTo[rep[j]].to;

			while (++j < nc && goTo[rep[j]].to == to) ;

			span[s->go.nSpans].ub = lb + j;

			span[s->go.nSpans].to = to;

			s->go.nSpans++;
		}

		for (j = nGoTos; j-- > 0;)
			goTo[goTo[j].ch - lb].to = NULL;

		s->go.span = new Span[s->go.nSpans];

		memcpy((char*) s->go.span, (char*) span, s->go.nSpans*sizeof(Span));

		(void) new Match(s);

	}

	delete [] work;
	delete [] goTo;
	delete [] span;
}

DFA::~DFA()
{
	State *s;

	while ((s = head))
	{
		head = s->next;
		delete s;
	}
	delete [] free_ins;
	delete [] free_rep;
	delete [] saves;
	delete [] rules;
}

void DFA::addState(State **a, State *s)
{
	s->label = nStates++;
	s->next = *a;
	*a = s;

	if (a == tail)
		tail = &s->next;
}

/* Finds or creates the unique state in the DFA with specified kernel.
   In order for this function to work, the kernel *must* have been
   obtained from an invocation of closure(), which marks the appropriate
   Ins belonging to the kernel. */
State *DFA::findState(Ins **kernel, unsigned kCount)
{
	Ins **cP, **iP, *i;
	State *s;

	kernel[kCount] = NULL;

	cP = kernel;

        /* Unmark all Ins which don't cross state boundaries, to
           obtain the genuine kernel. */
	for (iP = kernel; (i = *iP); ++iP)
	{
		if (i->i.tag == CHAR || i->i.tag == TERM || i->i.tag == CTXT)
		{
			*cP++ = i;
		}
		else
		{
			unmark(i);
		}
	}

	kCount = cP - kernel;
	kernel[kCount] = NULL;

        /* Compare to the kernels of existing states (by checking
           if the kernel sizes match and if s->kernel contains no
           unmarked Ins). */
	for (s = head; s; s = s->next)
	{
		if (s->kCount == kCount)
		{
			for (iP = s->kernel; (i = *iP); ++iP)
				if (!isMarked(i))
					goto nextState;

			goto unmarkAll;
		}

nextState:
		;
	}

        /* If no existing state found, create one and push it
           on the toDo list (using s->link to maintain a stack). */
	s = new State;
	addState(tail, s);
	s->kCount = kCount;
	s->kernel = new Ins * [kCount + 1];
	memcpy(s->kernel, kernel, (kCount + 1)*sizeof(Ins*));
	s->link = toDo;
	toDo = s;

unmarkAll:

	for (iP = kernel; (i = *iP); ++iP)
		unmark(i);

	return s;
}

} // end namespace re2c

