#include <windows.h>
#include <stdio.h>
#include <common/mapping.h>
#include "fasmsymfmt.h"
#include "dumper.h"
#include "fasmsyms.h"
#include "helper.h"

// Text strings with FASM symbol type names
static char* szSymTypes[] = {
	"Absolute",
	"Relocatable Segment Address",
	"Relocatable 32-bit Address",
	"Relocatable Relative 32-bit Address",
	"Relocatable 64-bit Address",
	"(ELF) GOT-relative 32-bit Address",
	"(ELF) 32-bit Address of PLT entry",
	"(ELF) Relative 32-bit Address of PLT entry",
};

static char* strndup (const char* str, size_t n)
{
	char* sz = (char*)malloc(n+1);
	memcpy (sz, str, n);
	sz[n] = 0;
	return sz;
}

static char* PascalToAsciiz (PPASCAL_STRING PasStr)
{
	return strndup ((char*)&PasStr->String[0], PasStr->Length);
}

//
// Dump section or external symbol reference
//

static void DumpSectionOrExternalSymbol (PFASM_SYMHDR Hdr, FASM_SECTION_OR_EXTERNAL_SYMBOL_REF* Ref)
{
	if (Ref->RelativeToExternal)
	{
		char *Name = FASM_STRING_TABLE(Hdr) + Ref->ExternalSymNameOffset;
		printf(" Relative: to external symbol\n");
		printf(" ExternalSymName: OffsetST 0x%lx : %s\n", Ref->ExternalSymNameOffset, Name);
	}
	else
	{
		printf(" Relative: to section #%d ", Ref->SectionIndex);
		if (Hdr->SectionNames.Length != 0 && Hdr->SectionNames.Offset != 0)
		{
			PDWORD SectNames = FASM_SECTION_NAMES(Hdr);
			char* Name = FASM_STRING_TABLE(Hdr) + SectNames[Ref->SectionIndex];
			printf("(%s)", Name);
		}
		printf("\n");
	}
}

//
// Dump section or external symbol reference (mini-version)
//

static void DumpSectionOrExternalSymbol_mini (PFASM_SYMHDR Hdr, FASM_SECTION_OR_EXTERNAL_SYMBOL_REF* Ref)
{
	if (Ref->RelativeToExternal)
	{
		char *Name = FASM_STRING_TABLE(Hdr) + Ref->ExternalSymNameOffset;
		printf("rel: ext_sym '%s' ", Name);
	}
	else
	{
		printf("rel: sect = %d ", Ref->SectionIndex);
		if (Hdr->SectionNames.Length != 0 && Hdr->SectionNames.Offset != 0 && Ref->SectionIndex > 0)
		{
			PDWORD SectNames = FASM_SECTION_NAMES(Hdr);
			char* Name = FASM_STRING_TABLE(Hdr) + SectNames[Ref->SectionIndex-1];
			printf("(%s) ", Name);
		}
	}
}

static PFASM_SYM_LINE DumpLine (PFASM_SYMHDR Hdr, PFASM_SYM_LINE Line, unsigned iLine);

//
// Dump single symbol
//

