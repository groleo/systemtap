// -*- C++ -*-
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ... TODOXXX additional blurb for re2c ...

#include "stapregex.h"
#include "../translate.h"
#include "../session.h"
#include "../util.h"

#include <iostream>
#include <cstdlib>
#include <string>

using namespace std;

// ---------------------------------------------------------------------

// TODOXXX support for standalone regcomp without ugly duplicate code
#ifdef REGCOMP_STANDALONE

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
}

/* just the bare minimum required for regtest */

std::string autosprintf(const char* format, ...)
{
  va_list args;
  char *str;
  va_start (args, format);
  int rc = vasprintf (&str, format, args);
  if (rc < 0)
    {
      va_end(args);
      return _F("autosprintf/vasprintf error %d", rc);
    }
  string s = str;
  va_end (args);
  free (str);
  return s; /* by copy */
}

translator_output::translator_output (ostream& f):
  buf(0), o2 (0), o (f), tablevel (0)
{
}


translator_output::translator_output (const string& filename, size_t bufsize):
  buf (new char[bufsize]),
  o2 (new ofstream (filename.c_str ())),
  o (*o2),
  tablevel (0),
  filename (filename)
{
  o2->rdbuf()->pubsetbuf(buf, bufsize);
}


translator_output::~translator_output ()
{
  delete o2;
  delete [] buf;
}


ostream&
translator_output::newline (int indent)
{
  if (!  (indent > 0 || tablevel >= (unsigned)-indent)) o.flush ();
  assert (indent > 0 || tablevel >= (unsigned)-indent);

  tablevel += indent;
  o << "\n";
  for (unsigned i=0; i<tablevel; i++)
    o << "  ";
  return o;
}


void
translator_output::indent (int indent)
{
  if (!  (indent > 0 || tablevel >= (unsigned)-indent)) o.flush ();
  assert (indent > 0 || tablevel >= (unsigned)-indent);
  tablevel += indent;
}


ostream&
translator_output::line ()
{
  return o;
}

#endif

// ---------------------------------------------------------------------

#include "re2c-globals.h"
#include "re2c-dfa.h"
#include "re2c-regex.h"

using namespace re2c;

class regex_parser {
public:
  regex_parser (const string& input) : input(input) {}
  RegExp *parse ();

private:
  string input;

  // scan state
  char next ();
  char peek ();
  bool finished ();

  unsigned next_pos; // pos of next char to be returned
  unsigned last_pos; // pos of last returned char
  char next_c;
  char last_c;

  // TODOXXX throughout: re2c errors should become parse errors
  void parse_error (const string& msg, unsigned pos);
  void parse_error (const string& msg); // report error at last_pos

  // character classes
  bool isspecial (char c); // any of .[{()\*+?|^$

  // expectations
  void expect (char expected);

private: // re2c hackery
  Scanner *sc;

private: // nonterminals
  RegExp *parse_expr ();
  RegExp *parse_term ();
  RegExp *parse_factor ();
  RegExp *parse_char_range ();
  unsigned parse_number ();
};


// ------------------------------------------------------------------------

stapdfa * 
regex_to_stapdfa (systemtap_session *s, const string& input, unsigned& counter)
{
  if (s->dfas.find(input) != s->dfas.end())
    return s->dfas[input];

  return s->dfas[input] = new stapdfa ("__stp_dfa" + lex_cast(counter++), input);
}

// ------------------------------------------------------------------------

RegExp *stapdfa::failRE = NULL;
RegExp *stapdfa::padRE = NULL;

stapdfa::stapdfa (const string& func_name, const string& re)
  : orig_input(re), func_name(func_name)
{
  if (!failRE) {
    //regex_parser p("[\\000-\\377]");
    regex_parser p("");
    failRE = p.parse();
  }
  if (!padRE) {
    regex_parser p(".*");
    padRE = p.parse();
  }

  regex_parser p(re);
  ast = prepare_rule(p.parse ()); // must be retained for re2c's reference

  // compile ast to DFA
  content = genCode (ast);
  // cerr << content;
  content->prepare();
}

stapdfa::~stapdfa ()
{
  delete content;
  delete ast;
}

