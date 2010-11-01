#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <assert.h>
#include <common/mapping.h>
#include "fasmsymfmt.h"
#include "converter.h"
#include "coffsyms.h"
#include "fasmsyms.h"
#include "fasm2coff.h"
#include "helper.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// COFF Symbols Management for FASM->COFF Conversion Routines.

// Remember all COFF relocations of all sections in vector<vector<IMAGE_RELOCATION>>
static void BackupCoffRelocs (PIMAGE_FILE_HEADER FileHdr, COFF_RELOCS &CoffRelocs)
{
	PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER)(FileHdr + 1);

	PIMAGE_SYMBOL SymTbl = (PIMAGE_SYMBOL)((PCHAR)FileHdr + FileHdr->PointerToSymbolTable);
	PCOFF_STRING_TABLE pST = (PCOFF_STRING_TABLE)(&SymTbl[FileHdr->NumberOfSymbols]);

	CoffRelocs.clear();

	for (unsigned i=0; i<FileHdr->NumberOfSections; i++)
	{
		PIMAGE_SECTION_HEADER Section = &Sections[i];
		PIMAGE_RELOCATION Relocs = (PIMAGE_RELOCATION)((PSZ)FileHdr + Section->PointerToRelocations);

		SECTION_RELOCS SectRelocs;

		for (unsigned j=0; j<Section->NumberOfRelocations; j++)
		{
			SectRelocs.push_back (Relocs[j]);
		}

		CoffRelocs.push_back (SectRelocs);
	}
}

// Restore COFF relocations (update SymbolTableIndex 'cause they have been changed) from vector<vector<IMAGE_RELOCATION>>
static bool RestoreCoffRelocs (
	HANDLE hFile, 
	PIMAGE_FILE_HEADER FileHdr,
	COFF_RELOCS &CoffRelocs,
	SYMBOL_VECTOR &OrigSymbolTable, 
	PCOFF_STRING_TABLE pOrigST,
	SYMBOL_VECTOR &SymTbl,
	PCOFF_STRING_TABLE pST
	)
{
	PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER)(FileHdr + 1);

	// For each section
	for (unsigned i=0; i<FileHdr->NumberOfSections; i++)
	{
		PIMAGE_SECTION_HEADER Section = &Sections[i];
		SECTION_RELOCS *SectRelocs = &CoffRelocs[i];  // get relocs

		if (!SectRelocs->empty())
		{
			SetFilePointer (hFile, Section->PointerToRelocations, 0, FILE_BEGIN);

			// For each reloc
			for (unsigned j=0; j<SectRelocs->size(); j++)
			{
				IMAGE_RELOCATION Rel = (*SectRelocs)[j];
				PIMAGE_SYMBOL Sym = &OrigSymbolTable[Rel.SymbolTableIndex];
				PSZ szSrcName = CoffSTGetSymbolName (pOrigST, Sym);
				unsigned iMatchFound = -1;

				if (szSrcName != NULL)
				{
					for (unsigned k=0; k<SymTbl.size() && iMatchFound == -1; k += 1 + SymTbl[k].NumberOfAuxSymbols)
					{
						PIMAGE_SYMBOL TestSym = &SymTbl[k];
						PSZ szTestName = CoffSTGetSymbolName (pST, TestSym);

						if (szTestName)
						{
							if (!strcmp (szTestName, szSrcName))
							{
// 								printf(" Found match for symbol '%s' : old iSym %d new iSym %d\n", szSrcName, Rel.SymbolTableIndex, k);
								iMatchFound = k;
							}

							FreeSymName (szTestName);

						} // if (reloc has name)

					} // for (new symbols)

					if (iMatchFound == -1)
						printf("Error: can't find match for relocation #%d in section #%d, symbol name: %s\n", j, i, szSrcName);

					FreeSymName (szSrcName);

				} // if (reloc has name)

				if (iMatchFound == -1)
					return false;

				Rel.SymbolTableIndex = iMatchFound;

				WriteBuff (hFile, &Rel, sizeof(IMAGE_RELOCATION));

			} // for (relocs)

			SectRelocs->clear();

		} // if (!SectRelocs->empty())

	} // for (sections)

	CoffRelocs.clear();

	return true;
}