static void DumpSymbol (PFASM_SYMHDR Hdr, PFASM_SYMBOL Sym, unsigned iSym)
{
	printf ("Symbol #%d  = %p\n", iSym, Sym);

	printf(" Name Offset: 0x%08x\n", Sym->SymbolNameOffset);

	if (IS_FASM_SYMBOL_ANONYMOUS(Sym))
		printf (" Anonymous\n");
	else if (IS_FASM_SYMBOL_NAME_IN_PREPROCESSED_SOURCE(Sym))
	{
		PPASCAL_STRING SymName = (PPASCAL_STRING)(FASM_PREPROCESSED_SOURCE(Hdr) + PREPROCESSED_SOURCE_SYMBOL_NAME_OFFSET(Sym));

		char *Name = PascalToAsciiz(SymName);

		printf(" Name (in PS): %s\n", Name);

		free(Name);
	}
	else if (IS_FASM_SYMBOL_NAME_IN_STRING_TABLE(Sym))
	{
		PSZ SymName = FASM_STRING_TABLE(Hdr) + STRING_TABLE_SYMBOL_NAME_OFFSET(Sym);
		printf("Name (ST): %s\n", SymName);
	}

	printf(" Value: 0x%I64x\n", Sym->Value);
	printf(" Flags: 0x%04x (%s)\n", Sym->Flags, FASM_SYMBOL_FLAGS_VALID(Sym) ? "valid" : "INVALID");
	if (FASM_SYMBOL_FLAGS_VALID(Sym) && Sym->Flags != 0)
	{
		printf("  ");
		if (Sym->Flags & FASM_SYMBOL_DEFINED) printf("DEFINED ");
		if (Sym->Flags & FASM_SYMBOL_ASSEMBLY_TIME_VAR) printf("ASM-TIME-VAR ");
		if (Sym->Flags & FASM_SYMBOL_NOT_FORWARD_REFERENCED) printf("NOT-FORWARD-REF ");
		if (Sym->Flags & FASM_SYMBOL_USED) printf("USED ");
		if (Sym->Flags & FASM_SYMBOL_PREDICTED_USE) printf("PRED-USE ");
		if (Sym->Flags & FASM_SYMBOL_PREDICTED_USE_RES) printf("PRED-USE-1 ");
		if (Sym->Flags & FASM_SYMBOL_PREDICTED_DEFINITION) printf("PRED-DEF ");
		if (Sym->Flags & FASM_SYMBOL_PREDICTED_DEFINITION_RES) printf("PRED-DEF-1 ");
		if (Sym->Flags & FASM_SYMBOL_OPTIMIZATION_ADJ_APPLIED) printf("OPTIMIZATION-ADJ-APPLIED ");
		printf("\n");
	}
	printf(" SizeOfData: %d\n", Sym->SizeOfData);
	printf(" Type: %d ", Sym->Type);
	if (Sym->Type <= FASM_SYMBOL_TYPE_MAX)
		printf("(%s)", szSymTypes[Sym->Type]);
	printf("\n");
	printf(" Extended SIB: 0x%08x\n", Sym->ExtendedSIB);
	printf(" #pass, where symbol was defined last time: %d\n", Sym->LastPassDefined);
	printf(" #pass, where symbol was used last time: %d\n", Sym->LastPassUsed);
	DumpSectionOrExternalSymbol (Hdr, &Sym->Ref);
	printf(" Symbol #line offset in PS: 0x%08x\n", Sym->LineOffset);
	
	printf(" Line:\n");
	PFASM_SYM_LINE Line = ((PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Sym->LineOffset));
	DumpLine (Hdr, Line, -1);

	printf("\n");
}

static PFASM_SYM_LINE DumpLine_Mini (PFASM_SYMHDR Hdr, PFASM_SYM_LINE Line, unsigned iLine);

//
// Mini-version of symbol dumping
//

static void DumpSymbol_Mini (PFASM_SYMHDR Hdr, PFASM_SYMBOL Sym, unsigned iSym)
{
	char* Name = 0;
	bool bFreeName = false;

	if (IS_FASM_SYMBOL_ANONYMOUS(Sym))
		Name = "<Anonymous>";
	else if (IS_FASM_SYMBOL_NAME_IN_PREPROCESSED_SOURCE(Sym))
	{
		PPASCAL_STRING SymName = (PPASCAL_STRING)(FASM_PREPROCESSED_SOURCE(Hdr) + PREPROCESSED_SOURCE_SYMBOL_NAME_OFFSET(Sym));

		Name = PascalToAsciiz(SymName);
		bFreeName = true;
	}
	else if (IS_FASM_SYMBOL_NAME_IN_STRING_TABLE(Sym))
	{
		Name = FASM_STRING_TABLE(Hdr) + STRING_TABLE_SYMBOL_NAME_OFFSET(Sym);
	}

	PFASM_SYM_LINE Line = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Sym->LineOffset);
	if (Sym->LineOffset == 0) Line = NULL;

	char *szSrcFile = 0;

	if (Line != NULL && Line->MacroGenerated == 0)
	{
		if (Line->FileNameOffset == 0)
			szSrcFile = (PSZ)Hdr + Hdr->StringTable.Offset + Hdr->dwInputFileNameOffset;
		else
			szSrcFile = FASM_PREPROCESSED_SOURCE(Hdr) + Line->FileNameOffset;
	}

	printf("#%04x =0x%I64x Type %d (%s), SizeOf: %d, Line: 0x%lx (#%d of %s), ", 
		iSym,
		Sym->Value,
		Sym->Type,
		FASM_SYMBOL_FLAGS_VALID(Sym) ? szSymTypes[Sym->Type] : "<INVALID>",
		Sym->SizeOfData,
		Sym->LineOffset,
		Line == NULL ? -1 : Line->LineNumber,
		szSrcFile == NULL ? "<unk>" : szSrcFile
		);

	DumpSectionOrExternalSymbol_mini (Hdr, &Sym->Ref);

	printf(", Name: '%s'\n", Name);

	if (bFreeName) free (Name);

	if (Line)
		DumpLine_Mini (Hdr, Line, -1);
}

