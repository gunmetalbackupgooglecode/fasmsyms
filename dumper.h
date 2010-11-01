#pragma once

//
// Dump Flags for DumpFasmSym(char* szSymFile, DWORD dwFlags)
//

#define DUMP_HEADER   0x0001
#define DUMP_STRINGS  0x0002
#define DUMP_SYMBOLS  0x0004
#define DUMP_PS_LINES 0x0008
#define DUMP_ASM_DUMP 0x0010
#define DUMP_SECTIONS 0x0020
#define DUMP_ALL      0x003f

int DumpFasmSym  (char* szSymFile, DWORD dwFlags);