// Merge two symbol sets into one
static bool CoffMergeSymbols (
	 IN SYMBOL_VECTOR &syms1,
	 IN PCOFF_STRING_TABLE pST1,
	 IN SYMBOL_VECTOR &syms2,
	 IN PCOFF_STRING_TABLE pST2,
	OUT SYMBOL_VECTOR &syms,
	OUT PCOFF_STRING_TABLE *ppST
	)
{
	size_t cSyms1 = syms1.size();
	size_t cSyms2 = syms2.size();
	size_t cSyms = cSyms1 + cSyms2;
	std::vector<size_t> offsets;

	syms.reserve (cSyms);
	offsets.reserve (cSyms);

	size_t cbST = 0;
	unsigned i;

	// Append syms1 to syms
	for (i=0; i<cSyms1; i++)
	{
		IMAGE_SYMBOL Sym = {0};
		PIMAGE_SYMBOL SrcSym = &syms1[i];

		Sym = *SrcSym;
		
		if (CoffIsSymNameLong (SrcSym))
		{
			PSZ szSymName = CoffSTGetSymbolName (pST1, SrcSym);
			Sym.N.Name.Long = cbST + sizeof(DWORD);
			cbST += strlen (szSymName) + 1;
			FreeSymName (szSymName);
		}

		syms.push_back (Sym);

		// Copy aux symbols for this symbol too.
		for (unsigned iAux = 0; iAux < SrcSym->NumberOfAuxSymbols; iAux++)
		{
			syms.push_back (syms1[i + iAux + 1]);
		}

		i += SrcSym->NumberOfAuxSymbols;
	}

	// Append syms2 to syms
	for (unsigned j=0; j < cSyms2; i++,j++)
	{
		IMAGE_SYMBOL Sym = {0};
		PIMAGE_SYMBOL SrcSym = &syms2[j];

		Sym = *SrcSym;

		if (CoffIsSymNameLong (SrcSym))
		{
			PSZ szSymName = CoffSTGetSymbolName (pST2, SrcSym);
			Sym.N.Name.Long = cbST + sizeof(DWORD);
			cbST += strlen (szSymName) + 1;
		}

		syms.push_back (Sym);

		// Copy aux symbols for this symbol too.
		for (unsigned iAux = 0; iAux < SrcSym->NumberOfAuxSymbols; iAux++)
		{
			syms.push_back (syms2[j + iAux + 1]);
		}

		j += SrcSym->NumberOfAuxSymbols;
	}

	PCOFF_STRING_TABLE pST = STCreate (cbST);

	// Add syms1+syms2 names to string table.
	for (i = 0; i < cSyms; i++)
	{
		PIMAGE_SYMBOL Sym = &syms[i];

		if (CoffIsSymNameLong (Sym))
		{
			PCOFF_STRING_TABLE pSrcST = i < cSyms1 ? pST1 : pST2;
			PSZ szSymName = CoffSTGetSymbolName (pSrcST, Sym);
			strcpy (STGetStr(pST, Sym->N.Name.Long), szSymName);
			FreeSymName (szSymName);
		}

		if (Sym->NumberOfAuxSymbols)
			i += Sym->NumberOfAuxSymbols;
	}

	*ppST = pST;
	return true;
}


////////////////////////////////////////////////////////////////////////////
// Do FASM->COFF conversion

//
// Convert FASM Symbol File into MS COFF Symbols and inject them into the specified COFF .obj
//

