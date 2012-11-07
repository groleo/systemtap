// -*- C++ -*-
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ... TODOXXX additional blurb for re2c ...

// TODOXXX support for REGCOMP_STANDALONE
#include "regcomp.h"
#include "../translate.h"
#include "../session.h"
#include "../util.h"

// TODOXXX more relevant includes from stdlib
#include <iostream>
#include <cstdlib>
#include <string>

#include "scanner.h"
#include "substr.h"
#include "re.h"
#include "dfa.h"

using namespace std;
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
    return &(s->dfas[input]);

  stapdfa d("__stp_dfa" + lex_cast(counter++), input);
  return &(s->dfas[input] = d);
}

// ------------------------------------------------------------------------

stapdfa::stapdfa ()
{
  content = NULL;
}

stapdfa::stapdfa (const string& func_name, const string& re)
  : orig_input(re), func_name(func_name)
{
  regex_parser p(re);
  RegExp *ast = p.parse ();

  // compile ast to DFA
  content = genCode (ast);
  content->prepare();

  delete ast;
}

stapdfa::~stapdfa ()
{
  delete content;
}

void
stapdfa::emit_declaration (translator_output *o)
{
  o->newline() << "int";
  o->newline() << func_name << " (char *cur)";
  o->newline() << "{";
  // TODOXXX indent contents

  o->newline() << "char *mar;";
  // TODOXXX anchor at left column:
  o->newline() << "#define YYCTYPE char";
  o->newline() << "#define YYCURSOR cur";
  o->newline() << "#define YYLIMIT cur";
  o->newline() << "#define YYMARKER mar";
  o->newline() << "#define YYFILL(n) ";

  // TODOXXX basic code + invocation of appropriate re2c code
  RegExpMap* specMap = new RegExpMap();
  re2c::uint ind = 0;
  string condName;
  bool bPrologBrace = false;
  content->emit(o->newline(), ind, specMap, condName, true, bPrologBrace);
  // o->newline() << "// condName == " << condName;

  o->newline() << "}";
}

void
// TODOXXX fix function prototype
stapdfa::emit_matchop (translator_output *o, const string& match_expr)
{
  // TODOXXX can multiple copies of a match be generated? avoid
  // TODOXXX imitate translate.cxx's careful paren-wrapping
  o->line() << func_name << " (" << match_expr << ")";
}

void
stapdfa::print (std::ostream& o) const
{
  // TODOXXX print func_name, orig_input
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
  sc = new Scanner(cin, cout); // cin/cout actually ignored...
  next_c = 0; last_c = 0;
  next_pos = 0; last_pos = 0;

  RegExp *result = parse_expr ();

  if (! finished ())
    {
      char c = peek ();
      if (c == ')')
        parse_error ("unbalanced ')'", next_pos);
      else
        // TODOXXX which errors are possible here?
        parse_error ("FIXME -- did not reach end of input", next_pos);
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
  //cerr << "NEXT " << last_c << endl;
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
  //cerr << "NEXT " << next_c << endl;
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
  // TODOXXX use switch() for more efficiency?
  return ( c == '.' || c == '[' || c == '{' || c == '(' || c == ')'
           || c == '\\' || c == '*' || c == '+' || c == '?' || c == '|'
           || c == '^' || c == '$' );
}

void
regex_parser::expect (char expected)
{
  char c = next ();
  // TODOXXX handle error for end of input
  if (c != expected)
    parse_error (_F("expected %c, found %c", expected, c));
}

void
regex_parser::parse_error (const string& msg, unsigned pos)
{
  throw new dfa_parse_error(msg, input, pos);
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
      parse_error(_F("FIXME -- '%c' not yet supported", c));
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

      //cerr << "ACCUM " << accumulate << endl;
      // strToRE takes this funky custom class
      SubStr accumSubStr (accumulate.c_str ());
      result = sc->strToRE (accumSubStr);
    }

  /* parse closures or other postfix operators */
  c = peek ();
  while (c == '*' || c == '+' || c == '?' || c == '{')
    {
      next ();
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
    }

  // TODOXXX grab range to next ']'

  cerr << "ACCUMRANGE " << accumulate << endl;
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
