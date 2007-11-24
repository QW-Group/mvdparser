#include <stdio.h>
#include <string.h>
#include "maindef.h"
#include "mvd_parser.h"
#include "frag_parser.h"

cmdline_params_t cmdargs;

void ShowHelp(char *filename)
{
	printf("MVD Parser version 0.1 (c) Joakim Söderberg\n");
}

qbool Cmdline_Parse(int argc, char **argv)
{
	int i;
	char **files_temp[1024];
	int filecount = 0;
	char *arg;

	memset(&cmdargs, 0, sizeof(cmdargs));

	if (argc < 2)
	{
		return false;
	}

	for (i = 1; i < argc; i++)
	{
		arg = argv[i];

		if (((arg[0] == '-') || (arg[0] == '/')) && arg[1])
		{
			// Command line switch.
			switch(arg[1])
			{
				case 'v' :
				{
					// Verbosity based on how many v's where specified.
					int j = 1;
					
					while (arg[j] == 'v')
					{
						cmdargs.debug++;
						j++;						
					}	
					break;
				}
			}
		}
		else
		{
			// Regard as being a file.
			files_temp[filecount] = arg;
			filecount++;
		}
	}

	cmdargs.mvd_files_count = filecount;

	// Allocate memory for the filenames.
	if (filecount > 0)
	{
		cmdargs.mvd_files = (char **)Q_calloc(filecount, sizeof(char **));

		for (i = 0; i < filecount; i++)
		{
			cmdargs.mvd_files[i] = Q_strdup(files_temp[i]);
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	int i;
	byte *mvd_data = NULL;
	long mvd_len = 0;

	Sys_InitDoubleTime();

	if (!Cmdline_Parse(argc, argv))
	{
		ShowHelp(argv[0]);
		return 1;
	}

	if (!LoadFragFile("c:\\fragfile.dat", false))
	{
		Sys_PrintError("Failed to load fragfile.dat\n");
		return 1;
	}

	for (i = 0; i < cmdargs.mvd_files_count; i++)
	{
		// Read the mvd demo file.
		if (!COM_ReadFile(cmdargs.mvd_files[i], &mvd_data, &mvd_len))
		{
			Sys_PrintError("Failed to read %s.\n", cmdargs.mvd_files[i]);
		}
		else
		{
			// Parse the demo.
			Sys_Print("Starting to parse %s\n", cmdargs.mvd_files[i]);
			MVD_Parser_StartParse(mvd_data, mvd_len);
		}

		Q_free(mvd_data);
	}

	return 0;
}

