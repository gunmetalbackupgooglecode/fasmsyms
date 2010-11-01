#include <windows.h>
#include <assert.h>
#include "helper.h"
#include "coffsyms.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PCOFF_STRING_TABLE STCreate (size_t cbST)
{
	PCOFF_STRING_TABLE pST = (PCOFF_STRING_TABLE) malloc (cbST + sizeof(DWORD));
	pST->cbST = cbST + sizeof(DWORD);
	return pST;
}

PCOFF_STRING_TABLE STDuplicate (PCOFF_STRING_TABLE pOldST)
{
	PCOFF_STRING_TABLE pST = STCreate (pOldST->cbST - sizeof(DWORD));
	if (pST != NULL)
	{
		memcpy (&pST->szST[0], &pOldST->szST[0], pOldST->cbST - sizeof(DWORD));
	}
	return pST;
}

PSZ STGetStr (PCOFF_STRING_TABLE pST, DWORD Offs)
{
	return (PSZ)pST + Offs;
}

void STDestroy (PCOFF_STRING_TABLE pST)
{
	free (pST);
}

char* STGetFirstString (PCOFF_STRING_TABLE pST)
{
	return &pST->szST[0];
}

char* STAddString (PCOFF_STRING_TABLE pST, char* pCurST, const char* szSymName, PDWORD pdwOffset)
{
	strcpy (pCurST, szSymName);
	*pdwOffset = (char*)pCurST - (char*)pST;
	pCurST += strlen(pCurST) + 1;
	if (pCurST > (char*)pST + pST->cbST)
		__debugbreak();
	return pCurST;
}

bool CoffIsSymNameLong (PIMAGE_SYMBOL Sym)
{
	return (Sym->N.Name.Short == 0);
}

PSZ CoffSTGetSymbolName (PCOFF_STRING_TABLE pST, PIMAGE_SYMBOL Sym)
{
	if (Sym->N.Name.Short)
		return strndup ((PSZ)&Sym->N.ShortName[0], 8);
	return _strdup(STGetStr (pST, Sym->N.Name.Long));
}

size_t STGetStringsLength (PCOFF_STRING_TABLE pST)
{
	return pST->cbST - sizeof(DWORD);
}

size_t STGetFullLength (PCOFF_STRING_TABLE pST)
{
	return pST->cbST;
}

DWORD STGetOffset (PCOFF_STRING_TABLE pST, char* szStr)
{
	return (PSZ)szStr - (PSZ)pST;
}

void STStore (PCOFF_STRING_TABLE pST, void* pBuffer)
{
	memcpy (pBuffer, pST, STGetFullLength (pST));
}

PCOFF_STRING_TABLE STFromStringVector (STRING_VECTOR &strs, OUT OFFSET_VECTOR &offsets)
{
	size_t cbST = 0;

	for (unsigned i=0; i<strs.size(); i++)
	{
		if (strs[i].size() > 8)
			cbST += strs[i].size() + 1;
	}

	if (cbST == 0)
	{
		offsets.clear();
		return EMPTY_STRING_TABLE;
	}

	PCOFF_STRING_TABLE pST = STCreate (cbST);
	if (pST != NULL)
	{
		char* pCurPtr = STGetFirstString (pST);

		for (unsigned i=0; i<strs.size(); i++)
		{
			DWORD offset = 0;
			if (strs[i].size() > 8)
				pCurPtr = STAddString (pST, pCurPtr, strs[i].c_str(), &offset);
			offsets.push_back (offset);
		}
	}
	return pST;
}

PIMAGE_SYMBOL CoffLookupSymbol (
	std::vector<IMAGE_SYMBOL> &SymbolTable,
	PCOFF_STRING_TABLE pST,
	PSZ SymName
	)
{
	for (unsigned i=0; i<SymbolTable.size(); i++)
	{
		PIMAGE_SYMBOL Sym = &SymbolTable[i];

		char* Name = NULL;

		if (Sym->N.Name.Short != 0)
			Name = (char*)&Sym->N.ShortName;
		else
			Name = STGetStr (pST, Sym->N.Name.Long);

		if (!strcmp (Name, SymName))
			return Sym;
	}
	return NULL;
}

