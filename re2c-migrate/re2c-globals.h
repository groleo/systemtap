/* $Id$ */
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

// moved here from basics.h

#if SIZEOF_CHAR == 1
typedef unsigned char byte;
#elif SIZEOF_SHORT == 1
typedef unsigned short byte;
#elif SIZEOF_INT == 1
typedef unsigned int byte;
#elif SIZEOF_LONG == 1
typedef unsigned long byte;
#else
typedef unsigned char byte;
#endif

#if SIZEOF_CHAR == 2
typedef unsigned char word;
#elif SIZEOF_SHORT == 2
typedef unsigned short word;
#elif SIZEOF_INT == 2
typedef unsigned int word;
#elif SIZEOF_LONG == 2
typedef unsigned long word;
#else
typedef unsigned short word;
#endif

#if SIZEOF_CHAR == 4
typedef unsigned char dword;
#elif SIZEOF_SHORT == 4
typedef unsigned short dword;
#elif SIZEOF_INT == 4
typedef unsigned int dword;
#elif SIZEOF_LONG == 4
typedef unsigned long dword;
#else
typedef unsigned long dword;
#endif

typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;

// ------------------------------------------------------------------

extern bool DFlag;
extern bool eFlag;
extern bool uFlag;
extern bool wFlag;

extern const uint asc2asc[256];
extern const uint asc2ebc[256];
extern const uint ebc2asc[256];

extern const uint *xlat;
extern const uint *talx;

extern char octCh(uint c);
extern char hexCh(uint c);

// ------------------------------------------------------------

// moved here from substr.h

class SubStr
{
public:
	const char * str;
	const char * const org;
	uint         len;

public:
	friend bool operator==(const SubStr &, const SubStr &);
	SubStr(const uchar*, uint);
	SubStr(const char*, uint);
	explicit SubStr(const char*);
	SubStr(const SubStr&);
	virtual ~SubStr();
	void out(std::ostream&) const;
	std::string to_string() const;
	uint ofs() const;

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

inline SubStr::SubStr(const uchar *s, uint l)
		: str((char*)s), org((char*)s), len(l)
{ }

inline SubStr::SubStr(const char *s, uint l)
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

inline uint SubStr::ofs() const
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

#ifndef HAVE_STRNDUP

char *strndup(const char *str, size_t len);

#endif

#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf _vsnprintf
#endif

#endif
