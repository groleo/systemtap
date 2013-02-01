// -*- C++ -*-
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ---
//
// This file incorporates code from the re2c project; please see
// re2c-migrate/README for details.

#ifndef	_re2c_globals_h
#define	_re2c_globals_h

#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(_WIN32)
#include "config_w32.h"
#endif

#include <set>
#include <algorithm>
#include <iostream>
#include <string>
#include <string.h>

namespace re2c
{

extern bool DFlag;
extern bool eFlag;
extern bool uFlag;
extern bool wFlag;

extern const unsigned asc2asc[256];
extern const unsigned asc2ebc[256];
extern const unsigned ebc2asc[256];

extern const unsigned *xlat;
extern const unsigned *talx;

extern char octCh(unsigned c);
extern char hexCh(unsigned c);

// ------------------------------------------------------------

// moved here from substr.h

class SubStr
{
public:
	const char * str;
	const char * const org;
	unsigned         len;

public:
	friend bool operator==(const SubStr &, const SubStr &);
	SubStr(const unsigned char*, unsigned);
	SubStr(const char*, unsigned);
	explicit SubStr(const char*);
	SubStr(const SubStr&);
	virtual ~SubStr();
	void out(std::ostream&) const;
	std::string to_string() const;
	unsigned ofs() const;

#ifdef PEDANTIC
protected:
	SubStr& operator = (const SubStr& oth);
#endif
};

class Str: public SubStr
{
public:
	explicit Str(const char*);
	Str(const SubStr&);
	Str();
	virtual ~Str();
};

inline std::ostream& operator<<(std::ostream& o, const SubStr &s)
{
	s.out(o);
	return o;
}

inline std::ostream& operator<<(std::ostream& o, const SubStr* s)
{
	return o << *s;
}

inline SubStr::SubStr(const unsigned char *s, unsigned l)
		: str((char*)s), org((char*)s), len(l)
{ }

inline SubStr::SubStr(const char *s, unsigned l)
		: str(s), org(s), len(l)
{ }

inline SubStr::SubStr(const char *s)
		: str(s), org(s), len(strlen(s))
{ }

inline SubStr::SubStr(const SubStr &s)
		: str(s.str), org(s.str), len(s.len)
{ }

inline SubStr::~SubStr()
{ }

inline std::string SubStr::to_string() const
{
	return str && len ? std::string(str, len) : std::string();
}

inline unsigned SubStr::ofs() const
{
	return str - org;
}

#ifdef PEDANTIC
inline SubStr& SubStr::operator = (const SubStr& oth)
{
	new(this) SubStr(oth);
	return *this;
}
#endif

} // end namespace re2c

#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf _vsnprintf
#endif

#endif