DWORD 
CoffLookupSymbolIndex (
	PIMAGE_SYMBOL      SymbolTable,
	SIZE_T             NumberOfSymbols,
	PCOFF_STRING_TABLE pST,
	PSZ                SymName
	)
{
	for (unsigned i=0; i<NumberOfSymbols; i++)
	{
		PIMAGE_SYMBOL Sym = &SymbolTable[i];
		char *Name = CoffSTGetSymbolName (pST, Sym);
		
		if (strcmp (Name, SymName) != 0)
			Sym = NULL;

		FreeSymName (Name);
		if (Sym != NULL)
			return i;
	}
	return -1;
}

PCOFF_STRING_TABLE STCreatePartialStringTable(
	std::vector<IMAGE_SYMBOL> PartialSymbolTable,
	PCOFF_STRING_TABLE pOriginalST
	)
{
	SIZE_T cbST = 0;

	for (unsigned i=0; i<PartialSymbolTable.size(); i++)
	{
		PIMAGE_SYMBOL ImgSym = &PartialSymbolTable[i];

		// Add only long-named
		if (ImgSym->N.Name.Short == 0)
		{
			PSZ szSymName = CoffSTGetSymbolName (pOriginalST, ImgSym);
			cbST += strlen(szSymName) + 1;
			FreeSymName (szSymName);
		}
	}

	PCOFF_STRING_TABLE pST = STCreate (cbST);

	if (pST != NULL)
	{
		char* pCurST = STGetFirstString (pST);

		for (unsigned i=0; i<PartialSymbolTable.size(); i++)
		{
			PIMAGE_SYMBOL ImgSym = &PartialSymbolTable[i];

			// Add only long-named
			if (ImgSym->N.Name.Short == 0)
			{
				PSZ szSymName = CoffSTGetSymbolName (pOriginalST, ImgSym);

				pCurST = STAddString (pST, pCurST, szSymName, &ImgSym->N.Name.Long);

				FreeSymName (szSymName);
			}
		}
	}

	return pST;
}

//////////////////////////////////////////////////////////////////////////////

bool CoffBuildSymbolsFromStringMapAndOffsetMap (
	SYMBOL_VECTOR &syms,
	STRING_VECTOR &strs,
	OFFSET_VECTOR &offsets
	)
{
	bool bEmptyStringTable = false;

	if (offsets.size() == 0 && syms.size() != 0)
	{
		bEmptyStringTable = true;
	}
	else
	{
		if (syms.size() != offsets.size())
			return false;
	}

	if (syms.size() != strs.size())
		return false;

	for (unsigned i=0; i<syms.size(); i++)
	{
		PIMAGE_SYMBOL Sym = &syms[i];

		if (!bEmptyStringTable && offsets[i] != 0)
		{
			Sym->N.Name.Short = 0;
			Sym->N.Name.Long = offsets[i];
		}
		else
		{
			strncpy ((char*)&Sym->N.ShortName[0], strs[i].c_str(), IMAGE_SIZEOF_SHORT_NAME);
		}

		i += Sym->NumberOfAuxSymbols;
	}

	return true;
}

//void CoffAddSymbols (SYMBOL_VECTOR &syms, PCOFF_STRING_TABLE pST, PCOFF_STRING_TABLE *pNewST, std::vector/

//
// Search symbol with the specified criteria
//