//
// Dump single preprocessed source line
//
// Return pointer to the next line
//

static PFASM_SYM_LINE DumpLine (PFASM_SYMHDR Hdr, PFASM_SYM_LINE Line, unsigned iLine)
{
	printf("Line #%d  =  %p (offset 0x%lx LineOffset 0x%lx)\n", iLine, Line, (ULONG_PTR)Line - (ULONG_PTR)Hdr, (PSZ)Line - FASM_PREPROCESSED_SOURCE(Hdr));
	printf(" raw: %08x %08x %08x %08x\n", ((DWORD*)Line)[0], ((DWORD*)Line)[1], 
		((DWORD*)Line)[2], ((DWORD*)Line)[3]);
	printf(" Generated by macro: %s\n", Line->MacroGenerated ? "TRUE" : "FALSE");

	// Generated by macro?
	if (Line->MacroGenerated)
	{
		printf(" MacroNameOffset: 0x%lx ", Line->MacroNameOffset);

		PPASCAL_STRING MacroName = (PPASCAL_STRING)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroNameOffset);

		char* szName = PascalToAsciiz(MacroName);

		printf(": %s\n", szName);

		free(szName);

		printf(" Line Number: %d\n", Line->LineNumber);
		printf(" MacroInvokedFromLine: Offset 0x%lx ", Line->MacroInvokedLineOffset);
		PFASM_SYM_LINE InvokedFrom = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroInvokedLineOffset);
		printf("(#%d)\n", InvokedFrom->LineNumber);
		printf(" InMacroLine: Offset 0x%lx ", Line->InMacroLineOffset);
		PFASM_SYM_LINE InMacro = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroInvokedLineOffset);
		printf("(#%d)\n", InMacro->LineNumber);
	}
	else
	{
		printf(" FileName: Offset 0x%lx ", Line->FileNameOffset);
		PSZ FileName = FASM_PREPROCESSED_SOURCE(Hdr) + Line->FileNameOffset;
		if (Line->FileNameOffset == 0)
			FileName = FASM_STRING_TABLE(Hdr) + Hdr->dwInputFileNameOffset;
		printf(": %s\n", FileName);
		printf(" Line Number: %d\n", Line->LineNumber);
		printf(" SourceFilePosition: 0x%lx (%d)\n", Line->SourceFilePosition, Line->SourceFilePosition);
	}

// 	printf("\n");
// 
// 	return NULL;

	PBYTE pInfo = (PBYTE)(Line+1);

	printf("                           ");

	while (pInfo[0] != 0) 
	{
		BYTE Len = pInfo[1];

		switch (pInfo[0])
		{
		case 0x3b:
		case 0x1a:
			{
				char* Str = strndup ((char*)&pInfo[2], Len);

				printf("\"%s\" ", Str);
				free (Str);

				pInfo = pInfo + pInfo[1] + 2;
			}
			break;

		case 0x22:
			{
				DWORD dwBytes = *(DWORD*)&pInfo[1];

				char* Value = strndup ((char*)&pInfo[5], dwBytes);

				printf("`%s` ", Value);

				free (Value);

				pInfo += sizeof(DWORD) + dwBytes + 1;
				break;
			}
		default:
			printf("'%c' ", pInfo[0]);
			pInfo ++;
		} // switch()
	}

	printf("\n");