void
stapdfa::emit_declaration (translator_output *o)
{
  o->newline() << "int";
  o->newline() << func_name << " (char *cur)";
  o->newline() << "{";
  o->indent(1);

  o->newline() << "char *start = cur;";
  o->newline() << "unsigned l = strlen(cur) + 1;"; /* include \0 byte at end of string */
  o->newline() << "char *mar;";
  o->newline() << "#define YYCTYPE char";
  o->newline() << "#define YYCURSOR cur";
  o->newline() << "#define YYLIMIT cur";
  o->newline() << "#define YYMARKER mar";
  o->newline() << "#define YYFILL(n) ({ if ((cur - start) + n > l) return 0; })";

  unsigned topIndent = 0;
  bool bPrologBrace = false;
  content->emit(o->newline(), topIndent, NULL, "", 0, bPrologBrace);

  o->newline() << "#undef YYCTYPE";
  o->newline() << "#undef YYCURSOR";
  o->newline() << "#undef YYLIMIT";
  o->newline() << "#undef YYMARKER";
  o->newline() << "#undef YYFILL";

  o->newline(-1) << "}";
}

void
stapdfa::emit_matchop_start (translator_output *o)
{
  // TODOXXX eventually imitate visit_functioncall in translate.cxx??
  o->line() << "(" << func_name << " (";
}

void
stapdfa::emit_matchop_end (translator_output *o)
{
  // TODOXXX eventually imitate visit_functioncall in translate.cxx??
  o->line() << ")" << ")";
}

RegExp *
stapdfa::prepare_rule (RegExp *expr)
{
  // Enable regex match to start at any point in the string:
  if (!expr->anchored) expr = new CatOp (padRE, expr);

#define CODE_YES "{ return 1; }"
#define CODE_NO "{ return 0; }"
  SubStr codeYes(CODE_YES);
  Token *tokenYes = new Token(codeYes, CODE_YES, 0);
  SubStr codeNo(CODE_NO);
  Token *tokenNo = new Token(codeNo, CODE_NO, 0);

  RegExp *nope = new NullOp;

  // To ensure separate outcomes for each alternative (match or fail),
  // simply give different accept parameters to the RuleOp
  // constructor:
  RegExp *resMatch = new RuleOp(expr, nope, tokenYes, 0);
  RegExp *resFail = new RuleOp(failRE, nope, tokenNo, 1);

  return mkAlt(resMatch, resFail);
}

void
stapdfa::print (std::ostream& o) const
{
  // TODOXXX escape special chars in orig_input
  o << "dfa(" << func_name << ",\"" << orig_input << "\")" << endl;
  o << content << endl;
  // TODOXXX properly indent and delineate content
}

std::ostream&
operator << (std::ostream &o, const stapdfa& d)
{
  d.print (o);
  return o;
}

// ------------------------------------------------------------------------

RegExp *
regex_parser::parse ()
{
  sc = new Scanner(cin, cout); // cin/cout are actually ignored here...
  next_c = 0; last_c = 0;
  next_pos = 0; last_pos = 0;

  RegExp *result = parse_expr ();

  if (! finished ())
    {
      char c = peek ();
      if (c == ')')
        parse_error ("unbalanced ')'", next_pos);
      else
        // This should not be possible:
        parse_error ("BUG -- regex parse failed to finish for unknown reasons", next_pos);
    }

  delete sc;
  return result;
}

char
regex_parser::next ()
{
  if (! next_c && finished ())
    parse_error(_("unexpected end of input"), next_pos);
  if (! next_c)
    {
      last_pos = next_pos;
      next_c = input[next_pos];
      next_pos++;
    }

  last_c = next_c;
  // advance by zeroing next_c
  next_c = 0;
  return last_c;
}

char
regex_parser::peek ()
{
  if (! next_c && ! finished ())
    {
      last_pos = next_pos;
      next_c = input[next_pos];
      next_pos++;
    }

  // don't advance by zeroing next_c
  last_c = next_c;
  return next_c;
}

bool
regex_parser::finished ()
{
  return ( next_pos >= input.size() );
}

bool
regex_parser::isspecial (char c)
{
  return ( c == '.' || c == '[' || c == '{' || c == '(' || c == ')'
           || c == '\\' || c == '*' || c == '+' || c == '?' || c == '|'
           || c == '^' || c == '$' );
}

void
regex_parser::expect (char expected)
{
  char c;
  try {
    c = next ();
  } catch (const dfa_parse_error &e) {
    parse_error (_F("expected %c, found end of input", expected));
  }

  if (c != expected)
    parse_error (_F("expected %c, found %c", expected, c));
}

void
regex_parser::parse_error (const string& msg, unsigned pos)
{
  throw dfa_parse_error(msg, input, pos);
}

void
regex_parser::parse_error (const string& msg)
{
  parse_error (msg, last_pos);
}

// ------------------------------------------------------------------------

