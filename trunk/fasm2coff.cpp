#include <windows.h>
#include "fasm2coff.h"
#include "helper.h"
#include "fasmsymfmt.h"

static bool FasmCheckIsPublicSymbol (STRING_VECTOR &publics, char* szSymName)
{
	for (unsigned i=0; i<publics.size(); i++)
	{
		if (!strcmp (publics[i].c_str(), szSymName))
			return true;
	}
	return false;
}

// Convert FASM symbols to COFF symbols
// Returns vector of coff symbols and coff string table.
bool FasmConvertSymbolsToCoff (
	 IN PFASM_SYMHDR Hdr,
	 IN FASMSYMPTR_VECTOR &fasmSyms,
	 IN STRING_VECTOR &publics,
	OUT SYMBOL_VECTOR &convertedSyms,
	OUT PCOFF_STRING_TABLE *ppST
	)
{
	size_t cFasm = fasmSyms.size();
	convertedSyms.reserve(cFasm);

	size_t cbSTNeeded = 0;

	unsigned iFasm, iCoff;

	// Two indices: in fasm vector and in coff vector
	// Enumerate fasm symbols and convert them to coff.
	// Also remember byte count for string table.
	for (iFasm=0,iCoff=0; iFasm < cFasm; iFasm++, iCoff++)
	{
		PFASM_SYMBOL FasmSym = fasmSyms[iFasm];
		
		PSZ szFasmSymName = FasmGetSymbolName (Hdr, FasmSym);
		if (szFasmSymName == NULL)
		{
			printf(" skipping anonymous symbol\n");
			iCoff--;
			continue;
		}

		IMAGE_SYMBOL Symbol = {0};

		printf(" converting %s = %I64x  (%s)\n", szFasmSymName, FasmSym->Value, FasmSym->Ref.RelativeToExternal ? "ext" : "pub");

		if (strlen(szFasmSymName) > 8)
			cbSTNeeded += strlen(szFasmSymName) + 1;
		else
			strncpy ((char*)Symbol.N.ShortName, szFasmSymName, 8);

		Symbol.Value = (DWORD) FasmSym->Value;

		if (FasmSym->Ref.RelativeToExternal)
		{
			// Relative to external symbol.
			Symbol.SectionNumber = IMAGE_SYM_UNDEFINED;
			Symbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
			Symbol.Type = IMAGE_SYM_TYPE_NULL;
		}
		else
		{
			//
			// Public or static symbol
			//

			if (FasmCheckIsPublicSymbol (publics, szFasmSymName))
			{
				Symbol.SectionNumber = FasmSym->Ref.SectionIndex;
				Symbol.StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
				Symbol.Type = 0;
			}
			else
			{
				Symbol.SectionNumber = FasmSym->Ref.SectionIndex;
				Symbol.StorageClass = IMAGE_SYM_CLASS_STATIC;
				Symbol.Type = 0;
			}
		}

		FreeSymName (szFasmSymName);

		convertedSyms.push_back (Symbol);
	
	} // for (fasm syms)

	//
	// Create new string table
	//

	printf("Creating new symbol table with size 0x%lx\n", cbSTNeeded);

	PCOFF_STRING_TABLE pFasmST = STCreate (cbSTNeeded);

	if (pFasmST == NULL)
		return printf("Error: not enough memory for Fasm ST\n"), false;

	char* szCurPtr = STGetFirstString (pFasmST);

	printf("Filling symbol table\n");

	// Add fasm symbol names to string table
	for (iFasm=0,iCoff=0; iFasm < cFasm; iFasm++, iCoff++)
	{
		PFASM_SYMBOL FasmSym = fasmSyms[iFasm];
		PIMAGE_SYMBOL ImgSym = &convertedSyms[iCoff];

		PSZ szSymName = FasmGetSymbolName (Hdr, FasmSym);
		if (szSymName == 0)
		{
			printf(" skipping anonymous symbol\n");
			iCoff--;
			continue;
		}

		if (strlen (szSymName) > 8)
		{
			printf(" processing %s\n", szSymName);

			ImgSym->N.Name.Short = 0;
			szCurPtr = STAddString (pFasmST, szCurPtr, szSymName, &ImgSym->N.Name.Long);
		}
	} // for (fasm syms)

	*ppST = pFasmST;
	return true;
}