// 	PBYTE pInfo = (PBYTE)(Line+1);
// 
// 	do 
// 	{
// 		BYTE Len = pInfo[1];
// 
// 		printf("  chain [%02x] ", pInfo[0], pInfo[1]);
// 
// 		switch (pInfo[0])
// 		{
// 		case 0x3b:
// 			printf("following_ignored ");
// 
// 		case 0x1a:
// 			{
// 				printf("[len %02x]\n   ", Len);
// 
// 				char* Str = strndup ((char*)&pInfo[2], Len);
// 
// 				printf("\"%s\"\n", Str);
// 				free (Str);
// 
// // 				for (unsigned j=0; j<Len; j++)
// // 					printf("%02x ", pInfo[2+j]);
// // 
// 				pInfo = pInfo + pInfo[1] + 2;
// // 				printf("\n");
// 			}
// 			break;
// 
// 		case 0x22:
// 			{
// 				DWORD dwBytes = *(DWORD*)&pInfo[1];
// 
// 				char* Value = strndup ((char*)&pInfo[5], dwBytes);
// 
// 				printf("\n");
// 				printf("   dwBytes : 0x%lx\n", dwBytes);
// 				printf("   value: '%s'\n", Value);
// 
// 				free (Value);
// 
// 				pInfo += sizeof(DWORD) + dwBytes + 1;
// 				break;
// 			}
// 		default:
// 			printf(" = '%c'\n", pInfo[0]);
// 			pInfo ++;
// 		} // switch()
// 	}
// 	while (pInfo[0] != 0);
// 
// 	printf("\n");

	return (PFASM_SYM_LINE)(++pInfo);
}

//
// Dump single preprocessed source line (mini-version)
//
// Return pointer to the next line
//

static PFASM_SYM_LINE DumpLine_Mini (PFASM_SYMHDR Hdr, PFASM_SYM_LINE Line, unsigned iLine)
{
	printf(" Line #%d from ", Line->LineNumber);
	printf("%s ", Line->MacroGenerated ? "MACRO" : "FILE");

	// Generated by macro?
	if (Line->MacroGenerated)
	{
		PPASCAL_STRING MacroName = (PPASCAL_STRING)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroNameOffset);
		char* szName = PascalToAsciiz(MacroName);

		printf("%s ", szName);

		free(szName);

		PFASM_SYM_LINE InvokedFrom = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroInvokedLineOffset);
		printf("invoked_from_line #%d ", InvokedFrom->LineNumber);

		PFASM_SYM_LINE InMacro = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Line->MacroInvokedLineOffset);
		printf("in_macro_line #%d\n", InMacro->LineNumber);
	}
	else
	{
		PSZ FileName = FASM_PREPROCESSED_SOURCE(Hdr) + Line->FileNameOffset;
		if (Line->FileNameOffset == 0)
			FileName = FASM_STRING_TABLE(Hdr) + Hdr->dwInputFileNameOffset;
		printf("%s ", FileName);
		printf("source_pos %d\n", Line->SourceFilePosition, Line->SourceFilePosition);
	}

	PBYTE pInfo = (PBYTE)(Line+1);

	printf("     ");

	while (pInfo[0] != 0) 
	{
		BYTE Len = pInfo[1];

		switch (pInfo[0])
		{
		case 0x3b:
		case 0x1a:
			{
				char* Str = strndup ((char*)&pInfo[2], Len);

				printf("%s ", Str);
				free (Str);

				pInfo = pInfo + pInfo[1] + 2;
			}
			break;

		case 0x22:
			{
				DWORD dwBytes = *(DWORD*)&pInfo[1];

				char* Value = strndup ((char*)&pInfo[5], dwBytes);

				printf("\"%s\" ", Value);

				free (Value);

				pInfo += sizeof(DWORD) + dwBytes + 1;
				break;
			}
		default:
			printf("%c ", pInfo[0]);
			pInfo ++;
		} // switch()
	}

	printf("\n");

	return (PFASM_SYM_LINE)(++pInfo);
}

//
// Dump Assembly Dump Row
//

#include <vector>

std::vector<DWORD> LineOffsets;

