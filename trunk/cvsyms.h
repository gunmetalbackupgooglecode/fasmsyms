#pragma once

#include <vector>
#include <string>

typedef struct _SYMTYPE SYMTYPE, *PSYMTYPE;

#define CV_COFF_SYMBOLS_C7   1
#define CV_COFF_SYMBOLS_C8   2

// COFF CodeView debugging info (.debug$S section)
typedef struct CV_COFF_DEBUG
{
	BYTE bCvCoffSig;

	PVOID pData;
	SIZE_T cbData;

	SIZE_T cbMax;

	SIZE_T cSyms;
	PBYTE pFreeSymRec;

	std::vector<IMAGE_RELOCATION> relocs;

	CV_COFF_DEBUG (BYTE sig = CV_COFF_SYMBOLS_C8, size_t cb = 0)
		: bCvCoffSig(sig),
			pData(0),
			cbData(0),
			cbMax(cb),
			cSyms(0),
			pFreeSymRec(0)
	{
	}

} CV_COFF_DEBUG, *PCV_COFF_DEBUG;

#define CV_DATA(cv) ((cv)->pData)

PCV_COFF_DEBUG CvCoffCreateDebugInfo (BYTE bCvCoffSig = CV_COFF_SYMBOLS_C8, size_t maxLength = 0);

size_t CvCoffAddSymRec (PCV_COFF_DEBUG cv, PSYMTYPE Symbol);

bool CvCoffAppendData (PCV_COFF_DEBUG cv, void* pb, size_t cb);

void CvCoffAddRelocation (PCV_COFF_DEBUG cv, DWORD addr, DWORD symind, WORD type);

bool CvCoffEmitDebugInfo (
	PCV_COFF_DEBUG cv, 
	PVOID pBuffer, 
	SIZE_T *pcbBuffer, 
	PIMAGE_SECTION_HEADER pSection,
	PVOID pRelocs,
	SIZE_T *pcbRelocs
	);

void CvCoffDestroy (PCV_COFF_DEBUG cv);

bool CvConvertCoffToCv (char* szObjFile);

typedef struct CVOBJNAME {
	WORD wLen;
	WORD wType;
	DWORD dwSignature;
} CVOBJNAME, *PCVOBJNAME;

typedef struct CVCOMPILE2 {
	WORD wLen;
	WORD wType;
	DWORD Language;
	WORD machine;
	WORD verFE[3]; // major,minor,build
	WORD ver[3];   // major,minor,build
} CVCOMPILE2, *PCVCOMPILE2;
