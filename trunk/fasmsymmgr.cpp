#include <windows.h>
#include <stdio.h>
#include <common/mapping.h>

#include "dumper.h"
#include "converter.h"

//
// Display help
//

int help()
{
	printf("Usage: FASMSYMMGR [options] [type-options] [input file] [output file]\n\n"
		"Type Options: (select type of the task)\n"
		" /TYPE:DUMP <fasm-symbols-file>                  Dump FASM symbols file\n"
		" /TYPE:CONV2COFF <fasm-symbols> <coff-object>    Convert FASM symbols to COFF symbols and add them to the specified OBJ\n"
		//" /TYPE:CONV2EXE  <fasm-symbols> <exe-file>       Convert FASM symbols to COFF symbols and add them to the specified EXE\n"
		" /HELP or /? or -?                          Display this help\n"
		"\n"
		"For the /TYPE:DUMP option one or more of the following dump options should be preceded:\n"
		" /HEADER      Include FASM Symbol File Header in the dump\n"
		" /STRINGS     Include string table in the dump\n"
		" /SYMBOLS     Include symbol table in the dump\n"
		" /PSLINES     Include preprocessed source lines in the dump\n"
		" /ASMDUMP     Include assembly dump in the dump\n"
		" /SECTIONS    Include section names in the dump\n"
		" /ALL         Dump all (equivalent for /HEADER /STRINGS /SYMBOLS /PSLINES /ASMDUMP /SECTIONS)\n"
		"\n"
		"Examples:\n"
		" fasmsymmgr /HEADER /ASMDUMP /TYPE:DUMP test.sym\n"
		" fasmsymmgr /TYPE:CONV2COFF test.sym test.obj\n"
		" fasmsymmgr /TYPE:CONV2EXE test.sym test.exe\n"
		"\n"
		"This is an 'as-is' open-source software. Program is licensed under MIT license\n"
		"(C) Great, 2010. All rights reserved.\n\n"
		);

	return 0;
}

//
// FasmSymMgr main
//

int main (int argc, char** argv)
{
	char *szSymFile = 0;
	char *szOutputFile = 0;
	DWORD dwDumpFlags = 0;

	printf("FasmSymMgr FASM symbol file manager v0.1 by (C) Great, 2010.\n"
		"For help type: fasmsymmgr /? (or /HELP)\n\n");

	//
	// Parse cmdline args
	//

	for (int i=1; i<argc; i++)
	{
		if (!_stricmp (argv[i], "/STRINGS"))
		{
			dwDumpFlags |= DUMP_STRINGS;
		}
		else if (!_stricmp (argv[i], "/SYMBOLS"))
		{
			dwDumpFlags |= DUMP_SYMBOLS;
		}
		else if (!_stricmp (argv[i], "/HEADER"))
		{
			dwDumpFlags |= DUMP_HEADER;
		}
		else if (!_stricmp (argv[i], "/PSLINES"))
		{
			dwDumpFlags |= DUMP_PS_LINES;
		}
		else if (!_stricmp (argv[i], "/ASMDUMP"))
		{
			dwDumpFlags |= DUMP_ASM_DUMP;
		}
		else if (!_stricmp (argv[i], "/SECTIONS"))
		{
			dwDumpFlags |= DUMP_SECTIONS;
		}
		else if (!_stricmp (argv[i], "/ALL"))
		{
			dwDumpFlags |= DUMP_ALL;
		}

		if (!_strnicmp (argv[i], "/TYPE:", 6))
		{
			if (!_stricmp (&argv[i][6], "DUMP"))
			{
				if (i == argc-1)
					return printf("fasmsymmgr: /DUMP: input file name required\n");

				printf("Type: dump FASM symbols file\n");
				printf("Dumping Flags:", dwDumpFlags);
				if ((dwDumpFlags & DUMP_ALL) == DUMP_ALL) printf("  ALL\n");
				else
				{
					if (dwDumpFlags & DUMP_HEADER) printf("  HEADER\n");
					if (dwDumpFlags & DUMP_STRINGS) printf("  STRING TABLE\n");
					if (dwDumpFlags & DUMP_SYMBOLS) printf("  SYMBOL TABLE\n");
					if (dwDumpFlags & DUMP_PS_LINES) printf("  PS LINES\n");
					if (dwDumpFlags & DUMP_ASM_DUMP) printf("  ASSEMBLY DUMP\n");
					if (dwDumpFlags & DUMP_SECTIONS) printf("  SECTION NAMES\n");
				}

				szSymFile = argv[i+1];

				printf("FASM Symbols: %s\n", szSymFile);
				printf("\n");

				return DumpFasmSym (szSymFile, dwDumpFlags);
			}
			else if (!_stricmp (&argv[i][6], "CONV2COFF"))
			{
				if (argc < i+2)
					return printf("fasmsymmgr: /TYPE:CONV2COFF: input and output file names are required\n");

				szSymFile = argv[i+1];
				szOutputFile = argv[i+2];
				printf("Type: convert FASM symbols to COFF obj\n");
				printf("FASM Symbols: %s\n", szSymFile);
				printf("Output COFF .obj: %s\n", szOutputFile);
				printf("\n");
				return FasmSym2CoffObj (szSymFile, szOutputFile);
			}
// 			else if (!_stricmp (&argv[i][6], "CONV2EXE"))
// 			{
// 				if (argc < i+2)
// 					return printf("fasmsymmgr: /TYPE:CONV2EXE: input and output file names are required\n");
// 
// 				szSymFile = argv[i+1];
// 				szOutputFile = argv[i+2];
// 				printf("Type: convert FASM symbols to PE EXE\n");
// 				printf("FASM Symbols: %s\n", szSymFile);
// 				printf("Output PE EXE: %s\n", szOutputFile);
// 				printf("\n");
// 				return FasmSym2CoffExe (szSymFile, szOutputFile);
// 			}
			else if (!_stricmp (argv[i], "/HELP") || !_stricmp (argv[i], "/?") || !_stricmp (argv[i], "-?"))
			{
				return help();
			}
		}
	}

	// no arguments
	return help();
}
