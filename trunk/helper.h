#pragma once

#include <vector>
#include <string>

#define MEMORY_DEBUGGING     0

#if MEMORY_DEBUGGING

void* xalloc (size_t cb);
void xfree (void* p);

#define malloc xalloc
#define free xfree

char* _mstrdup (const char* szSrc);
#define _strdup _mstrdup

#endif

char* strndup (const char* str, size_t n);

typedef struct COFF_SYMBOL_TABLE
{
	SIZE_T NumberOfSymbols;
#pragma warning(disable:4200)
	IMAGE_SYMBOL Symbols[0];
#pragma warning(default:4200)
} COFF_SYMBOL_TABLE, *PCOFF_SYMBOL_TABLE;
// 
// 
// PCOFF_SYMBOL_TABLE SYMCreate (size_t cSyms);
// PCOFF_SYMBOL_TABLE SYMFromCoffSyms (PIMAGE_SYMBOL SymTbl, size_t cSyms);
// PCOFF_SYMBOL_TABLE SYMFromVector (std::vector<IMAGE_SYMBOL> &syms);
// PCOFF_SYMBOL_TABLE SYMDuplicate (PCOFF_SYMBOL_TABLE SymTbl);
// PIMAGE_SYMBOL SYMGetSymbol (PCOFF_SYMBOL_TABLE pSym, unsigned iSym);
// void SYMDestroy (PCOFF_SYMBOL_TABLE pSym);
// void SYMSetSymbol (PCOFF_SYMBOL_TABLE pSym, unsigned iSym, PIMAGE_SYMBOL Sym);
// size_t SYMGetSymsCount (PCOFF_SYMBOL_TABLE pSym);
// DWORD SYMGetIndex (PCOFF_SYMBOL_TABLE pSym, PIMAGE_SYMBOL Sym);
// size_t SYMStore (PCOFF_SYMBOL_TABLE pSym, void *pBuffer);
// PCOFF_SYMBOL_TABLE SYMConcat (PCOFF_SYMBOL_TABLE pOne, PCOFF_SYMBOL_TABLE pOther);
// PIMAGE_SYMBOL SYMFirstSymbol (PCOFF_SYMBOL_TABLE pSym);
// PIMAGE_SYMBOL SYMLastSymbol (PCOFF_SYMBOL_TABLE pSym);
// void SYMToVector (PCOFF_SYMBOL_TABLE pSym, std::vector<IMAGE_SYMBOL> &syms);

void FreeSymName (PSZ szSymName);



typedef struct MEMBLOCK
{
	PVOID pBuf;
	SIZE_T cBuf;
} MEMBLOCK, *PMEMBLOCK;

inline MEMBLOCK MemBlock (PVOID p, SIZE_T cb)
{
	MEMBLOCK mb = {p, cb};
	return mb;
}

MEMBLOCK AllocateMemBlock (SIZE_T cb);
void FreeMemBlock (MEMBLOCK mb);

static void WriteBuff (HANDLE hFile, PVOID pb, SIZE_T cb)
{
	DWORD wr = 0;
	if (!WriteFile (hFile, pb, cb, &wr, 0))
		__debugbreak();
	if (wr < cb)
		__debugbreak();
}

static void WriteBlock (HANDLE hFile, MEMBLOCK &mb)
{
	DWORD wr = 0;
	if (!WriteFile (hFile, mb.pBuf, mb.cBuf, &wr, 0))
		__debugbreak();
	if (wr < mb.cBuf)
		__debugbreak();
}