PIMAGE_SYMBOL CoffFindSpecialSymbol (
	PIMAGE_SYMBOL SymbolTable,                  // pointer to symbol table
	size_t cSyms,                               // number of symbols
	std::vector<SYMBOL_SEARCH_CRITERIA> &crit,  // vector of search criterias
	bool bAndOperation ,                        // = true if AND, = false for OR
	PIMAGE_SYMBOL SymStartAt                    // symrec to start search at
	)
{
	unsigned iStartAt = 0;
	if (SymStartAt) iStartAt = (SymStartAt - SymbolTable);

	// Enum all symrecs
	for (unsigned i=iStartAt; i<cSyms; i++)
	{
		PIMAGE_SYMBOL Sym = &SymbolTable[i];

		bool bCondChainSucceeded = true;

		// Enum all search crits
		for (unsigned c=0; c<crit.size() && bCondChainSucceeded; c++)
		{
			PSYMBOL_SEARCH_CRITERIA Crit = &crit[c];
			bool bConditionSucceeded = false;

			switch (Crit->CritType)
			{
			case SSCT_VALUE:          bConditionSucceeded = (Sym->Value == Crit->Value); break;
			case SSCT_SECTION_NUMBER: bConditionSucceeded = (Sym->SectionNumber == Crit->SectionNumber); break;
			case SSCT_TYPE:           bConditionSucceeded = (Sym->Type == Crit->Type); break;
			case SSCT_STORAGE_CLASS:  bConditionSucceeded = (Sym->StorageClass == Crit->StorageClass); break;
			default:                  bConditionSucceeded = false;
			}

			if (bAndOperation)
			{
				// Stop search when one check fails.
				if (!bConditionSucceeded)
				{
					bCondChainSucceeded = false;
					break;
				}
			}
			else
			{
				// Stop search when one check succeeds.
				if (!bCondChainSucceeded && bConditionSucceeded)
				{
					bCondChainSucceeded = true;
					break;
				}
			}

		} // for (criterias)

		if (bCondChainSucceeded)
			return Sym;

		if (Sym->NumberOfAuxSymbols)
			i += Sym->NumberOfAuxSymbols;

	} // for (symbols)

	return NULL;
}

//
// Find first symbol defined for the section
// (usually IMAGE_SYM_CLASS_with Value = 0 and SectionNumber set to section number)
//

PIMAGE_SYMBOL CoffFirstSymbolForSection (
	PIMAGE_SYMBOL SymbolTable, 
	size_t cSyms, 
	SHORT SectionNumber
	)
{
	for (unsigned i=0; i<cSyms; i++)
	{
		PIMAGE_SYMBOL Sym = &SymbolTable[i];
		if (Sym->SectionNumber == SectionNumber &&
			Sym->StorageClass == IMAGE_SYM_CLASS_STATIC &&
			Sym->Value == 0)
		{
			return Sym;
		}

		if (Sym->NumberOfAuxSymbols) 
			i += Sym->NumberOfAuxSymbols;
	}
	return NULL;
}

//
// Find next symbol defined for the section
//

PIMAGE_SYMBOL CoffNextSymbolForSection(
	PIMAGE_SYMBOL SymbolTable,
	size_t cSyms,
	SHORT SectionNumber,
	PIMAGE_SYMBOL Sym
	)
{
	for (unsigned i=(Sym - SymbolTable + Sym->NumberOfAuxSymbols + 1); 
		i<cSyms; 
		i++)
	{
		Sym = &SymbolTable[i];

		if (Sym->SectionNumber == SectionNumber)
			return Sym;

		if (Sym->NumberOfAuxSymbols) 
			i += Sym->NumberOfAuxSymbols;		
	}

	return NULL;
}

//
// Find COMDAT corresponding symbol record
//

PIMAGE_SYMBOL CoffFindComdatSymrecForSection (
	PIMAGE_SYMBOL SymbolTable,
	size_t cSyms,
	SHORT SectionNumber,
	OUT PIMAGE_SYMBOL *ppSectSym OPTIONAL        // optionally return section symrec too
	)
{
	// Find first symbol for the section. It will be IMAGE_SYM_CLASS_section definition
	//  with one Aux symbol with section attributes.
	PIMAGE_SYMBOL SectSym = CoffFirstSymbolForSection (SymbolTable, cSyms, SectionNumber);
	PIMAGE_SYMBOL ComdatSym = NULL;
	if (SectSym)
	{
		// Find second symbol for the section. It's our required symbol (if it's IMAGE_SYM_CLASS_too).
		ComdatSym = CoffNextSymbolForSection (SymbolTable, cSyms, SectionNumber, SectSym);

		if (ppSectSym)
			*ppSectSym = SectSym;
	}
	return ComdatSym;
}

//
// Get pointer to symbol's aux symbols (if any)
//

PIMAGE_AUX_SYMBOL CoffGetAuxSymbols (PIMAGE_SYMBOL Sym)
{
	if (!Sym->NumberOfAuxSymbols)
		return NULL;
	return (PIMAGE_AUX_SYMBOL)(Sym+1);
}

const char *szSectionAlign[] = {
	0,
	"1 bytes",
	"2 bytes",
	"4 bytes",
	"8 bytes",
	"16 bytes",
	"32 bytes",
	"64 bytes",
	"128 bytes",
	"256 bytes",
	"512 bytes",
	"1k bytes",
	"2k bytes",
	"4k bytes",
	"8k bytes",
	0
};