int FasmSym2CoffObj (char* szSymFile, char* szCoffFile)
{
	//
	// Map input FASM Symbol File, parse headers.
	//

	SIZE_T Size = 0;
	LPVOID lpSym = MapExistingFile (szSymFile, MAP_READ, 0, &Size);
	if (lpSym == NULL)
		return printf("Error: can't open input file\n");

	// Vector of fasm symbols to be converted to COFF
	FASMSYMPTR_VECTOR fasmSymbols;

	printf("FASM Symbol File mapped at %p size 0x%lx\n", lpSym, Size);
	PFASM_SYMHDR Hdr = (PFASM_SYMHDR) lpSym;
	int res = FasmCheckSymbolsHdr (Hdr, Size);
	if (res != 0)
		return res;

	//
	// Map input obj and extract currect COFF symbols
	//

	// Backups of symbol table and string table from the COFF
	SYMBOL_VECTOR OriginalSymbolTable;
	PCOFF_STRING_TABLE pOriginalST = NULL;

	SIZE_T ObjSize = 0;
	LPVOID lpObj = MapExistingFile (szCoffFile, MAP_READ, 0, &ObjSize);
	if (lpObj == NULL)
		return printf("Error: can't open output file\n");

	printf("Mapped input OBJ at %p size 0x%lx\n", lpObj, ObjSize);

	PIMAGE_FILE_HEADER FileHdr = (PIMAGE_FILE_HEADER)(lpObj);
	printf(" IMAGE_FILE_HEADER\n");
	printf("  PointerToSymbolTable: %08x\n", FileHdr->PointerToSymbolTable);
	printf("  NumberOfSymbols: %08x\n", FileHdr->NumberOfSymbols);
	printf("\n");

	//
	// Load symbol table
	//

	PIMAGE_SYMBOL FileSymbolTable = 0;
	PCOFF_STRING_TABLE FileST = 0;
	size_t cFileSyms = 0;

	if (!CoffGetPointers (FileHdr, &FileSymbolTable, &cFileSyms, &FileST))
	{
		return printf("Error: no symbol information in the source obj\n");
	}

	if ((PSZ)&FileSymbolTable[cFileSyms] + FileST->cbST != (PSZ)FileHdr + ObjSize)
	{
		return printf("Error: coff symbols end not at the end of the file\n");
	}

	// Backup all symbol records
	for (unsigned i=0; i<cFileSyms; i++)
		OriginalSymbolTable.push_back (FileSymbolTable[i]);

	// Backup string table
	pOriginalST = STDuplicate (FileST);

	//
	// Dump some information from input obj
	//

#ifdef _DEBUG

// 	CoffDumpSections (FileHdr);
// 
// 	CoffDumpSymbolTable (FileHdr);

#endif

	//
	// Make backup for relocs from all sections
	//

	COFF_RELOCS CoffRelocs;
	BackupCoffRelocs (FileHdr, CoffRelocs);

	// Extract symbols from FASM Symbols File
	FasmExtractSymbolsToConvert (Hdr, fasmSymbols);

	// Dump extracted vector of FASM symbols
	printf("Extracted FASM symbols to save in OBJ (cSyms %08x)\n", fasmSymbols.size());
	FasmDumpSymsVector (Hdr, fasmSymbols);

	SYMBOL_VECTOR nonUnique;

	// Extract COFF symbols from original table to leave them there.
	SYMBOL_VECTOR UniqueOriginalSymbols;
	Fasm2CoffExtractUniqueSymbols (
		OriginalSymbolTable,
		pOriginalST,
		UniqueOriginalSymbols,
		Hdr,
		fasmSymbols,
		nonUnique
		);

	STRING_VECTOR Publics;

	//
	// Calc publics
	//

	for (unsigned i=0; i<nonUnique.size(); i++)
	{
		PIMAGE_SYMBOL Sym = &nonUnique[i];
		if (Sym->StorageClass == IMAGE_SYM_CLASS_EXTERNAL)
		{
			char* Name = CoffSTGetSymbolName (pOriginalST, Sym);
			Publics.push_back (Name);
			FreeSymName (Name);
		}

		i += Sym->NumberOfAuxSymbols;
	}

	// Dump extracted vector of COFF unique
	printf("Extracted old unique COFF symbols and remembered (cSyms %08x)\n", UniqueOriginalSymbols.size());
	CoffDumpSymsVector (UniqueOriginalSymbols, pOriginalST);

// 	printf("Non-unique symbols:\n");
// 	CoffDumpSymsVector (nonUnique, pOriginalST);

 	nonUnique.clear();

	printf("Publics:\n");
	for (unsigned i=0; i<Publics.size(); i++)
		printf(" [%d] = %s\n", i, Publics[i].c_str());

	//
	// Search UniqueSymbols for sections,
	//  add aux symbol for each section definition
	//

	Fasm2CoffProcessSectionNames (
		FileHdr,
		UniqueOriginalSymbols,
		pOriginalST
		);

	bool bRes;

#define SPECIAL_SYMBOLS 1

#if SPECIAL_SYMBOLS

	//
	// Special symbols
	//

	SYMBOL_VECTOR SpecialSymbols;
	STRING_VECTOR SpecialStringMap;
	OFFSET_VECTOR SpecialSTOffsets;

	IMAGE_SYMBOL Symbol = {0};
	IMAGE_AUX_SYMBOL AuxSymbol = {0};

	Symbol.Value = 0;
	Symbol.SectionNumber = IMAGE_SYM_DEBUG;
	Symbol.Type = IMAGE_SYM_TYPE_NULL;
	Symbol.StorageClass = IMAGE_SYM_CLASS_FILE;
	Symbol.NumberOfAuxSymbols = 1;

	PSZ InputFile = FASM_STRING_TABLE(Hdr) + Hdr->dwInputFileNameOffset;
	strncpy ((char*)AuxSymbol.File.Name, InputFile, IMAGE_SIZEOF_SYMBOL);

 	SpecialSymbols.push_back (Symbol);
	SpecialStringMap.push_back (".file");

	SpecialSymbols.push_back (*(PIMAGE_SYMBOL)&AuxSymbol);
	SpecialStringMap.push_back ("");

	PCOFF_STRING_TABLE pSpecialST = STFromStringVector (
		SpecialStringMap,
		SpecialSTOffsets
		);

	if (pSpecialST == EMPTY_STRING_TABLE)
	{
		// All symbols have names shorter 8 symbols, so string table is empty.
	}
	else if (!pSpecialST)
		return printf("Error: can't create string table for special symbols\n");

	bRes = CoffBuildSymbolsFromStringMapAndOffsetMap (
		SpecialSymbols,
		SpecialStringMap,
		SpecialSTOffsets
		);

	if (!bRes)
		return printf("Can't build special symbols\n");

	////

	SYMBOL_VECTOR CoffInputSyms;
	PCOFF_STRING_TABLE pInputST = 0;

	bRes = CoffMergeSymbols (
		SpecialSymbols,
		pSpecialST,
		UniqueOriginalSymbols,
		pOriginalST,
		CoffInputSyms,
		&pInputST
		);
	if (!bRes)
		return printf("Can't merge unique original symbols + special symbols\n");

#endif

	printf("\nConverting FASM->COFF\n");

	// Convert fasm symbols
	SYMBOL_VECTOR fasmConvertedSyms;
	PCOFF_STRING_TABLE pFasmST = 0;

	bRes = FasmConvertSymbolsToCoff (
		Hdr,
		fasmSymbols,
		Publics,
		fasmConvertedSyms,
		&pFasmST
		);

	if (bRes == false)
		return printf("error: fasm->coff conversion failed\n");

	printf("FASM symbols converted to COFF format\n");
// 	CoffDumpSymsVector (fasmConvertedSyms, pFasmST);
	
	printf("\n");

	//
	// Merge FASM converted symbols with unique COFF extracted symbols.
	//

	printf("Merging FASM symbols with unique COFF symbols\n");

	SYMBOL_VECTOR newSyms;
	PCOFF_STRING_TABLE pNewST = 0;

	bRes = CoffMergeSymbols (
#if SPECIAL_SYMBOLS
		CoffInputSyms,
		pInputST,
#else
		UniqueOriginalSymbols,
		pOriginalST,
#endif
		fasmConvertedSyms,
		pFasmST,
		newSyms,
		&pNewST
		);

	if (!bRes)
		return printf("error: can't merge coff symbols\n");

	// String table ready.
	MEMBLOCK ConvertedStringTable = {pNewST, pNewST->cbST};

	//
	// Finally, build COFF Symbol Table
	//

	printf("Converted FASM symbols merged with original unique COFF symbols\n");
	CoffDumpSymsVector (newSyms, pNewST);

	// No longer need converted syms & ST
	fasmConvertedSyms.clear();
	STDestroy (pFasmST);

	size_t cNewSyms = newSyms.size();

	printf("\n");

	printf("Building COFF Symbol Table\n");

	//
	// Generate COFF symbolic information from SYMBOL_VECTOR newSyms
	//

	PIMAGE_SYMBOL NewSymTbl = CoffBuildSymbolTable (newSyms);
	if (NewSymTbl == NULL)
		return printf("Error: building COFF symbol table failed\n");

	// Symbol table ready.
	MEMBLOCK ConvertedSymbolsTable = {NewSymTbl, cNewSyms * sizeof(IMAGE_SYMBOL)};

	/////////////////////////////
	// Write to file

	char szOutputFile[MAX_PATH];
	strcpy (szOutputFile, szCoffFile);
	char* pExt = strrchr (szOutputFile, '.');
	if (pExt == NULL)
		strcat (szOutputFile, "_syms.obj");
	else
		strcpy (pExt, "_syms.obj");

	printf("Suggested output file name: %s\n", szOutputFile);

	// Open output for writing
	HANDLE hFile = CreateFile (szOutputFile, GENERIC_WRITE, FILE_SHARE_READ,
		0, CREATE_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return printf("Error: can't open output file for writing\n");

	printf("Opened output file for writing\n");

	// Create new file header
	IMAGE_FILE_HEADER NewFileHdr = *FileHdr;
	if (FileHdr->PointerToSymbolTable == 0 || FileHdr->NumberOfSymbols == 0)
	{
		NewFileHdr.PointerToSymbolTable = Size;
		NewFileHdr.NumberOfSymbols = cNewSyms;
		printf("Created output IMAGE_FILE_HEADER::PointerToSymbolTable\n");
	}
	else
	{
		NewFileHdr.NumberOfSymbols = cNewSyms;
		printf("Updated output IMAGE_FILE_HEADER::NumberOfSymbols (%08x)\n", cNewSyms);
	}

	DWORD dwWritten = 0;

	// Write new file header
	WriteBuff (hFile, &NewFileHdr, sizeof(IMAGE_FILE_HEADER));

	size_t cbData = NewFileHdr.PointerToSymbolTable - sizeof(IMAGE_FILE_HEADER);
	void* pData = FileHdr + 1;

	// Write all file contents except symbol table
	WriteBuff (hFile, pData, cbData);

	printf("File contents written; Restoring COFF Relocations\n");

	//
	// Restore COFF Relocs from the backup
	//

	bRes = RestoreCoffRelocs (
		hFile,
		FileHdr,
		CoffRelocs,
		OriginalSymbolTable,
		pOriginalST,
		newSyms,
		pNewST
		);

	if (!bRes)
		return printf("Error: failed to restore sections' relocs\n");

	printf("COFF Relocations restored, writing the rest of the file\n");

	// Go to the start of the old COFF symbol table
	SetFilePointer (hFile, NewFileHdr.PointerToSymbolTable, 0, FILE_BEGIN);

	// Write symbol table
	WriteBlock (hFile, ConvertedSymbolsTable);

	// Write string table
	WriteBlock (hFile, ConvertedStringTable);

	printf("COFF symbolic information written\n");

	// Close the file
	CloseHandle (hFile);

	// Unmap output file
	UnmapViewOfFile (lpObj);

	printf("\n\n");

	lpObj = MapExistingFile (szOutputFile, MAP_READ, 0, 0);
	FileHdr = (PIMAGE_FILE_HEADER) lpObj;
	CoffDumpSymbolTable (FileHdr);
	UnmapViewOfFile (lpObj);

	printf("%s saved successfully\n\n", szOutputFile);

	STDestroy (pOriginalST);

	FreeMemBlock (ConvertedSymbolsTable);
	FreeMemBlock (ConvertedStringTable);

	return 0;
}
