/* Moved here from main.cc: */

#include <fstream>
#include <iostream>
#include <set>
#include <stdlib.h>
#include <string.h>

#include "stream_lc.h"
#include "code_names.h"
#include "re.h"

namespace re2c
{

file_info sourceFileInfo;
file_info outputFileInfo;
file_info headerFileInfo;

bool bFlag = false;
bool cFlag = false;
bool dFlag = false;
bool DFlag = false;
bool eFlag = false;
bool fFlag = false;
bool FFlag = false;
bool gFlag = false;
bool iFlag = false;
bool rFlag = false;
bool sFlag = false;
bool tFlag = false;
bool uFlag = false;
bool wFlag = false;

bool bNoGenerationDate = false;

bool bSinglePass = false;
bool bFirstPass  = true;
bool bLastPass   = false;
bool bUsedYYBitmap  = false;

bool bUsedYYAccept  = false;
bool bUsedYYMaxFill = false;
bool bUsedYYMarker  = true;

bool bEmitYYCh       = true;
bool bUseStartLabel  = false;
bool bUseStateNext   = false;
bool bUseYYFill      = true;
bool bUseYYFillParam = true;
bool bUseYYFillCheck = true;
bool bUseYYFillNaked = false;
bool bUseYYSetConditionParam = true;
bool bUseYYGetConditionNaked = false;
bool bUseYYSetStateParam = true;
bool bUseYYSetStateNaked = false;
bool bUseYYGetStateNaked = false;

std::string startLabelName;
std::string labelPrefix("yy");
std::string condPrefix("yyc_");
std::string condEnumPrefix("yyc");
std::string condDivider("/* *********************************** */");
std::string condDividerParam("@@");
std::string condGoto("goto @@;");
std::string condGotoParam("@@");
std::string yychConversion("");
std::string yyFillLength("@@");
std::string yySetConditionParam("@@");
std::string yySetStateParam("@@");
std::string yySetupRule("");
uint maxFill = 1;
uint next_label = 0;
uint cGotoThreshold = 9;

uint topIndent = 0;
std::string indString("\t");
bool yybmHexTable = false;
bool bUseStateAbort = false;
bool bWroteGetState = false;
bool bWroteCondCheck = false;
bool bCaseInsensitive = false;
bool bCaseInverted = false;
bool bTypesDone = false;

uint nRealChars = 256;

uint next_fill_index = 0;
uint last_fill_index = 0;
std::set<uint> vUsedLabels;
CodeNames mapCodeName;
std::string typesInline;

free_list<RegExp*> RegExp::vFreeList;
free_list<Range*>  Range::vFreeList;

}
