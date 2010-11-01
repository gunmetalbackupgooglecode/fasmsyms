#pragma once

#include "fasmsyms.h"
#include "coffsyms.h"

// Convert FASM symbols to COFF symbols
// Returns vector of coff symbols and coff string table.
bool FasmConvertSymbolsToCoff (
	 IN PFASM_SYMHDR Hdr,
	 IN FASMSYMPTR_VECTOR &fasmSyms,
	 IN STRING_VECTOR &publics,
	OUT SYMBOL_VECTOR &convertedSyms,
	OUT PCOFF_STRING_TABLE *ppST
	);

void Fasm2CoffExtractUniqueSymbols (
	 IN SYMBOL_VECTOR &origSyms,
	 IN PCOFF_STRING_TABLE pST,
	OUT SYMBOL_VECTOR &uniqueSyms,
	 IN PFASM_SYMHDR Hdr,
	 IN FASMSYMPTR_VECTOR &fasmSymbols,
	 OUT SYMBOL_VECTOR &nonUnique
	);

void Fasm2CoffProcessSectionNames (
	PIMAGE_FILE_HEADER FileHdr,
	SYMBOL_VECTOR &syms, 
	PCOFF_STRING_TABLE pST
	);
