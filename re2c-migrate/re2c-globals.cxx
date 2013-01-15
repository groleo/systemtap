/* Moved here from main.cc: */

#include <fstream>
#include <iostream>
#include <set>
#include <stdlib.h>
#include <string.h>

#include "re.h"

#ifndef HAVE_STRNDUP

char *strndup(const char *str, size_t len) throw ()
{
	char * ret = (char*)malloc(len + 1);
	
	memcpy(ret, str, len);
	ret[len] = '\0';
	return ret;
}

#endif

namespace re2c
{

bool DFlag = false;
bool eFlag = false;
bool uFlag = false;
bool wFlag = false;

free_list<RegExp*> RegExp::vFreeList;
free_list<Range*>  Range::vFreeList;

// moved here from substr.h

void SubStr::out(std::ostream& o) const
{
	o.write(str, len);
}

bool operator==(const SubStr &s1, const SubStr &s2)
{
	return (bool) (s1.len == s2.len && memcmp(s1.str, s2.str, s1.len) == 0);
}

Str::Str(const SubStr& s)
	: SubStr(strndup(s.str, s.len), s.len)
{
	;
}

Str::Str(const char *s)
	: SubStr(strdup(s), strlen(s))
{
	;
}

Str::Str()
	: SubStr((char*) NULL, 0)
{
	;
}


Str::~Str()
{
	if (str) {
		free((void*)str);
	}
	str = NULL;
	len = 0;
}

} // end namespace re2c
