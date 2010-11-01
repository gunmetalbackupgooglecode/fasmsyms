#include <windows.h>
#include <stdio.h>
#include "cv.h"
#include "cvsyms.h"
#include "coffsyms.h"

typedef struct CVSIG {
	DWORD dwSig;
}  CVSIG, *PCVSIG;

//
// Create CodeView COFF debug info
//

PCV_COFF_DEBUG CvCoffCreateDebugInfo (BYTE bCvCoffSig, size_t maxLength)
{
	PCV_COFF_DEBUG cv = new CV_COFF_DEBUG (bCvCoffSig,maxLength);
	cv->pData = malloc (maxLength);
	
	PCVSIG CvSig = (PCVSIG) cv->pData;
	CvSig->dwSig = bCvCoffSig;

	cv->pFreeSymRec = (PBYTE) (CvSig + 1);

	return cv;
}

//
// Add symbol record to CodeView COFF debug info
//
// (don't forget to add the corresponding relocations, yo!)
//

size_t CvCoffAddSymRec (PCV_COFF_DEBUG cv, PSYMTYPE Symbol)
{
	size_t cbSym = Symbol->reclen + sizeof(WORD);

	if (cv->cbData + cbSym > cv->cbMax)
	{
		// need to grow the buffer.
		// can't go it now
		return 0;
	}

	memcpy (cv->pFreeSymRec, Symbol, cbSym);

	size_t offs = cv->cbData;

// 	if (cbSym & 3)
// 		cbSym += (4 - (cbSym & 3));

	cv->pFreeSymRec += cbSym;
	cv->cbData += cbSym;
	return offs;
}

//
// Build CodeView COFF debug information for .debug$S section
//

bool CvCoffEmitDebugInfo (
	PCV_COFF_DEBUG cv, 
	PVOID pBuffer, 
	SIZE_T *pcbBuffer, 
	PIMAGE_SECTION_HEADER pSection,
	PVOID pRelocs,
	SIZE_T *pcbRelocs
	)
{
	if (cv->cbData > *pcbBuffer)
	{
		*pcbBuffer = cv->cbData;
		return false;
	}
	*pcbBuffer = cv->cbData;

	size_t cRelocs = cv->relocs.size();
	if (cRelocs * sizeof(IMAGE_RELOCATION) > *pcbRelocs)
	{
		*pcbRelocs = cRelocs * sizeof(IMAGE_RELOCATION);
		return false;
	}
	*pcbRelocs = cRelocs * sizeof(IMAGE_RELOCATION);

	//
	// Write section header
	//

	strncpy ((char*)pSection->Name, ".debug$S", IMAGE_SIZEOF_SHORT_NAME);
	pSection->Characteristics = 
		IMAGE_SCN_TYPE_NO_PAD
		| IMAGE_SCN_CNT_INITIALIZED_DATA
		| IMAGE_SCN_MEM_DISCARDABLE
		| IMAGE_SCN_MEM_READ;
	pSection->SizeOfRawData = cv->cbData;
	pSection->NumberOfRelocations = cRelocs;

	//
	// Write symbols
	//

	memcpy (pBuffer, cv->pData, cv->cbData);

	//
	// Write relocations
	//

	PIMAGE_RELOCATION Relocs = (PIMAGE_RELOCATION) pRelocs;

	for (unsigned i=0; i<cv->relocs.size(); i++)
		Relocs[i] = cv->relocs[i];

	//
	// Succeeded
	//

	return true;
}

//
// Add a relocation to the relocation table.
//

void CvCoffAddRelocation (PCV_COFF_DEBUG cv, DWORD addr, DWORD symind, WORD type)
{
	IMAGE_RELOCATION Reloc = {{addr},symind,type};
	cv->relocs.push_back (Reloc);
}

//
// Destroy CodeView info
//

void CvCoffDestroy (PCV_COFF_DEBUG cv)
{
	free (cv->pData);
	delete cv;
}

//
// Append data to CV syms
//

bool CvCoffAppendData (PCV_COFF_DEBUG cv, void* pb, size_t cb)
{
	if (cv->cbData + cb > cv->cbMax)
		return false;

	memcpy (cv->pFreeSymRec, pb, cb);
	cv->pFreeSymRec += cb;
	cv->cbData += cb;
	return true;
}