#define ALIGN_SHIFT 20

//
// Align Info: szSectionAlign[(Sect->Characteristics & IMAGE_SCN_ALIGN_MASK) >> ALIGN_SHIFT]
//

//
// Get COFF symbol pointers
//

bool CoffGetPointers (
	 IN PIMAGE_FILE_HEADER FileHdr,
	OUT PIMAGE_SYMBOL *ppSymTbl,
	OUT size_t *pcSyms OPTIONAL,
	OUT PCOFF_STRING_TABLE *ppST OPTIONAL
	)
{
	PIMAGE_SYMBOL SymbolTable = NULL;
	PCOFF_STRING_TABLE pST = 0;
	size_t cSyms = 0;

	// Get symbol table
	if (FileHdr->NumberOfSymbols && FileHdr->PointerToSymbolTable)
	{
		cSyms = FileHdr->NumberOfSymbols;
		SymbolTable = (PIMAGE_SYMBOL)((PSZ)FileHdr + FileHdr->PointerToSymbolTable);
		pST = (PCOFF_STRING_TABLE)(&SymbolTable[cSyms]);
	}

	*ppSymTbl = SymbolTable;
	if (pcSyms)
		*pcSyms = cSyms;
	if (ppST)
		*ppST = pST;

	return (SymbolTable != NULL);
}

//
// Dump aux symbols
//

void CoffDumpAuxSymbols (PIMAGE_FILE_HEADER FileHdr, PIMAGE_SYMBOL Sym)
{
	PIMAGE_SYMBOL SymbolTable;
	size_t cSyms;
	PCOFF_STRING_TABLE pST = 0;
	assert (CoffGetPointers (FileHdr, &SymbolTable, &cSyms, &pST));

	PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER)(FileHdr+1);

	PIMAGE_AUX_SYMBOL AuxSym = CoffGetAuxSymbols (Sym);
	if (!AuxSym)
		return;

	size_t cAuxSyms = Sym->NumberOfAuxSymbols;

	printf("    ");

	switch (Sym->StorageClass)
	{
	case IMAGE_SYM_CLASS_EXTERNAL:
		if ((Sym->Type & 0x20) // T_FUNCTION
			&& Sym->SectionNumber > 0)
		{
			printf("[Func Def] ");

			printf("#.bf-symrec %8x, CodeSize %8x, #LineNumber %8x, #NextFunc %8x\n",
				AuxSym->Sym.TagIndex,
				AuxSym->Sym.Misc.TotalSize,
				AuxSym->Sym.FcnAry.Function.PointerToLinenumber,
				AuxSym->Sym.FcnAry.Function.PointerToNextFunction
				);
		}
		else if (Sym->SectionNumber == IMAGE_SYM_UNDEFINED && Sym->Value == 0)
		{
			printf("[Weak External] ");

			static const char* szWeakExternSearchTypes[] = {
				0,
				"No library search for sym1",
				"Library search for sym1",
				"sym1 is an alias for sym2"
			};

			DWORD Type = ((PIMAGE_AUX_SYMBOL_EX)AuxSym)->Sym.WeakSearchType;

			printf("#symrec2 %8x, Flags: %s\n", AuxSym->Sym.TagIndex,
				(Type > 3) ? "<unknown-search-flags>" : szWeakExternSearchTypes[Type]
				);
		}
		else
		{
			printf("[type unrecognized]\n");
		}
		break;
	
	case IMAGE_SYM_CLASS_FUNCTION:
		{
			char* Name = CoffSTGetSymbolName (pST, Sym);
			if (!_stricmp (Name, ".bf") || !_stricmp (Name, ".ef"))
			{
				printf("[%s funcsym] ", Name);

				printf("LineNumber: %8x", AuxSym->Sym.Misc.LnSz.Linenumber);

				if (!_stricmp (Name, ".bf"))
				{
					printf(", #NextFunction: %8x", AuxSym->Sym.FcnAry.Function.PointerToNextFunction);
				}

				printf("\n");
			}
			else
			{
				printf("[type unrecognized]\n");
			}
			FreeSymName (Name);
		}
		break;

	case IMAGE_SYM_CLASS_FILE:
		{
			char* Name = strndup ((char*)AuxSym->File.Name, IMAGE_SIZEOF_SYMBOL);
			printf("[File] %s\n", Name);
			free (Name);
		}
		break;

	case IMAGE_SYM_CLASS_STATIC:
		if (Sym->Value == 0 && Sym->SectionNumber > 0)
		{
			printf("[Section] Length %8x, #relocs: %8x, #LineNums: %8x",
				AuxSym->Section.Length,
				AuxSym->Section.NumberOfRelocations,
				AuxSym->Section.NumberOfLinenumbers);

			if ((Sections[Sym->SectionNumber-1].Characteristics & IMAGE_SCN_LNK_COMDAT) && Sym->Type == IMAGE_SYM_TYPE_NULL)
			{
				printf("; COMDAT: CheckSum: %8x ", AuxSym->Section.CheckSum);

				static const char* szComdatSelectionTypes[] = {
					"<unknown-0x00>",
					"no duplicates",
					"any",
					"same size",
					"exact match",
					"associative",
					"largest",
					"newest"
				};

				printf("(");

				if (AuxSym->Section.Selection <= IMAGE_COMDAT_SELECT_NEWEST)
				{
					printf("%s", szComdatSelectionTypes[AuxSym->Section.Selection]);

					if (AuxSym->Section.Selection == IMAGE_COMDAT_SELECT_ASSOCIATIVE)
					{
						printf(" for section #%lx", AuxSym->Section.Number);

						unsigned iSect = AuxSym->Section.Number-1;
						if (iSect < FileHdr->NumberOfSections)
						{
							char* Name = strndup ((char*)Sections[iSect].Name, IMAGE_SIZEOF_SHORT_NAME);
							printf(" name '%s'", Name);
							free (Name);
						}

					} // if (associative)

				} // if (valid COMDAT selection)

				printf(")");
			
			} // if (COMDAT)

			printf("\n");
		
		} // if (section)
		else
		{
			printf("[type unrecognized]\n");
		}
		break;

	default:
		printf("[class unrecognized]\n");
		break;
	}
}

