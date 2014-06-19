#include <stdio.h>
#include <string.h>
#include "maindef.h"
#include "mvd_parser.h"
#include "frag_parser.h"
#include "logger.h"

cmdline_params_t cmdargs;

void ShowHelp(char *filename)
{
	printf("MVD Parser, version 0.1\n");
	printf("(c) Joakim S?derberg\n");
	printf("\n");
	printf("Usage:\n");
	printf("  %s [-f <fragfile>] [-t <template>] [-v[v[v]]] <demoname>\n", filename);
	printf("\n");
	printf("Options:\n");
	printf("    -f: load your own defined fragfile (fragfile.dat by default)\n");
	printf("    -t: load your own defined log template (template.dat by default)\n");
	printf("    -v: script debugging (use -vv or -vvv for extra verbosity)\n");
	printf("\n");
}

qbool Cmdline_Parse(int argc, char **argv)
{
	int i;
	char *files_temp[1024];
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
				case 'f' :
				{
					int next_arg = (i + 1);

					if (next_arg < argc)
					{
						cmdargs.frag_file = Q_strdup(argv[next_arg]);
						i++;
					}
					else
					{
						Sys_PrintError("-t: No fragfile specified\n");
					}

					break;
				}
				case 't' :
				{
					int next_arg = (i + 1);

					if (next_arg < argc)
					{
						cmdargs.template_file = Q_strdup(argv[next_arg]);
						i++;
					}
					else
					{
						Sys_PrintError("-t: No template specified\n");
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

void CmdArgs_Clear(void)
{
	int i;

	for (i = 0; i < cmdargs.mvd_files_count; i++)
	{
		Q_free(cmdargs.mvd_files[i]);
	}

	Q_free(cmdargs.template_file);
	Q_free(cmdargs.frag_file);
}

int main(int argc, char **argv)
{
	int i;
	byte *mvd_data = NULL;
	long mvd_len = 0;

	Sys_InitDoubleTime();
	LogVarHashTable_Init();

	if (!Cmdline_Parse(argc, argv))
	{
		ShowHelp(argv[0]);
		return 1;
	}

	if (!Log_ParseOutputTemplates(&logger, cmdargs.template_file) && !Log_ParseOutputTemplates(&logger, "template.dat"))
	{
		Sys_PrintError("Failed to load template file.\n");
		return 1;
	}

	if (!LoadFragFile(cmdargs.frag_file, false) && !LoadFragFile("fragfile.dat", false))
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
			char *demopath = cmdargs.mvd_files[i];

			// Parse the demo.
			Sys_PrintDebug(1, "Starting to parse %s\n", cmdargs.mvd_files[i]);
			MVD_Parser_StartParse(demopath, mvd_data, mvd_len);
		}

		Q_free(mvd_data);
	}

	Log_ClearLogger(&logger);
	CmdArgs_Clear();

	return 0;
}