static void DumpADRow (PFASM_SYMHDR Hdr, PFASM_ASMDUMP_ROW Row, unsigned iRow)
{
	if (Row->LineOffset)
	{
		LineOffsets.push_back (Row->LineOffset);
	}

	if (iRow > 15200)
	{
		printf("Row #%d  -  %p (offset 0x%lx)\n", iRow, Row, (ULONG_PTR)Row - (ULONG_PTR)Hdr);
		printf(" Offset in output file: 0x%lx\n", Row->InOutputFileOffset);
		printf(" Offset of line in preprocessed source: 0x%lx\n", Row->LineOffset);
		// 	if (Row->LineOffset)
		// 	{
		// 		printf(" Line:\n");
		// 		PFASM_SYM_LINE Line = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + Row->LineOffset);
		// 		DumpLine(Hdr, Line, 0);
		// 	}
		printf(" Value of $ address: 0x%lx\n", Row->AddressValue);
		printf(" Extended SIB: 0x%lx\n", Row->ExtendedSIB);
		DumpSectionOrExternalSymbol (Hdr, &Row->Ref);
		printf(" Address Type: %d (%s)\n", Row->AddressType,
			FASM_SYMBOL_TYPE_VALID(Row->AddressType) ? szSymTypes[Row->AddressType] : "<type-invalid>");
		printf(" Code Type: %d\n", Row->CodeType);
		printf(" Inside Virtual Block: %s\n", Row->InVirtualBlock ? "TRUE" : "FALSE");
		printf(" Not Included In The Output File: %s\n", Row->NotIncludedInOutput ? "TRUE" : "FALSE");
		printf("\n");
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// Dump FASM Symbol File
//

int DumpFasmSym  (char* szSymFile, DWORD dwFlags)
{
	SIZE_T Size = 0;
	LPVOID lpSym = MapExistingFile (szSymFile, MAP_READ, 0, &Size);

	if (lpSym == NULL)
		return printf("FASMSYMMGR: DumpFasmSym: can't map input file\n");

	printf("Input FASM symbol file mapped at %p size %lx\n", lpSym, Size);

	PFASM_SYMHDR Hdr = (PFASM_SYMHDR) lpSym;
	FasmCheckSymbolsHdr (Hdr, Size);

	if (dwFlags & DUMP_HEADER)
	{
		printf("\nFASM Symbol File Header\n");
		printf(  "-----------------------\n");

		printf(" Signature  %08x (%s)\n", Hdr->dwSignature, Hdr->dwSignature == FASM_SYM_SIGNATURE ? "valid" : "INVALID");
		printf(" Version: %d.%d\n", Hdr->bVerMajor, Hdr->bVerMinor);
		printf(" Headers Length: 0x%lx\n", Hdr->wHeaderLen);

		printf(" StringTable: offset 0x%lx length 0x%lx\n", Hdr->StringTable);
		printf(" SymbolTable: offset 0x%lx length 0x%lx\n", Hdr->SymbolTable);
		printf(" Preprocessed Source: offset 0x%lx length 0x%lx\n", Hdr->PreprocessedSource);
		printf(" Assembly Dump:       offset 0x%lx length 0x%lx\n", Hdr->AssemblyDump);
		printf(" Section Names:       offset 0x%lx length 0x%lx\n", Hdr->SectionNames);

		PSZ InputFile = (PSZ)lpSym + Hdr->dwInputFileNameOffset + Hdr->StringTable.Offset;
		PSZ OutputFile = (PSZ)lpSym + Hdr->dwOutputFileNameOffset + Hdr->StringTable.Offset;

		printf(" InputFile: (+0x%lx) %s\n", Hdr->dwInputFileNameOffset, InputFile);
		printf(" OutputFile: (+0x%lx) %s\n", Hdr->dwOutputFileNameOffset, OutputFile);

		printf("\n");
	}

	// pointers
	PSZ StringTable = FASM_STRING_TABLE(Hdr);
	PSZ PreSource = FASM_PREPROCESSED_SOURCE(Hdr);

	if (dwFlags & DUMP_STRINGS)
	{
		// Dump strings
		printf("\nString table:\n");
		printf("-------------\n");
		for (char* sz = StringTable; sz < StringTable + Hdr->StringTable.Length; sz += strlen(sz) + 1)
			printf(" %s\n", sz);

		printf("\n");
	}

	if (dwFlags & DUMP_SYMBOLS)
	{
		// Dump symbols
		printf("\nSymbol table:\n");
		printf("-------------\n");
		size_t cSyms = Hdr->SymbolTable.Length / sizeof(FASM_SYMBOL);
		PFASM_SYMBOL Symbols = FASM_FIRST_SYMBOL(Hdr);

		//	if (cSyms > 20) cSyms = 20;

		for (unsigned i=0; i<cSyms; i++)
		{
			PFASM_SYMBOL Sym = &Symbols[i];

#if 0
			printf("Symbol Record %d\n", i);
			DumpSymbol (SymHdr, Sym);
#else
			DumpSymbol_Mini (Hdr, Sym, i);
#endif
		}

		printf("\n");
	}

	if (dwFlags & DUMP_PS_LINES)
	{
		// Dump lines
		printf("\nPreprocessed Source Lines:\n");
		printf(  "--------------------------\n");

		PFASM_SYM_LINE Line = (PFASM_SYM_LINE) FASM_PREPROCESSED_SOURCE(Hdr);
		unsigned i = 0;

		Line = (PFASM_SYM_LINE)((PSZ)Hdr + 0x7a7a1);

		do 
		{
			Line = DumpLine (Hdr, Line, i);
			i++;
		} 
		while ((PSZ)Line < FASM_PREPROCESSED_SOURCE(Hdr) + Hdr->PreprocessedSource.Length
			&& Line != NULL);

		printf("\n");
	}

	if (dwFlags & DUMP_ASM_DUMP)
	{
		// Dump ASM dump
		printf("\nAssembly Dump:\n");
		printf(  "--------------\n");

		size_t cRows = (Hdr->AssemblyDump.Length - sizeof(DWORD)) / sizeof(FASM_ASMDUMP_ROW);

		printf("SizeOf: %d\n", Hdr->AssemblyDump.Length);
		printf("Number of rows: %d\n", cRows);
		printf("SizeOf2: %d\n", cRows * sizeof(FASM_ASMDUMP_ROW) + sizeof(DWORD));

		PDWORD pLineStopped = (PDWORD)((PCHAR)Hdr + Hdr->AssemblyDump.Offset + Hdr->AssemblyDump.Length - sizeof(DWORD));
		PFASM_ASMDUMP_ROW Rows = (PFASM_ASMDUMP_ROW)((PCHAR)Hdr + Hdr->AssemblyDump.Offset);

		printf("#line stopped: %d\n\n", *pLineStopped);

		for (unsigned i=0; i<cRows; i++)
		{
			PFASM_ASMDUMP_ROW Row = &Rows[i];

			DumpADRow (Hdr, Row, i);
		}

#if 1
// 		for (unsigned i=0; i<LineOffsets.size()/2; i++)
// 			for (unsigned j=0; j<LineOffsets.size()/2; j++)
// 			{
// 				if ((i < j && LineOffsets[i] > LineOffsets[j]) ||
// 					(i > j && LineOffsets[i] < LineOffsets[j]))
// 				{
// 					DWORD offset = LineOffsets[j];
// 					LineOffsets[j] = LineOffsets[i];
// 					LineOffsets[i] = offset;
// 				}
// 			}

		for (unsigned i=0; i<min(LineOffsets.size(), 20); i++)
		{
			printf(" line offset #%d = 0x%lx\n", i, LineOffsets[i]);

			PFASM_SYM_LINE Line = (PFASM_SYM_LINE)(FASM_PREPROCESSED_SOURCE(Hdr) + LineOffsets[i]);
			
			__asm nop;
		}

		LineOffsets.clear();
#endif

		printf("\n");
	}

	if (dwFlags & DUMP_SECTIONS)
	{
		// Dump ASM dump
		printf("\nSection Names:\n");
		printf(  "--------------\n");

		size_t cSections = Hdr->SectionNames.Length / sizeof(DWORD);
		PDWORD SectionNames = (PDWORD)((PSZ)Hdr + Hdr->SectionNames.Offset);

		for (unsigned i=0; i<cSections; i++)
		{
			char* Name = FASM_STRING_TABLE(Hdr) + SectionNames[i];
			printf("Section #%d = %s\n", i, Name);
		}

		printf("\n");
	}

	printf("\n===============================================\nEnd Of Dump\n");

	return 0;
}