void CoffGetSzSectionNumber (SHORT SectionNumber, OUT char* pBuf)
{
	static const char* szHexDigits = "0123456789ABCDEF";

	if (SectionNumber < 16)
		*(pBuf++) = szHexDigits[(SectionNumber & 0xf)];

	else if (SectionNumber < 256)
	{
		*(pBuf++) = szHexDigits[((SectionNumber >> 4) & 0xf)];
		*(pBuf++) = szHexDigits[(SectionNumber & 0xf)];
	}
	else if (SectionNumber < 4096)
	{
		*(pBuf++) = szHexDigits[((SectionNumber >> 8) & 0xf)];
		*(pBuf++) = szHexDigits[((SectionNumber >> 4) & 0xf)];
		*(pBuf++) = szHexDigits[(SectionNumber & 0xf)];
	}
	else
	{
		*(pBuf++) = szHexDigits[((SectionNumber >> 12) & 0xf)];
		*(pBuf++) = szHexDigits[((SectionNumber >> 8) & 0xf)];
		*(pBuf++) = szHexDigits[((SectionNumber >> 4) & 0xf)];
		*(pBuf++) = szHexDigits[(SectionNumber & 0xf)];
	}

	*pBuf = 0;
}

//
// Basic symbol types
//
static const char* szSymBasicTypes[] = {
	"notype",
	"void",
	"char",
	"short",
	"int",
	"long",
	"float",
	"double",
	"struct",
	"union",
	"enum",
	"moe",
	"byte",
	"word",
	"uint",
	"dword"
};

// Derived types
static const char* szSymDerivedTypes[] = {
	"   ",
	"*  ",
	"( )",
	"[ ]"
};

// Symbol storage classes
static const char* szSymStorageClasses[] = {
	"Null",
	"Auto",
	"External",
	"Static",
	"Register",
	"ExternalDef",
	"Label",
	"UndefLabel",
	"MemberOfStruct",
	"Argument",
	"StructTag",
	"MemberOfUnion",
	"UnionTag",
	"TypeDefinition",
	"UndefinedStatic",
	"EnumTag",
	"MemberOfEnum",
	"RegisterParam",
	"BitField",
};