void Fasm2CoffExtractUniqueSymbols (
	 IN SYMBOL_VECTOR &origSyms,
	 IN PCOFF_STRING_TABLE pST,
	OUT SYMBOL_VECTOR &uniqueSyms,
	 IN PFASM_SYMHDR Hdr,
	 IN FASMSYMPTR_VECTOR &fasmSymbols,
  OUT SYMBOL_VECTOR &nonUnique
	)
/*++
	Look thru original symbol table and for each entry try to find a match
	in FASM symbols. If there is no match, add symbol to uniqueSyms.
--*/
{
	for (unsigned i=0; i<origSyms.size(); i++)
	{
		PIMAGE_SYMBOL Sym = &origSyms[i];
		PSZ origSymName = CoffSTGetSymbolName(pST, Sym);

		bool bBreak = false;
		bool bUnique = true;

		for (unsigned k=0; k<fasmSymbols.size() && !bBreak; k++)
		{
			PFASM_SYMBOL FasmSym = fasmSymbols[k];
			PSZ FasmSymName = FasmGetSymbolName (Hdr, FasmSym);
			if (FasmSymName != NULL)
			{
				if (!strcmp (origSymName, FasmSymName))
				{
					// COFF symbol exists in FASM symbol table.
					// Mark as BAD
					bBreak = true;
					bUnique = false;
				}

				FreeSymName (FasmSymName);
			}
		} // for (fasmSymbols)

		if (bUnique)
		{
			uniqueSyms.push_back(*Sym);

			// aux symbols for this sym should be pushed too.
			for (unsigned iAux = 0; iAux < Sym->NumberOfAuxSymbols; iAux++)
			{
				uniqueSyms.push_back (origSyms[i + iAux + 1]);
			}
		}
		else
		{
			nonUnique.push_back (*Sym);

			// aux symbols for this sym should be pushed too.
			for (unsigned iAux = 0; iAux < Sym->NumberOfAuxSymbols; iAux++)
			{
				nonUnique.push_back (origSyms[i + iAux + 1]);
			}
		}

		FreeSymName (origSymName);

		// No need to process aux symbols.
		i += Sym->NumberOfAuxSymbols;
	}
}

//
// Process symbols for section names.
// Generate aux symbols for them.
// (see COFF format reference, 'section symbols')
//

void Fasm2CoffProcessSectionNames (
	PIMAGE_FILE_HEADER FileHdr,
	SYMBOL_VECTOR &syms, 
	PCOFF_STRING_TABLE pST
	)
{
	PIMAGE_SECTION_HEADER Sections = (PIMAGE_SECTION_HEADER)(FileHdr+1);
	size_t cSyms = syms.size();

	for (unsigned i=0; i<cSyms; i++)
	{
		PIMAGE_SYMBOL Sym = &syms[i];
		PIMAGE_SECTION_HEADER SectHdr = &Sections[Sym->SectionNumber - 1];

		if (Sym->StorageClass == IMAGE_SYM_CLASS_STATIC && Sym->Type == 0 && Sym->Value == 0)
		{
			union {
				IMAGE_AUX_SYMBOL AuxSect;
				IMAGE_SYMBOL AuxSectSymbol;
			};

			memset (&AuxSect, 0, sizeof(IMAGE_SYMBOL));

			AuxSect.Section.Length = SectHdr->SizeOfRawData;
			AuxSect.Section.NumberOfRelocations = SectHdr->NumberOfRelocations;
			
			if (SectHdr->Characteristics & (IMAGE_SCN_CNT_CODE|IMAGE_SCN_MEM_EXECUTE))
			{
				AuxSect.Section.Selection = IMAGE_COMDAT_SELECT_NODUPLICATES;
			}
			else
			{
				AuxSect.Section.Selection = IMAGE_COMDAT_SELECT_ASSOCIATIVE;
				AuxSect.Section.Number = Sym->SectionNumber;
			}

			syms.insert (syms.begin() + i + 1, AuxSectSymbol);
			cSyms++;

 			syms[i].NumberOfAuxSymbols = 1;
		}

		i += syms[i].NumberOfAuxSymbols;
	}
}