//
// Convert COFF symbols to CodeView Symbol Information
//

CVOBJNAME CvObjName = {0, S_OBJNAME_ST, 0};

CVCOMPILE2 CvCompile2 = {
	0,
	S_COMPILE2_ST, 
	CV_CFL_CXX, 
	CV_CFL_80386,
	{0,0,0},
	{0,1,0}
};

bool CvConvertCoffToCv (char* szObjFile)
{
	//
	// Open file, get file size, allocate temp buffer
	//

	HANDLE hObj = CreateFile (szObjFile, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
	
	if (hObj == INVALID_HANDLE_VALUE)
		return printf("Error: can't open input file (last error %lx)\n", GetLastError()), false;

	size_t ObjSize = GetFileSize (hFile, 0);
	void* pObj = malloc (ObjSize);

	if (!pObj)
	{
		printf("Error: Can't allocate %lx bytes for .obj\n", ObjSize);
		CloseHandle (hObj);
		return false;
	}

	//
	// Read-in current .obj file contents
	//

	DWORD dwRead;
	if (!ReadFile (hFile, pObj, ObjSize, &dwRead, 0) || dwRead < ObjSize)
	{
		printf("Error: can't read .obj\n");
		free (pObj);
		CloseHandle (hObj);
		return false;
	}

	//
	// Rewind the file pointer.
	//

	SetFilePointer (hFile, 0, 0, FILE_BEGIN);

	//
	// Parse .obj and update
	//

	PIMAGE_FILE_HEADER FileHdr = (PIMAGE_FILE_HEADER) pObj;
	PIMAGE_SYMBOL SymbolTable = (PIMAGE_SYMBOL) ((PSZ)pObj + FileHdr->PointerToSymbolTable);
	size_t cSyms = FileHdr->NumberOfSymbols;

	IMAGE_SECTION_HEADER DebugSection = {0};
	SIZE_T cbRelocs = cSyms*2*sizeof(IMAGE_RELOCATION);
	PVOID pRelocs = malloc (cbRelocs);

	SIZE_T cbData = 0;
	PVOID pData = 0;

	PCV_COFF_DEBUG cv = 0;

	cv = CvCoffCreateDebugInfo (CV_SIGNATURE_C8, 256*cSyms + 256);

	//
	// Add OBJNAME symbol
	//

	size_t cbCvObjName = sizeof(CVOBJNAME) + 1 + strlen(szObjName);
	PBYTE pSym = new BYTE[cbCvObjName];
	((PSYMTYPE)pSym)->reclen = cbCvObjName - sizeof(WORD);
	*((PCVOBJNAME)pSym) = CvObjName;
	PBYTE pObjNameStr = (PBYTE) ((PCVOBJNAME)pSym + 1);
	pObjNameStr[0] = strlen(szObjName);
	memcpy (&pObjNameStr[1], szObjName, pObjNameStr[0]);

	size_t offset = CvCoffAddSymRec (cv, (PSYMTYPE)pSym);
	
	delete pSym;

	//
	// Add COMPILE2 symbol
	//

	const char* szBuilderVer = "FASM Compiler by Tomasz Grysztar; FASM2COFF Converter + COFF2CV Converter by Great";
	size_t cbCvCompile2 = sizeof(CvCompile2) + 1 + strlen(szBuilderVer);
	pSym = new BYTE[cvCvCompile2];
	((PSYMTYPE)pSym)->reclen = cbCvCompile2 - sizeof(WORD);
	*((PCVCOMPILE2)pSym) = CvCompile2;
	PBYTE pBuilderStr = (PBYTE)((PCVCOMPILE2)pSym + 1);
	pBuilderStr[0] = strlen(szBuilderVer);
	memcpy (&pBuilderStr[1], szBuilderVer, pBuilderStr[0]);

	offset = CvCoffAddSymRec (cv, (PSYMTYPE)pSym);
	delete pSym;

	//
	// Convert COFF symbol records into CV symbol records
	//

	for (unsigned i=0; i<cSyms; i++)
	{
		DATASYM32 CvDataSym =  {0, S_LDATA32_ST, 0, 0, 0};
		char* szName = CoffSTGetSymbolName (
	}


	//
	// Destroy temp buffer & close the file.
	//

	free (pObj);
	CloseHandle (hFile);
}