void CoffDumpSymbolType (
	PIMAGE_SYMBOL Sym
	)
{
	WORD Type = (Sym->Type & ~IMAGE_SYM_TYPE_PCODE);
	WORD BasicType = BTYPE(Type);
	WORD SpecialType = ((Type & N_TMASK) >> N_BTSHFT);
	bool bPCode = !!(Sym->Type & IMAGE_SYM_TYPE_PCODE);

	if (bPCode) printf("(*) ");
	printf("%-8s %s ", szSymBasicTypes[BasicType], szSymDerivedTypes[SpecialType]);
}

PCSZ CoffGetSymStorageClass (
	PIMAGE_SYMBOL Sym
	)
{
	if (Sym->StorageClass == IMAGE_SYM_CLASS_END_OF_FUNCTION)
		return "EndOfFunction";

	if (Sym->StorageClass <= IMAGE_SYM_CLASS_BIT_FIELD)
		return szSymStorageClasses[Sym->StorageClass];

	if (Sym->StorageClass == IMAGE_SYM_CLASS_FAR_EXTERNAL)
		return "FarExternal";

	if (Sym->StorageClass >= 0x64 && Sym->StorageClass <= 0x6b)
	{
		static const char* szExtClass[] = {
			"Block",
			"Function",
			"EndOfStruct",
			"File",
			"Section",
			"WeakExternal",
			"Unknown-0x6a",
			"CLR-token"
		};
		return szExtClass[Sym->StorageClass - 0x64];
	}

	return "Unknown";
}

//
// Dump COFF symbol
//

void CoffDumpSymbol (
	PIMAGE_FILE_HEADER FileHdr,
	PIMAGE_SYMBOL Sym,
	unsigned iSym,
	PCOFF_STRING_TABLE pST
	)
{
	char* Name = CoffSTGetSymbolName (pST, Sym);
	char *szSectCode = "UNKNOWN";
	char szSectIndex[4+4+1] = {'S', 'E', 'C', 'T', ' ', ' ', ' ', ' ', 0};

	if (Sym->SectionNumber > 0)
	{
		CoffGetSzSectionNumber (Sym->SectionNumber, &szSectIndex[4]);
		szSectCode = &szSectIndex[0];
	}
	else if (Sym->SectionNumber == IMAGE_SYM_UNDEFINED)
		szSectCode = "UNDEF";
	else if (Sym->SectionNumber == IMAGE_SYM_DEBUG)
		szSectCode = "DEBUG";
	else if (Sym->SectionNumber == IMAGE_SYM_ABSOLUTE)
		szSectCode = "ABS";

	printf("%8x  %08x  %-8s ", iSym, Sym->Value, szSectCode);
	CoffDumpSymbolType (Sym);
	printf("%-15s ", CoffGetSymStorageClass (Sym));
	printf("|  %s\n", Name);

	CoffDumpAuxSymbols (FileHdr, Sym);

	FreeSymName (Name);
}

//
// Dump Symbol Table

void CoffDumpSymbolTable (
	PIMAGE_FILE_HEADER FileHdr
	)
{
	PIMAGE_SYMBOL SymbolTable = 0;
	size_t cSyms = 0;
	PCOFF_STRING_TABLE pST = 0;
	assert (CoffGetPointers (FileHdr, &SymbolTable, &cSyms, &pST));

	printf ("COFF SYMBOL TABLE\n");
	for (unsigned i=0; i<cSyms; i++)
	{
		PIMAGE_SYMBOL Sym = &SymbolTable[i];
		
		CoffDumpSymbol (FileHdr, Sym, i, pST);

		if (Sym->NumberOfAuxSymbols)
			i += Sym->NumberOfAuxSymbols;
	}
	printf("\n");
}

//
// Dump COFF section information
//