RegExp *
regex_parser::parse_expr ()
{
  RegExp *result = parse_term ();

  char c = peek ();
  while (c && c == '|')
    {
      next ();
      RegExp *alt = parse_term ();
      result = mkAlt (result, alt); // TODOXXX right-association o.k.?
      c = peek ();
    }

  return result;
}

RegExp *
regex_parser::parse_term ()
{
  RegExp *result = parse_factor ();

  char c = peek ();
  while (c && c != '|' && c != ')')
    {
      RegExp *next = parse_factor ();
      result = new CatOp(result, next); // TODOXXX right-association o.k.?
      c = peek ();
    }

  return result;
}

RegExp *
regex_parser::parse_factor ()
{
  RegExp *result;

  char c = peek ();
  if (! c || c == '|' || c == ')')
    {
      result = new NullOp();
      return result;
    }
  else if (c == '*' || c == '+' || c == '?' || c == '{')
    {
      parse_error(_F("unexpected '%c'", c));
    }

  if (isspecial (c) && c != '\\')
    next (); // c is guaranteed to be swallowed

  if (c == '.')
    {
      result = sc->mkDot ();
    }
  else if (c == '[')
    {
      result = parse_char_range ();
      expect (']');
    }
  else if (c == '(')
    {
      result = parse_expr ();
      expect (')');
    }
  else if (c == '^' || c == '$')
    {
      result = new AnchorOp(c);
    }
  else // escaped or ordinary character -- not yet swallowed
    {
      string accumulate;

      while (c && ( ! isspecial (c) || c == '\\' ))
        {
          if (c == '\\')
            {
              next ();
              c = peek ();
            }

          accumulate.push_back (c);
          next ();
          c = peek ();
        }

      // strToRE takes this funky custom class
      SubStr accumSubStr (accumulate.c_str ());
      result = sc->strToRE (accumSubStr);
    }

  /* parse closures or other postfix operators */
  c = peek ();
  while (c == '*' || c == '+' || c == '?' || c == '{')
    {
      next ();

      /* closure-type operators applied to $^ are definitely not kosher */
      if (string(result->typeOf()) == string("AnchorOp"))
        {
          parse_error(_F("postfix closure '%c' applied to anchoring operator", c));
        }

      if (c == '*')
        {
          result = mkAlt (new CloseOp(result), new NullOp());
        }
      else if (c == '+')
        {
          result = new CloseOp(result);
        }
      else if (c == '?')
        {
          result = mkAlt (result, new NullOp());
        }
      else if (c == '{')
        {
          int minsize = parse_number ();
          int maxsize;

          c = next ();
          if (c == ',')
            {
              c = peek ();
              if (c == '}')
                {
                  next ();
                  maxsize = -1;
                }
              else if (isdigit (c))
                {
                  maxsize = parse_number ();
                  expect ('}');
                }
              else
                parse_error(_("expected '}' or number"), next_pos);
            }
          else if (c == '}')
            {
              maxsize = minsize;
            }
          else
            parse_error(_("expected ',' or '}'"));

          /* optimize {0,0}, {0,} and {1,} */
          if (minsize == 0 && maxsize == 0)
            {
              // TODOXXX will not be correct in the case of subgroups
              delete result;
              result = new NullOp();
            }
          else if (minsize == 0 && maxsize == -1)
            {
              result = mkAlt (new CloseOp(result), new NullOp());
            }
          else if (minsize == 1 && maxsize == -1)
            {
              result = new CloseOp(result);
            }
          else
            {
              result = new CloseVOp(result, minsize, maxsize);
            }
        }
      
      c = peek ();
    }

  return result;
}

RegExp *
regex_parser::parse_char_range ()
{
  string accumulate;

  bool inv = false;
  char c = peek ();
  if (c == '^')
    {
      inv = true;
      next ();
      c = peek ();
    }

  // grab range to next ']'
  while (c != ']')
    {
      accumulate.push_back (c);
      next ();
      c = peek ();
    }

  // invToRE and ranToRE take this funky custom class
  SubStr accumSubStr (accumulate.c_str ());
  return inv ? sc->invToRE (accumSubStr) : sc->ranToRE (accumSubStr);
}

unsigned
regex_parser::parse_number ()
{
  string digits;

  char c = peek ();
  while (c && isdigit (c))
    {
      next ();
      digits.push_back (c);
      c = peek ();
    }

  if (digits == "") parse_error("expected number", next_pos);

  // TODOXXX check for overly large numbers
  return atoi (digits.c_str ());
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
