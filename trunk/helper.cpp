#include <windows.h>
#include <stdio.h>
#include "helper.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if MEMORY_DEBUGGING

#define PAGE_SIZE 0x1000

#define ALIGN_DOWN(x, align) (x & ~(align-1))
#define ALIGN_UP(x, align) ((x & (align-1))?ALIGN_DOWN(x,align)+align:x)

void* xalloc (size_t cb)
{
	size_t cbFull = ALIGN_UP(cb, PAGE_SIZE) + PAGE_SIZE;

	PSZ pPages = (PSZ) VirtualAlloc (0, cbFull, MEM_COMMIT, PAGE_NOACCESS);
	if (pPages)
	{
		DWORD old;

		if (VirtualProtect (pPages, cbFull - PAGE_SIZE, PAGE_READWRITE, &old))
		{
			PSZ pBuffer = pPages + ALIGN_UP(cb,PAGE_SIZE) - cb;
			*(DWORD*)(pBuffer - 4) = 0xFEFEFEFE;
			*(DWORD*)(pBuffer - 8) = cb;
			return pBuffer;
		}

		VirtualFree (pPages, 0, MEM_RELEASE);
	}
	return NULL;
}

void xfree (void* ptr)
{
	PSZ p = (PSZ)ptr;
	p -= 8;
	size_t cb = ((DWORD*)p)[0];
	if (((DWORD*)p)[1] != 0xFEFEFEFE)
		__debugbreak();
	
	PSZ pBuffer = p + 8 + cb - ALIGN_UP(cb,PAGE_SIZE);
	VirtualFree (pBuffer, 0, MEM_RELEASE);
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char* _mstrdup (const char* szSrc)
{
	size_t cbStr = strlen(szSrc);
	char* str = (char*) malloc (cbStr + 1);
	memcpy (str, szSrc, cbStr);
	str[cbStr] = 0;
	return str;
}

char* strndup (const char* str, size_t n)
{
	char* sz = (char*)malloc(n+1);
	memcpy (sz, str, n);
	sz[n] = 0;
	return sz;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PCOFF_SYMBOL_TABLE SYMCreate (size_t cSyms)
{
	SIZE_T cbSyms = sizeof(COFF_SYMBOL_TABLE) + cSyms * sizeof(IMAGE_SYMBOL);
	PCOFF_SYMBOL_TABLE pSym = (PCOFF_SYMBOL_TABLE) malloc(cbSyms);
	if (pSym != NULL)
	{
		pSym->NumberOfSymbols = cSyms;
		memset (&pSym->Symbols[0], 0, cSyms * sizeof(IMAGE_SYMBOL));
	}
	return pSym;
}

PCOFF_SYMBOL_TABLE SYMFromCoffSyms (PIMAGE_SYMBOL SymTbl, size_t cSyms)
{
	PCOFF_SYMBOL_TABLE pSym = SYMCreate (cSyms);
	if (pSym != NULL)
	{
		memcpy (&pSym->Symbols[0], SymTbl, cSyms * sizeof(IMAGE_SYMBOL));
	}
	return pSym;
}

PCOFF_SYMBOL_TABLE SYMFromVector (std::vector<IMAGE_SYMBOL> &syms)
{
	PCOFF_SYMBOL_TABLE pSym = SYMCreate (syms.size());
	if (pSym != NULL)
	{
		for (unsigned i=0; i<syms.size(); i++)
		{
			pSym->Symbols[i] = syms[i];
		}
	}
	return pSym;
}

PCOFF_SYMBOL_TABLE SYMDuplicate (PCOFF_SYMBOL_TABLE SymTbl)
{
	PCOFF_SYMBOL_TABLE pSym = SYMCreate (SymTbl->NumberOfSymbols);
	if (pSym != NULL)
	{
		memcpy (&pSym->Symbols[0], &SymTbl->Symbols[0], pSym->NumberOfSymbols * sizeof(IMAGE_SYMBOL));
	}
	return pSym;
}

PIMAGE_SYMBOL SYMGetSymbol (PCOFF_SYMBOL_TABLE pSym, unsigned iSym)
{
	if (iSym >= pSym->NumberOfSymbols)
		return NULL;
	return &pSym->Symbols[iSym];
}

void SYMDestroy (PCOFF_SYMBOL_TABLE pSym)
{
	free (pSym);
}

void SYMSetSymbol (PCOFF_SYMBOL_TABLE pSym, unsigned iSym, PIMAGE_SYMBOL Sym)
{
	if (iSym < pSym->NumberOfSymbols)
		pSym->Symbols[iSym] = *Sym;
}

size_t SYMGetSymsCount (PCOFF_SYMBOL_TABLE pSym)
{
	return pSym->NumberOfSymbols;
}

DWORD SYMGetIndex (PCOFF_SYMBOL_TABLE pSym, PIMAGE_SYMBOL Sym)
{
	if ((ULONG_PTR)Sym >= (ULONG_PTR)&pSym->Symbols[0] &&
		(ULONG_PTR)Sym < (ULONG_PTR)&pSym->Symbols[pSym->NumberOfSymbols])
	{
		return Sym - &pSym->Symbols[0];
	}
	return -1;
}

size_t SYMStore (PCOFF_SYMBOL_TABLE pSym, void *pBuffer)
{
	memcpy (pBuffer, &pSym->Symbols[0], pSym->NumberOfSymbols*sizeof(IMAGE_SYMBOL));
	return pSym->NumberOfSymbols;
}

PCOFF_SYMBOL_TABLE SYMConcat (PCOFF_SYMBOL_TABLE pOne, PCOFF_SYMBOL_TABLE pOther)
{
	PCOFF_SYMBOL_TABLE pSym = SYMCreate (pOne->NumberOfSymbols + pOther->NumberOfSymbols);
	if (pSym != NULL)
	{
		memcpy (
			&pSym->Symbols[0], 
			&pOne->Symbols[0], 
			pOne->NumberOfSymbols*sizeof(IMAGE_SYMBOL));

		memcpy (
			&pSym->Symbols[pOne->NumberOfSymbols], 
			&pOther->Symbols[0], 
			pOther->NumberOfSymbols*sizeof(IMAGE_SYMBOL));
	}
	return pSym;
}

PIMAGE_SYMBOL SYMFirstSymbol (PCOFF_SYMBOL_TABLE pSym)
{
	return &pSym->Symbols[0];
}

PIMAGE_SYMBOL SYMLastSymbol (PCOFF_SYMBOL_TABLE pSym)
{
	return &pSym->Symbols[pSym->NumberOfSymbols - 1];
}

void SYMToVector (PCOFF_SYMBOL_TABLE pSym, std::vector<IMAGE_SYMBOL> &syms)
{
	syms.clear ();
	syms.reserve (pSym->NumberOfSymbols);
	syms.insert (syms.end(), SYMFirstSymbol(pSym), SYMLastSymbol(pSym));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FreeSymName (PSZ szSymName)
{
	if (szSymName)
		free (szSymName);
}


MEMBLOCK AllocateMemBlock (SIZE_T cb)
{
	return MemBlock (malloc(cb), cb);
}

void FreeMemBlock (MEMBLOCK mb)
{
	free (mb.pBuf);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

