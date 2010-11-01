#include <windows.h>
#include "fasmsyms.h"
#include "helper.h"
#include "fasmsymfmt.h"

int IsSymHdrObjectValid (PFASM_SYMHDR Hdr, SIZE_T FileSize, FASM_SYMHDR_OBJECT *pObj, char* pszObject)
{
	if (pObj->Offset > FileSize)
		return printf("Error: FASM_SYMHDR_OBJECT(%s) Offset is out of bounds\n", pszObject);

	if (pObj->Offset + pObj->Length > FileSize)
		return printf("Error: FASM_SYMHDR_OBJECT(%s) Offset+Length is out of bounds\n", pszObject);

	return 0;
}

// returns 0 on success, positive value otherwise
int FasmCheckSymbolsHdr (PFASM_SYMHDR Hdr, SIZE_T Size)
{
	if (Hdr->dwSignature != FASM_SYM_SIGNATURE)
		return printf("Error: FASM symbols file header signature mismatch\n");

	if (Hdr->bVerMajor > FASM_MAX_MAJOR_VER_SUPPORTED)
		return printf("Error: Compiled with FASM %d.%d, maximum VerMajor supported is %d\n",
		Hdr->bVerMajor,
		Hdr->bVerMinor,
		FASM_MAX_MAJOR_VER_SUPPORTED);

	if (Hdr->dwInputFileNameOffset >= Hdr->StringTable.Length)
		return printf("Error: Input file name offset is out of file bounds\n");

	if (Hdr->dwOutputFileNameOffset >= Hdr->StringTable.Length)
		return printf("Error: Output file name offset is out of file bounds\n");

	int res = 0;

	if ((res = IS_SYM_HDR_OBJECT_VALID (Hdr, Size, StringTable)) != 0)
		return res;

	if ((res = IS_SYM_HDR_OBJECT_VALID (Hdr, Size, SymbolTable)) != 0)
		return res;

	if ((res = IS_SYM_HDR_OBJECT_VALID (Hdr, Size, PreprocessedSource)) != 0)
		return res;

	if ((res = IS_SYM_HDR_OBJECT_VALID (Hdr, Size, AssemblyDump)) != 0)
		return res;

	if ((res = IS_SYM_HDR_OBJECT_VALID (Hdr, Size, SectionNames)) != 0)
		return res;

	return 0;
}

char* PascalToAsciiz (PPASCAL_STRING PasStr)
{
	return strndup ((char*)&PasStr->String[0], PasStr->Length);
}

PSZ FasmGetSymbolName (PFASM_SYMHDR Hdr, PFASM_SYMBOL Sym)
{
	PSZ szName = NULL;

	if (Sym->Ref.RelativeToExternal)
	{
		// External.
		szName = _strdup (FASM_STRING_TABLE(Hdr) + Sym->Ref.ExternalSymNameOffset);
	}
	else
	{
		// Public

		if (IS_FASM_SYMBOL_ANONYMOUS(Sym))
			szName = NULL;

		else if (IS_FASM_SYMBOL_NAME_IN_PREPROCESSED_SOURCE(Sym))
		{
			PPASCAL_STRING SymName = (PPASCAL_STRING)(FASM_PREPROCESSED_SOURCE(Hdr) + PREPROCESSED_SOURCE_SYMBOL_NAME_OFFSET(Sym));
			szName = PascalToAsciiz(SymName);
		}
		else if (IS_FASM_SYMBOL_NAME_IN_STRING_TABLE(Sym))
		{
			szName = _strdup (FASM_STRING_TABLE(Hdr) + STRING_TABLE_SYMBOL_NAME_OFFSET(Sym));
		}
	}

	return szName;
}

// Extracts symbols from FASM Symbols File to FASMSYMPTR vector
// (vector of pointers to FASM_SYMBOL)
void FasmExtractSymbolsToConvert (
	PFASM_SYMHDR Hdr, 
	FASMSYMPTR_VECTOR &syms
	)
{
	PFASM_SYMBOL Symbols = FASM_FIRST_SYMBOL(Hdr);
	size_t cSyms = Hdr->SymbolTable.Length / sizeof(FASM_SYMBOL);

	// Enumerate all FASM symbols
	for (unsigned i=0; i<cSyms; i++)
	{
		PFASM_SYMBOL Sym = &Symbols[i];

		if (Sym->LineOffset == 0)
			continue;

		PFASM_SYM_LINE Line = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Sym->LineOffset);

		if (Sym->Ref.RelativeToExternal == 1 || 
			(Sym->Ref.RelativeToExternal == 0 && Sym->Ref.SectionIndex > 0))
		{
			PSZ szSymName = FasmGetSymbolName (Hdr, Sym);

			if (szSymName != NULL)
			{
				// Remember FASM symbol
				syms.push_back (Sym);

				FreeSymName (szSymName);
			}

		} // if (extract symbol)

	} // for (all syms)
}

// Dump FASMSYMPTR_VECTOR
void FasmDumpSymsVector (PFASM_SYMHDR Hdr, FASMSYMPTR_VECTOR &syms)
{
	for (unsigned i=0; i<syms.size(); i++)
	{
		PFASM_SYMBOL Sym = syms[i];
		PSZ szSymName = FasmGetSymbolName (Hdr, Sym);
		if (szSymName)
		{
			printf(" [%02d] = %I64x = %s\n", i, Sym->Value, szSymName);

			FreeSymName (szSymName);
		}
	}
}

