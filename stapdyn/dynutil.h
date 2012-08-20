#ifndef DYNUTIL_H
#define DYNUTIL_H

// Check that environment DYNINSTAPI_RT_LIB exists and is a valid file.
// If not, try to guess a good value and set it.
bool check_dyninst_rt(void);

// Check that SELinux settings are ok for Dyninst operation.
bool check_dyninst_sebools(void);

#endif // DYNUTIL_H