void CoffDumpSectionFlags (
	PIMAGE_FILE_HEADER FileHdr,
	unsigned iSect,
	PIMAGE_SECTION_HEADER Sect
	)
{
	PIMAGE_SYMBOL SymbolTable = NULL;
	PCOFF_STRING_TABLE pST = 0;
	size_t cSyms = 0;

	assert (CoffGetPointers (FileHdr, &SymbolTable, &cSyms, &pST) != false);

	ULONG ci = Sect->Characteristics & 0xf0;        // content information
	ULONG li = Sect->Characteristics & 0x3f00;      // linker information
	ULONG oi = Sect->Characteristics & 0xFF000000;  // options
	ULONG ai = Sect->Characteristics & IMAGE_SCN_ALIGN_MASK; // alignment information

	if (ci & IMAGE_SCN_CNT_CODE)
		printf("         Code\n");
	if (ci & IMAGE_SCN_CNT_INITIALIZED_DATA)
		printf("         Data\n");
	if (ci & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
		printf("         Uninit Data\n");

	if (li & IMAGE_SCN_LNK_INFO)
		printf("         Link info\n");
	if (li & IMAGE_SCN_LNK_REMOVE)
		printf("         Linker Removeable\n");
	if (li & IMAGE_SCN_LNK_COMDAT)
	{
		printf("         COMDAT ");

		PIMAGE_SYMBOL SectSym = 0;
		PIMAGE_SYMBOL ComdatSym = CoffFindComdatSymrecForSection (SymbolTable, cSyms, iSect+1, &SectSym);

		if (!ComdatSym)
			printf("(no symbol)");
		else
		{
			PSZ szComdatSymName = CoffSTGetSymbolName (pST, ComdatSym);
			printf("(%s)", szComdatSymName);
			FreeSymName (szComdatSymName);
		}
		printf("\n");
	}

	if (oi & IMAGE_SCN_MEM_DISCARDABLE)
		printf("         Discardable\n");
	if (oi & IMAGE_SCN_MEM_NOT_CACHED)
		printf("         Not cached\n");
	if (oi & IMAGE_SCN_MEM_NOT_PAGED)
		printf("         Not paged\n");
	if (oi & IMAGE_SCN_MEM_SHARED)
		printf("         Shared Section\n");
	if (oi & IMAGE_SCN_MEM_EXECUTE)
		printf("         Executable\n");
	if (oi & IMAGE_SCN_MEM_READ)
		printf("         Readable\n");
	if (oi & IMAGE_SCN_MEM_WRITE)
		printf("         Writeable\n");
	if (oi & IMAGE_SCN_LNK_NRELOC_OVFL)
		printf("         Extended Relocations\n");
	if (oi & IMAGE_SCN_SCALE_INDEX)
		printf("         TLS index is scaled\n");

	if (ai != 0)
		printf("         Alignment: %s\n", szSectionAlign[ai >> ALIGN_SHIFT]);
}

static const char* szRelocTypes[] = {
	/* 00 */ "ABSOLUTE",
	/* 01 */ "DIR16",
	/* 02 */ "REL16",
	/* 03 */ "UnkRel-3",
	/* 04 */ "UnkRel-4",
	/* 05 */ "UnkRel-5",
	/* 06 */ "DIR32",
	/* 07 */ "DIR32NB",
	/* 08 */ "UnkRel-8",
	/* 09 */ "SEG12",
	/* 0A */ "SECTION",
	/* 0B */ "SECREL",
	/* 0C */ "TOKEN",
	/* 0D */ "SECREL7",
	/* 0E */ "UnkRel-E",
	/* 0F */ "UnkRel-F",
	/* 10 */ "UnkRel-10",
	/* 11 */ "UnkRel-11",
	/* 12 */ "UnkRel-12",
	/* 13 */ "UnkRel-13",
	/* 14 */ "REL32",
};

// Dump COFF Relocation
void CoffDumpRelocation (PIMAGE_FILE_HEADER FileHdr, PIMAGE_SECTION_HEADER Sect, unsigned iReloc, PIMAGE_RELOCATION Reloc)
{
	printf("%08x  %10s  %08x  %-8x  ",
		iReloc * sizeof(IMAGE_RELOCATION),
		Reloc->Type <= 0x14 ? szRelocTypes[Reloc->Type] : "UnkRel",
		Reloc->VirtualAddress,
		Reloc->SymbolTableIndex
		);

	PIMAGE_SYMBOL SymbolTable = NULL;
	PCOFF_STRING_TABLE pST = 0;
	size_t cSyms = 0;

	assert (CoffGetPointers (FileHdr, &SymbolTable, &cSyms, &pST) != false);

	PIMAGE_SYMBOL Sym = &SymbolTable[Reloc->SymbolTableIndex];
	char* Name = CoffSTGetSymbolName (pST, Sym);

	printf("%s\n", Name);

	FreeSymName (Name);
}

// Dump COFF Relocations
void CoffDumpRelocations (PIMAGE_FILE_HEADER FileHdr, PIMAGE_SECTION_HEADER Sect)
{
	if (Sect->NumberOfRelocations && Sect->PointerToRelocations)
	{
		PIMAGE_RELOCATION Relocations = (PIMAGE_RELOCATION)((PSZ)FileHdr + Sect->PointerToRelocations);
		size_t cRelocs = Sect->NumberOfRelocations;

		for (unsigned i=0; i<cRelocs; i++)
		{
			CoffDumpRelocation (FileHdr, Sect, i, &Relocations[i]);
		}
	}
}

//
// Dump COFF Section Table
//

void CoffDumpSections (PIMAGE_FILE_HEADER FileHdr)
{
	PIMAGE_SYMBOL SymbolTable = NULL;
	PCOFF_STRING_TABLE pST = 0;
	size_t cSyms = 0;

	assert (CoffGetPointers (FileHdr, &SymbolTable, &cSyms, &pST) != false);

	PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER)(FileHdr+1);
	size_t cSects = FileHdr->NumberOfSections;

	printf("COFF Sections\n"
		"-------------\n\n");

	for (unsigned i=0; i<cSects; i++)
	{
		PIMAGE_SECTION_HEADER Sect = &Sections[i];
		char* Name = strndup ((char*)&Sect->Name[0], IMAGE_SIZEOF_SHORT_NAME);
		printf("SECTION HEADER #%x\n", i);
		printf("%s Name\n", Name);
		printf("%8x physical address\n", Sect->Misc.PhysicalAddress);
		printf("%8x virtual address\n", Sect->VirtualAddress);
		printf("%8x size of raw data\n", Sect->SizeOfRawData);
		printf("%8x pointer to raw data\n", Sect->PointerToRawData);
		printf("%8x pointer to relocations\n", Sect->PointerToRelocations);
		printf("%8x pointer to line numbers\n", Sect->PointerToLinenumbers);
		printf("    %8x number of relocations\n", Sect->NumberOfRelocations);
		printf("    %8x number of line numbers\n", Sect->NumberOfLinenumbers);
		printf("%8x flags\n", Sect->Characteristics);

		CoffDumpSectionFlags (FileHdr, i, Sect);

		printf("\n");

		printf("COFF RELOCATIONS\n");

		CoffDumpRelocations (FileHdr, Sect);

// 		printf(" Symbols for this section:\n");
// 
// 		PIMAGE_SYMBOL Sym = CoffFirstSymbolForSection (SymbolTable, cSyms, i+1);
// 		if (Sym != NULL)
// 		{
// 			// Enum all symbols for this section
// 			for ( ; Sym != NULL; Sym = CoffNextSymbolForSection (SymbolTable, cSyms, i+1, Sym))
// 			{
// 			}
// 		}
	}
}

