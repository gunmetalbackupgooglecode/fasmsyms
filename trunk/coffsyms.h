#pragma once

#include <vector>
#include <string>

typedef std::vector<IMAGE_RELOCATION> SECTION_RELOCS;
typedef std::vector<SECTION_RELOCS> COFF_RELOCS;
typedef std::vector<IMAGE_SYMBOL> SYMBOL_VECTOR;
typedef std::vector<std::string> STRING_VECTOR;
typedef std::vector<size_t> OFFSET_VECTOR;

typedef struct COFF_STRING_TABLE
{
	DWORD cbST;
#pragma warning(disable:4200)
	char szST[0];
#pragma warning(default:4200)
} COFF_STRING_TABLE, *PCOFF_STRING_TABLE;

#define EMPTY_STRING_TABLE ((PCOFF_STRING_TABLE)1)

PCOFF_STRING_TABLE STCreate (size_t cbST);
PCOFF_STRING_TABLE STDuplicate (PCOFF_STRING_TABLE pOldST);
PSZ STGetStr (PCOFF_STRING_TABLE pST, DWORD Offs);
void STDestroy (PCOFF_STRING_TABLE pST);
char* STGetFirstString (PCOFF_STRING_TABLE pST);
char* STAddString (PCOFF_STRING_TABLE pST, char* pCurST, const char* szSymName, PDWORD pdwOffset);
size_t STGetStringsLength (PCOFF_STRING_TABLE pST);
size_t STGetFullLength (PCOFF_STRING_TABLE pST);
DWORD STGetOffset (PCOFF_STRING_TABLE pST, char* szStr);
void STStore (PCOFF_STRING_TABLE pST, void* pBuffer);

PCOFF_STRING_TABLE STFromStringVector (STRING_VECTOR &strs, OUT OFFSET_VECTOR &offsets);

bool CoffIsSymNameLong (PIMAGE_SYMBOL Sym);
PSZ CoffSTGetSymbolName (PCOFF_STRING_TABLE pST, PIMAGE_SYMBOL Sym);

PIMAGE_SYMBOL CoffLookupSymbol (
	SYMBOL_VECTOR &OldSymbolTable,
	PCOFF_STRING_TABLE OldStringTable,
	PSZ SymName
	);

DWORD 
CoffLookupSymbolIndex (
	PIMAGE_SYMBOL      SymbolTable,
	SIZE_T             NumberOfSymbols,
	PCOFF_STRING_TABLE pST,
	PSZ                SymName
	);

PCOFF_STRING_TABLE STCreatePartialStringTable(
	SYMBOL_VECTOR &PartialSymbolTable,
	PCOFF_STRING_TABLE pOriginalST
	);

///////////////////////////////////////////////////////////////////////////////

bool CoffBuildSymbolsFromStringMapAndOffsetMap (
	SYMBOL_VECTOR &syms,
	STRING_VECTOR &strs,
	OFFSET_VECTOR &offsets
	);

enum SymSearchCritType {
	SSCT_UNKNOWN,
	SSCT_TYPE,
	SSCT_STORAGE_CLASS,
	SSCT_SECTION_NUMBER,
	SSCT_VALUE
};

typedef struct SYMBOL_SEARCH_CRITERIA {
	DWORD CritType;
	union {
		DWORD Value;
		SHORT SectionNumber;
		WORD Type;
		BYTE StorageClass;
	};
} SYMBOL_SEARCH_CRITERIA, *PSYMBOL_SEARCH_CRITERIA;

PIMAGE_SYMBOL CoffFindSpecialSymbol (
	PIMAGE_SYMBOL SymbolTable,                  // pointer to symbol table
	size_t cSyms,                               // number of symbols
	std::vector<SYMBOL_SEARCH_CRITERIA> &crit,  // vector of search criterias
	bool bAndOperation = true,                  // = true if AND, = false for OR
	PIMAGE_SYMBOL SymStartAt = NULL             // symrec to start search at
	);

PIMAGE_SYMBOL CoffFirstSymbolForSection (
	PIMAGE_SYMBOL SymbolTable, 
	size_t cSyms, 
	SHORT SectionNumber
	);

PIMAGE_SYMBOL CoffNextSymbolForSection(
	PIMAGE_SYMBOL SymbolTable,
	size_t cSyms,
	SHORT SectionNumber,
	PIMAGE_SYMBOL Sym
	);

PIMAGE_SYMBOL CoffFindComdatSymrecForSection (
	PIMAGE_SYMBOL SymbolTable,
	size_t cSyms,
	SHORT SectionNumber,
	OUT PIMAGE_SYMBOL *ppSectSym OPTIONAL        // optionally return section symrec too
	);

PIMAGE_AUX_SYMBOL CoffGetAuxSymbols (PIMAGE_SYMBOL Sym);

bool CoffGetPointers (
	 IN PIMAGE_FILE_HEADER FileHdr,
	OUT PIMAGE_SYMBOL *ppSymTbl,
	OUT size_t *pcSyms OPTIONAL,
	OUT PCOFF_STRING_TABLE *ppST OPTIONAL
	);

void CoffDumpAuxSymbols (PIMAGE_FILE_HEADER FileHdr, PIMAGE_SYMBOL Sym);

void CoffGetSzSectionNumber (SHORT SectionNumber, OUT char* pBuf);

void CoffDumpSymbolType (PIMAGE_SYMBOL Sym);

#include <common/tcharex.h>

PCSZ CoffGetSymStorageClass (PIMAGE_SYMBOL Sym);

void CoffDumpSymbol (
	PIMAGE_FILE_HEADER FileHdr,
	PIMAGE_SYMBOL Sym,
	unsigned iSym,
	PCOFF_STRING_TABLE pST
	);

void CoffDumpSymbolTable (
	PIMAGE_FILE_HEADER FileHdr
	);

void CoffDumpSectionFlags (
	PIMAGE_FILE_HEADER FileHdr,
	unsigned iSect,
	PIMAGE_SECTION_HEADER Sect
	);

void CoffDumpSections (PIMAGE_FILE_HEADER FileHdr);

void CoffDumpSymsVector (SYMBOL_VECTOR &syms, PCOFF_STRING_TABLE pST);

PIMAGE_SYMBOL CoffBuildSymbolTable (SYMBOL_VECTOR &syms);
