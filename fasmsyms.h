#pragma once

#include <vector>

/*
 * Module for working with FASM symbols
 */

struct FASM_SYMHDR_OBJECT;

struct FASM_SYMHDR;
typedef struct FASM_SYMHDR FASM_SYMHDR, *PFASM_SYMHDR, **PPFASM_SYMHDR;

struct PASCAL_STRING;
typedef struct PASCAL_STRING PASCAL_STRING, *PPASCAL_STRING, **PPPASCAL_STRING;

union FASM_SECTION_OR_EXTERNAL_SYMBOL_REF;

struct FASM_SYMBOL;
typedef struct FASM_SYMBOL FASM_SYMBOL, *PFASM_SYMBOL, **PPFASM_SYMBOL;

struct FASM_SYM_LINE;
typedef struct FASM_SYM_LINE  FASM_SYM_LINE, *PFASM_SYM_LINE, **PPFASM_SYM_LINE;

struct FASM_ASMDUMP_ROW;
typedef struct FASM_ASMDUMP_ROW FASM_ASMDUMP_ROW, *PFASM_ASMDUMP_ROW, **PPFASM_ASMDUMP_ROW;

//////////////////////////////////////////////////////////////////////////

char* PascalToAsciiz (PPASCAL_STRING PasStr);
int IsSymHdrObjectValid (PFASM_SYMHDR Hdr, SIZE_T FileSize, FASM_SYMHDR_OBJECT *pObj, char* pszObject);

#define IS_SYM_HDR_OBJECT_VALID(HDR,FS,OBJ) IsSymHdrObjectValid((HDR),(FS),&(HDR)->OBJ,#OBJ)

int FasmCheckSymbolsHdr (PFASM_SYMHDR Hdr, SIZE_T Size);

#define FASM_MAX_MAJOR_VER_SUPPORTED 1

PSZ FasmGetSymbolName (PFASM_SYMHDR Hdr, PFASM_SYMBOL Sym);

typedef std::vector<PFASM_SYMBOL> FASMSYMPTR_VECTOR;

void FasmExtractSymbolsToConvert (
	PFASM_SYMHDR Hdr, 
	FASMSYMPTR_VECTOR &syms
	);

void FasmDumpSymsVector (PFASM_SYMHDR Hdr, FASMSYMPTR_VECTOR &syms);