// Dump SYMBOL_VECTOR
void CoffDumpSymsVector (SYMBOL_VECTOR &syms, PCOFF_STRING_TABLE pST)
{
	for (unsigned i=0; i<syms.size(); i++)
	{
		PIMAGE_SYMBOL Sym = &syms[i];
		PSZ szSymName = CoffSTGetSymbolName (pST, Sym);
		if (szSymName)
		{
			printf(" [%02d] = %08x = %s\n", i, Sym->Value, szSymName);

			FreeSymName (szSymName);
		}

		// Process aux symbols
		for (unsigned iAux = 0; iAux < Sym->NumberOfAuxSymbols; iAux++)
		{
			printf("        aux symbol #%d\n", iAux);
		}

		i += Sym->NumberOfAuxSymbols;
	}
}

// Build COFF Symbol Table from std::vector<IMAGE_SYMBOL>
PIMAGE_SYMBOL CoffBuildSymbolTable (SYMBOL_VECTOR &syms)
{
	size_t cSyms = syms.size();
	size_t cbSyms = cSyms * sizeof(IMAGE_SYMBOL);

	PIMAGE_SYMBOL Syms = (PIMAGE_SYMBOL) malloc (cbSyms);
	if (Syms != NULL)
	{
		for (unsigned i=0; i<cSyms; i++)
			Syms[i] = syms[i];
	}
	return Syms;
}
