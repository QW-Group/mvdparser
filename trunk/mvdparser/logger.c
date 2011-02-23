#include <stdio.h>
#include <string.h>
#include "maindef.h"
#include "qw_protocol.h"
#include "logger.h"

logger_t logger;

char *LogVarValueAsString(mvd_info_t *mvd, const char *varname, int player_num);

#define LOG_CHECK_NUMARGS(num)																						\
{																													\
	if (arg_count != num)																							\
	{																												\
		Sys_PrintError("Output templates parsing: %i tokens expected %i found (line %i)\n", num, arg_count, line);	\
		line_start = line_end + 1;																					\
		continue;																									\
	}																												\
}

char *Armor_Name(int num)
{
	switch (num)
	{
		case GREEN_ARMOR :	return "GA";
		case YELLOW_ARMOR :	return "YA";
		case RED_ARMOR :	return "RA";
	}

	return NULL;
}

char *WeaponFlagToName(int flag)
{
	switch (flag) 
	{
		case IT_AXE:				return "AXE";
		case IT_SHOTGUN:			return "SG";
		case IT_SUPER_SHOTGUN:		return "SSG";
		case IT_NAILGUN:			return "NG";
		case IT_SUPER_NAILGUN:		return "SNG";
		case IT_GRENADE_LAUNCHER:	return "GL";
		case IT_ROCKET_LAUNCHER:	return "RL";
		case IT_LIGHTNING:			return "LG";
		default:					return NULL;
	}
}

char *WeaponNumToName(int num)
{
	switch (num) 
	{
		case AXE_NUM:		return "AXE";
		case SG_NUM:		return "SG";
		case SSG_NUM:		return "SSG";
		case NG_NUM:		return "NG";
		case SNG_NUM:		return "SNG";
		case GL_NUM:		return "GL";
		case RL_NUM:		return "RL";
		case LG_NUM:		return "LG";
		default:			return NULL;
	}
}

static void Log_ParseFileLine(logger_t *logger, int line)
{
	// Allocate memory for the template.
	log_outputfile_template_t *output_template = Q_calloc(1, sizeof(log_outputfile_template_t));

	// Save 1st arg as the template ID.
	strlcpy(output_template->id, Cmd_Argv(1), sizeof(output_template->id));
	
	// Save 2nd arg as template name.
	output_template->name = Q_strdup(Cmd_Argv(2));

	// Add the template to the array of output file templates.
	logger->output_file_template_count++;
	logger->output_file_templates = Q_realloc(logger->output_file_templates, (logger->output_file_template_count * sizeof(*logger->output_file_templates)));

	logger->output_file_templates[logger->output_file_template_count - 1] = output_template;
}

static log_eventlogger_type_t Log_ParseEventloggerType(const char *event_logger_type)
{
	if (!strcasecmp(event_logger_type, "DEATH"))
	{
		return LOG_DEATH;
	}
	else if (!strcasecmp(event_logger_type, "MOVE"))
	{
		return LOG_MOVE;
	}
	else if (!strcasecmp(event_logger_type, "MATCHSTART"))
	{
		return LOG_MATCHSTART;
	}
	else if (!strcasecmp(event_logger_type, "MATCHEND"))
	{
		return LOG_MATCHEND;
	}
	else if (!strcasecmp(event_logger_type, "MATCHEND_ALL"))
	{
		return LOG_MATCHEND_ALL;
	}
	else if (!strcasecmp(event_logger_type, "DEMOSTART"))
	{
		return LOG_DEMOSTART;
	}
	else if (!strcasecmp(event_logger_type, "DEMOEND"))
	{
		return LOG_DEMOEND;
	}
	else if (!strcasecmp(event_logger_type, "SPAWN"))
	{
		return LOG_SPAWN;
	}
	else if (!strcasecmp(event_logger_type, "ITEMPICKUP"))
	{
		return LOG_ITEMPICKUP;
	}

	return LOG_UNKNOWN;
}

static qbool Log_TokenizeNextLine(char **t_line_start, char **t_line_end)
{
	char *line_start = NULL;
	char *line_end = NULL;
	char line_end_save	= '\n';	// The previous end of line character.

	// Make sure we have something to work with.
	if (!t_line_start || !t_line_end)
	{
		return false;
	}

	line_start = (*t_line_start);
	line_end = (*t_line_end);

	// Find the end of the line and terminate it so that we can tokenize it.
	for (line_end = line_start; *line_end && (*line_end != '\n'); line_end++) ;

	// Save the last char before terminating so that we can make it fine again after tokenizing.
	line_end_save = *line_end;
	(*line_end) = 0;

	// Tokenize the current line.
	Cmd_TokenizeString(line_start);

	// Restore the line ending.
	(*line_end) = line_end_save;

	// Return the new pointers.
	(*t_line_start) = line_start;
	(*t_line_end) = line_end;

	return true;
}

static void Log_ClearEventLogger(log_eventlogger_t *event_logger)
{
	Q_free(event_logger->output_template_string);
	Q_free(event_logger->outputfile_templates);
	memset(event_logger, 0, sizeof(log_eventlogger_t));
}

static void Log_ClearOutputFileTemplate(log_outputfile_template_t *file_template)
{
	Q_free(file_template->name);
	memset(file_template, 0, sizeof(*file_template));
}

void Log_ClearLogger(logger_t *logger)
{
	int i;

	for (i = 0; i < logger->event_logger_count; i++)
	{
		Log_ClearEventLogger(logger->event_loggers[i]);
		Q_free(logger->event_loggers[i]);
	}

	for (i = 0; i < logger->output_file_template_count; i++)
	{
		Log_ClearOutputFileTemplate(logger->output_file_templates[i]);
		Q_free(logger->output_file_templates[i]);
	}

	memset(logger, 0, sizeof(logger_t));
}

qbool Log_ParseOutputTemplates(logger_t *logger, const char *template_file)
{
	#define LOG_NEXTLINE() { if (!(*line_end)) break; line_start = (line_end + 1); continue; } 

	int eventlogger_count	= 0;
	int file_template_count	= 0;
	byte *text				= NULL;
	char *line_start		= NULL; // Start of the current line.
	char *line_end			= NULL; // End of the current line.
	long filelen			= 0;
	int line				= 0;
	int arg_count			= 0;

	Log_ClearLogger(logger);

	if (!template_file)
	{
		return false;
	}

	// Read the contents of the template file.
	if (!COM_ReadFile(template_file, &text, &filelen))
	{
		Sys_PrintError("Error! Output templates parsing: Failed to read from file %s\n", template_file);
		return false;
	}

	line_start = text;
	line_end = text;

	// Count the number of events and filename templates so that we can allocate enough memory.
	while (*line_end)
	{
		// Tokenize the next line.
		if (!Log_TokenizeNextLine(&line_start, &line_end))
		{
			Sys_PrintError("Error! Output templates parsing: Failed to tokenize line, NULL pointers passed when parsing %s.\n", template_file);
			return false;
		}

		// No tokens on this line.
		if (!(arg_count = Cmd_Argc()))
		{
			LOG_NEXTLINE();
		}

		if (!strcasecmp(Cmd_Argv(0), "#FILE"))
		{
			file_template_count++;
		}
		else if (!strcasecmp(Cmd_Argv(0), "#EVENT"))
		{
			eventlogger_count++;
		}

		LOG_NEXTLINE();
	}

	// Allocate memory.
	logger->event_loggers = Q_calloc(eventlogger_count, sizeof(log_eventlogger_t));
	logger->output_file_templates = Q_calloc(file_template_count, sizeof(log_outputfile_template_t));

	line_start = text;
	line_end = text;

	while (*line_end)
	{
		line++;

		// Tokenize the next line.
		if (!Log_TokenizeNextLine(&line_start, &line_end))
		{
			Sys_PrintError("Error! Output templates parsing: Failed to tokenize line, NULL pointers passed when parsing %s.\n", template_file);
			return false;
		}

		// No tokens on this line.
		if (!(arg_count = Cmd_Argc()))
		{
			LOG_NEXTLINE();
		}
	
		if (!strcasecmp(Cmd_Argv(0), "#FILE"))
		{
			LOG_CHECK_NUMARGS(3);
			Log_ParseFileLine(logger, line);
		}
		else if (!strcasecmp(Cmd_Argv(0), "#EVENT"))
		{
			char line_end_save = '\n';
			log_eventlogger_t *eventlogger = NULL;

			LOG_CHECK_NUMARGS(3);

			// Allocate memory for an eventlogger.
			eventlogger = (log_eventlogger_t *)Q_calloc(1, sizeof(log_eventlogger_t));

			// Get the event type.
			eventlogger->type = Log_ParseEventloggerType(Cmd_Argv(1));

			// Get ID.
			eventlogger->id = atoi(Cmd_Argv(2));

			// From the next line, read everything and save it as a string until an #EVENT_END token is found.
			{
				#define EVENT_END_STR "#EVENT_END"
				size_t event_end_len = strlen(EVENT_END_STR);

				line_start = line_end; // + 1;

				for (line_end = line_start; *line_end; line_end++)
				{
					if (*line_end == '\n')
					{
						line++; // Could've used strstri, but we want to maintain the line count :)
					}

					if (!strncasecmp(EVENT_END_STR, line_end, event_end_len))
					{
						// Go back to the previous char so we don't include the #.
						line_end--;
						break;
					}
				}

				line_end_save = (*line_end);
				(*line_end) = 0;

				// Copy the template substring to the event logger.
				eventlogger->output_template_string = Q_strdup(line_start);

				(*line_end) = line_end_save;
			}

			// Add the event logger to the list of event loggers.
			logger->event_loggers[logger->event_logger_count] = eventlogger;
			logger->event_logger_count++;
		}
		else if (!strcasecmp(Cmd_Argv(0), "#OUTPUT"))
		{
			int i										= 0;
			int event_id								= 0;
			char *file_template_id						= NULL;
			log_outputfile_template_t *file_template	= NULL;
			log_eventlogger_t *eventlogger				= NULL;

			LOG_CHECK_NUMARGS(3);

			// Read the ID of the event logger.
			event_id = atoi(Cmd_Argv(1));

			// Read the file template ID.
			file_template_id = Cmd_Argv(2);

			// Find the file template with the specified ID.
			for (i = 0; i < logger->output_file_template_count; i++)
			{
				if (!strcmp(logger->output_file_templates[i]->id, file_template_id))
				{
					// We've found the template.
					file_template = logger->output_file_templates[i];
					break;
				}
			}

			// We couldn't find a file template with this ID, so skip to next row.
			if (!file_template)
			{
				Sys_PrintError("Warning! Output templates parsing: #OUTPUT: Found no output file template with id \"%s\" (line %i)\n", file_template_id, line);
				LOG_NEXTLINE();
			}

			// Find the event logger with the found ID.
			for (i = 0; i < logger->event_logger_count; i++)
			{
				if (logger->event_loggers[i]->id == event_id)
				{
					eventlogger = logger->event_loggers[i];
					break;
				}
			}

			if (!eventlogger)
			{
				Sys_PrintError("Warning! Output templates parsing: #OUTPUT: Found no event logger with id %i (line %i)\n", event_id, line);
				LOG_NEXTLINE();
			}

			// Add the file template to the list of templates in the event logger.
			// Reallocate the array to fit a pointer to the new template.
			eventlogger->templates_count++;
			eventlogger->outputfile_templates = Q_realloc(eventlogger->outputfile_templates, (eventlogger->templates_count * sizeof(log_outputfile_template_t)));
			eventlogger->outputfile_templates[eventlogger->templates_count - 1] = file_template;
		}

		LOG_NEXTLINE();
	}

	if (logger->event_logger_count != eventlogger_count)
	{
		Sys_PrintError("Error! Output templates parsing: Expected %i #EVENTs, only found %i\n", eventlogger_count, logger->event_logger_count);
		Log_ClearLogger(logger);
		return false;
	}

	if (logger->output_file_template_count != file_template_count)
	{
		Sys_PrintError("Error! Output templates parsing: Expected %i #FILEs, only found %i\n", file_template_count, logger->output_file_template_count);
		Log_ClearLogger(logger);
		return false;
	}

	return true;
}

char *Log_ExpandTemplateString(logger_t *logger, mvd_info_t *mvd, const char *template_string, int player_num, int *output_len)
{
	char var_value[1024];
	char tmp[1024];
	int expand_count		= 0;
	char *vv				= NULL;
	const char *var_start	= NULL;
	const char *var_end		= NULL;
	const char *input		= NULL;
	char *output			= NULL;
	size_t template_len		= strlen(template_string) * 3;

	//strlcpy(template_str, template_string, sizeof(template_str));
	input = template_string;

	// Expand the string by going through it and resolving any variables in it.
	if ((int)template_len > logger->expand_buf_size)
	{
		logger->expand_buf_size = (int)template_len;
		logger->expand_buf = (char *)Q_realloc(logger->expand_buf, logger->expand_buf_size);
	}

	output = logger->expand_buf;

	while (*input && (expand_count < logger->expand_buf_size))
	{
		// We found a variable name.
		if (*input == '%')
		{
			input++;
			
			// Only look vor a var name if it's not zero length.
			if (*input != '%')
			{
				var_start = input;

				// Find the end of the variable name.
				while (*input && (*input != '%'))
				{
					input++;
				}

				var_end = input;

				// No end %, incorrect template.
				if (*var_end != '%')
				{
					Sys_Error("Log_ExpandTemplateString: Incorrectly terminated var.\n");
				}

				var_end++; // Skip the trailing % in the output.

				// Expand the variable based on the current mvd context / player.
				snprintf(tmp, min((var_end - var_start), sizeof(tmp)), "%s", var_start);
				strlcpy(var_value, LogVarValueAsString(mvd, tmp, player_num), sizeof(var_value));

				// Add the value to the output string.
				vv = var_value;

				while (*vv && (expand_count < logger->expand_buf_size))
				{
					// TODO : We might cut off a vars value here, but that's not very likely :)
					*output = *vv;
					expand_count++;

					output++;
					vv++;
				}

				input = var_end;
			}
		}

		// Keep count on how many chars we have expanded so that we don't go past the bounds of the array.
		expand_count++;

		// Grow the array.
		if (expand_count >= logger->expand_buf_size)
		{
			logger->expand_buf_size *= 2;
			logger->expand_buf = (char *)Q_realloc(logger->expand_buf, logger->expand_buf_size);
			output = logger->expand_buf;
		}

		*output = *input;

		output++;
		input++;
	}

	*output = 0;
	
	if (output_len)
	{
		*output_len = expand_count;
	}

	return logger->expand_buf;
}

log_outputfile_t *Log_OutputFilesHashTable_GetValue(logger_t *logger, const char *expanded_filename)
{
	unsigned long filename_hash = Com_HashKey(expanded_filename); 
	log_outputfile_t *it = (log_outputfile_t *)logger->output_hashtable[filename_hash % LOG_OUTPUTFILES_HASHTABLE_SIZE];

	while (it && (it->filename_hash != filename_hash))
	{
		it = it->next;
	}

	return it;
}

void Log_OutputFilesHashTable_AddValue(logger_t *logger, log_outputfile_t *output_file)
{
	log_outputfile_t *it = NULL;
	unsigned long filename_hash;

	output_file->filename_hash = Com_HashKey(output_file->filename);
	filename_hash = (output_file->filename_hash % LOG_OUTPUTFILES_HASHTABLE_SIZE);
	it = logger->output_hashtable[filename_hash];

	if (!it)
	{
		logger->output_hashtable[filename_hash] = output_file;
	}
	else
	{
		while (it->next)
		{
			it = it->next;
		}

		it->next = output_file;
	}
}

void Log_Event(logger_t *logger, mvd_info_t *mvd, log_eventlogger_type_t type, int player_num)
{
	int i;
	int j;
	int p;
	int player_start				= player_num;
	int player_count				= 1;
	int output_len					= 0;
	char *output					= NULL;
	char *expanded_filename			= NULL;
	log_eventlogger_t *eventlogger	= NULL;
	log_outputfile_t *output_file	= NULL;

/*
	int num_written_to				= 0;
	qbool skip						= false;
	log_outputfile_t *written_to[128]; // An array containing the files we've already written to. HACK HACK!
	int n;
	
	memset(written_to, 0, sizeof(written_to));

	// If this event isn't directed at any particular player we still need to expand
	// player-specific variables in the filenames, since the event should be registered
	// in all files, even if they're player specific.
	if (player_num < 0)
	{
		player_start = 0;
		player_count = 32;
	}*/

	for (i = 0; i < logger->event_logger_count; i++)
	{
		eventlogger = logger->event_loggers[i];

		// Find all event loggers that has the type specified.
		if (eventlogger->type != type)
		{
			continue;
		}

		// Expand the event loggers template string into an output string.
		output = Q_strdup(Log_ExpandTemplateString(logger, mvd, eventlogger->output_template_string, player_num, &output_len));

		// See the above comment for setting player_count for explination.
		// TODO : Make this more readable by making the stuff inside the for-loop a separate function, and calling it in two separate if-cases instead.
		for (p = player_start; p < (player_start + player_count); p++)
		{
			if (!PLAYER_ISVALID(&mvd->players[p]))
			{
				continue;
			}

			// Loop through and expand the filename template strings for this event logger
			// and then write the output to the files.
			for (j = 0; j < eventlogger->templates_count; j++)
			{
				expanded_filename = Log_ExpandTemplateString(logger, mvd, eventlogger->outputfile_templates[j]->name, p, NULL);

				// Search for the expanded filename in the output_hashtable.
				output_file = Log_OutputFilesHashTable_GetValue(logger, expanded_filename);

				// If no open file was found, open it and add it to the hash table.
				if (!output_file)
				{
					output_file = Q_calloc(1, sizeof(*output_file));
					output_file->filename = Q_strdup(expanded_filename);
					output_file->file = fopen(expanded_filename, "w");

					if (!output_file->file)
					{
						Q_free(output_file->filename);
						Q_free(output_file);
						Q_free(output);
						Sys_Error("Log_Event: Failed to open the file %s (expanded from %s) for output.\n", output_file->filename, eventlogger->outputfile_templates[j]->name); 
					}

					Log_OutputFilesHashTable_AddValue(logger, output_file);
				}

				/*
				// Check so that we haven't written to this file yet.
				// TODO : HACK HACK HACK fix this in a nicer way :((
				{
					skip = false;

					for (n = 0; n < num_written_to; n++)
					{
						if (output_file == written_to[n])
						{
							skip = true;
							break;
						}
					}

					if (skip)
					{
						continue;
					}

					written_to[num_written_to] = output_file;
					num_written_to++;
				}*/

				if (output_len > 0)
				{
					// Write the expanded output to the file.
					fwrite(output, sizeof(char), output_len, output_file->file); 
					fflush(output_file->file);
				}
			}
		}
	}

	Q_free(output);
}

void Log_OutputFilesHashTable_Clear(logger_t *logger)
{
	int i;
	log_outputfile_t *tmp = NULL;
	log_outputfile_t *l	= NULL;

	for (i = 0; i < LOG_OUTPUTFILES_HASHTABLE_SIZE; i++)
	{
		l = logger->output_hashtable[i];
		
		while (l)
		{
			fflush(l->file);
			fclose(l->file);
			Q_free(l->filename);

			tmp = l->next;
			Q_free(l);
			l = tmp;
		}
	}
}

// ============================================================================
//								Log variables
// ============================================================================

static char *LogVar_name(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].name;
}

static char *LogVar_nameraw(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].name_raw;
}

static char *LogVar_team(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].team;
}

static char *LogVar_teamraw(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].team_raw;
}

static char *LogVar_userinfo(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].userinfo;
}

static char *LogVar_playernum(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", player_num);
}

static char *LogVar_animframe(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].frame);
}

static char *LogVar_playerid(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].userid);
}

static char *LogVar_frags(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].frags);
}

static char *LogVar_spectator(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].spectator);
}

static char *LogVar_health(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_HEALTH]);
}

static char *LogVar_armor(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ARMOR]);
}

static char *LogVar_activeweapon(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ACTIVEWEAPON]);
}

static char *LogVar_shells(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_SHELLS]);
}

static char *LogVar_nails(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_NAILS]);
}

static char *LogVar_rockets(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ROCKETS]);
}

static char *LogVar_cells(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_CELLS]);
}

static char *LogVar_quad(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_QUAD));
}

static char *LogVar_pent(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_INVULNERABILITY));
}

static char *LogVar_ring(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_INVISIBILITY));
}

static char *LogVar_sg(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SHOTGUN));
}

static char *LogVar_ssg(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SUPER_SHOTGUN));
}

static char *LogVar_ng(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_NAILGUN));
}

static char *LogVar_sng(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SUPER_NAILGUN));
}

static char *LogVar_gl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_GRENADE_LAUNCHER));
}

static char *LogVar_rl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_ROCKET_LAUNCHER));
}

static char *LogVar_lg(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_LIGHTNING));
}

static char *LogVar_pl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl);
}

static char *LogVar_avgpl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_average / mvd->players[player_num].pl_count);
}

static char *LogVar_maxpl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_highest);
}

static char *LogVar_minpl(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_lowest);
}

static char *LogVar_ping(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping);
}

static char *LogVar_avgping(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_average / mvd->players[player_num].ping_count);
}

static char *LogVar_maxping(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_highest);
}

static char *LogVar_minping(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_lowest);
}

static char *LogVar_gacount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[0]);
}

static char *LogVar_yacount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[1]);
}

static char *LogVar_racount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[2]);
}

static char *LogVar_ssgcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[0]);
}

static char *LogVar_ngcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[1]);
}

static char *LogVar_sngcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[2]);
}

static char *LogVar_glcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[3]);
}

static char *LogVar_rlcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[4]);
}

static char *LogVar_lgcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[5]);
}

static char *LogVar_ssgdrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[0]);
}

static char *LogVar_ngdrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[1]);
}

static char *LogVar_sngdrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[2]);
}

static char *LogVar_gldrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[3]);
}

static char *LogVar_rldrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[4]);
}

static char *LogVar_lgdrop(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[5]);
}

static char *LogVar_sgshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[1]);
}

static char *LogVar_ssgshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[2]);
}

static char *LogVar_ngshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[3]);
}

static char *LogVar_sngshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[4]);
}

static char *LogVar_glshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[5]);
}

static char *LogVar_rlshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[6]);
}

static char *LogVar_lgshots(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[7]);
}

static char *LogVar_deaths(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].death_count);
}

static char *LogVar_mhcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].megahealth_count);
}

static char *LogVar_quadcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].quad_count);
}

static char *LogVar_ringcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].ring_count);
}

static char *LogVar_pentcount(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].pent_count);
}

static char *LogVar_avgspeed(mvd_info_t *mvd, const char *varname, int player_num)
{
	if (mvd->players[player_num].speed_frame_count > 0)
	{
		return va("%f", mvd->players[player_num].acc_average_speed / mvd->players[player_num].speed_frame_count);
	}

	return "0";
}

static char *LogVar_maxspeed(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%f", mvd->players[player_num].speed_highest);
}

static char *LogVar_posx(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[0]);
}

static char *LogVar_posy(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[1]);
}

static char *LogVar_posz(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[2]);
}

static char *LogVar_pitch(mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[0]);
}

static char *LogVar_yaw(mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[1]);
}

static char *LogVar_roll(mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[2]);
}

static char *LogVar_distancemoved(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%f", mvd->players[player_num].distance_moved);
}

static char *LogVar_topcolor(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].topcolor);
}

static char *LogVar_bottomcolor(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].bottomcolor);
}

static char *LogVar_matchstartfulldate(mvd_info_t *mvd, const char *varname, int player_num)
{
	static char buf[128];
	//TODO: allow user defined format (my demos use  %Y%m%d-%H%M%S for example - deurk)
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &mvd->match_start_date_full);
	return buf;
}

static char *LogVar_matchstartdate(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->match_start_date);
}

static char *LogVar_matchstartmonth(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->match_start_month);
}

static char *LogVar_matchstartyear(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->match_start_year);
}

static char *LogVar_matchstarthour(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->match_start_hour);
}

static char *LogVar_matchstartminute(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->match_start_minute);
}

static char *LogVar_serverinfo(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.serverinfo;
}

static char *LogVar_demotime(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->demotime);
}

static char *LogVar_matchtime(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->demotime - mvd->match_start_demotime);
}

static char *LogVar_mvdframe(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->frame_count);
}

static char *LogVar_timelimit(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.timelimit);
}

static char *LogVar_fraglimit(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.fraglimit);
}

static char *LogVar_deathmatchmode(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.deathmatch);
}

static char *LogVar_watervis(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.watervis);
}

static char *LogVar_serverversion(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.serverversion;
}

static char *LogVar_maxclients(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.maxclients);
}

static char *LogVar_maxspectators(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.maxspectators);
}

static char *LogVar_fpd(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.fpd);
}

static char *LogVar_status(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.status;
}

static char *LogVar_teamplay(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.teamplay);
}

static char *LogVar_map(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.mapname;
}

static char *LogVar_demoname(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->demo_name;
}

static char *LogVar_droppedweapon(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].last_dropped_weapon);
}

static char *LogVar_droppedweaponstr(mvd_info_t *mvd, const char *varname, int player_num)
{
	return WeaponNumToName(mvd->players[player_num].last_dropped_weapon);
}

static char *LogVar_gamedir(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.gamedir;
}

static char *LogVar_maxfps(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.maxfps);
}

static char *LogVar_zext(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->serverinfo.zext);
}

static char *LogVar_hostname(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.hostname;
}

static char *LogVar_mod(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->serverinfo.mod;
}

static char *LogVar_client(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].client;
}

typedef char * (* logvar_func)(mvd_info_t *mvd, const char *varname, int player_num);

typedef enum logvartype_e
{
	LOGVAR_DEMO,
	LOGVAR_PLAYER
} logvartype_t;

typedef struct logvar_s
{
	char			*name;
	unsigned long	hash;
	logvar_func		func;
	logvartype_t	type;
	struct logvar_s	*next;
} logvar_t;

#define LOGVAR_DEFINE(name, type)	{#name, 0, LogVar_##name, type}

logvar_t logvar_list[] =
{
	// Demo variables.
	LOGVAR_DEFINE(demoname, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstartfulldate, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstartdate, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstartmonth, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstartyear, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstarthour, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchstartminute, LOGVAR_DEMO),
	LOGVAR_DEFINE(serverinfo, LOGVAR_DEMO),
	LOGVAR_DEFINE(timelimit, LOGVAR_DEMO),
	LOGVAR_DEFINE(fraglimit, LOGVAR_DEMO),
	LOGVAR_DEFINE(deathmatchmode, LOGVAR_DEMO),
	LOGVAR_DEFINE(watervis, LOGVAR_DEMO),
	LOGVAR_DEFINE(serverversion, LOGVAR_DEMO),
	LOGVAR_DEFINE(maxclients, LOGVAR_DEMO),
	LOGVAR_DEFINE(maxspectators, LOGVAR_DEMO),
	LOGVAR_DEFINE(fpd, LOGVAR_DEMO),
	LOGVAR_DEFINE(status, LOGVAR_DEMO),
	LOGVAR_DEFINE(teamplay, LOGVAR_DEMO),
	LOGVAR_DEFINE(demotime, LOGVAR_DEMO),
	LOGVAR_DEFINE(matchtime, LOGVAR_DEMO),
	LOGVAR_DEFINE(mvdframe, LOGVAR_DEMO),
	LOGVAR_DEFINE(map, LOGVAR_DEMO),
	LOGVAR_DEFINE(gamedir, LOGVAR_DEMO),
	LOGVAR_DEFINE(maxfps, LOGVAR_DEMO),
	LOGVAR_DEFINE(zext, LOGVAR_DEMO),
	LOGVAR_DEFINE(hostname, LOGVAR_DEMO),
	LOGVAR_DEFINE(mod, LOGVAR_DEMO),

	// Player specific variables.
	LOGVAR_DEFINE(client, LOGVAR_PLAYER),
	LOGVAR_DEFINE(name, LOGVAR_PLAYER),
	LOGVAR_DEFINE(nameraw, LOGVAR_PLAYER),
	LOGVAR_DEFINE(team, LOGVAR_PLAYER),
	LOGVAR_DEFINE(teamraw, LOGVAR_PLAYER),
	LOGVAR_DEFINE(userinfo, LOGVAR_PLAYER),
	LOGVAR_DEFINE(playernum, LOGVAR_PLAYER),
	LOGVAR_DEFINE(animframe, LOGVAR_PLAYER),
	LOGVAR_DEFINE(playerid, LOGVAR_PLAYER),
	LOGVAR_DEFINE(frags, LOGVAR_PLAYER),
	LOGVAR_DEFINE(spectator, LOGVAR_PLAYER),
	LOGVAR_DEFINE(health, LOGVAR_PLAYER),
	LOGVAR_DEFINE(armor, LOGVAR_PLAYER),
	LOGVAR_DEFINE(activeweapon, LOGVAR_PLAYER),
	LOGVAR_DEFINE(shells, LOGVAR_PLAYER),
	LOGVAR_DEFINE(nails, LOGVAR_PLAYER),
	LOGVAR_DEFINE(rockets, LOGVAR_PLAYER),
	LOGVAR_DEFINE(cells, LOGVAR_PLAYER),
	LOGVAR_DEFINE(quad, LOGVAR_PLAYER),
	LOGVAR_DEFINE(pent, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ring, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sg, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ssg, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ng, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sng, LOGVAR_PLAYER),
	LOGVAR_DEFINE(gl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(rl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(lg, LOGVAR_PLAYER),
	LOGVAR_DEFINE(pl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(avgpl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(maxpl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(minpl, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ping, LOGVAR_PLAYER),
	LOGVAR_DEFINE(avgping, LOGVAR_PLAYER),
	LOGVAR_DEFINE(maxping, LOGVAR_PLAYER),
	LOGVAR_DEFINE(minping, LOGVAR_PLAYER),
	LOGVAR_DEFINE(gacount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(yacount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(racount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ssgcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ngcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sngcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(glcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(rlcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(lgcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ssgdrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ngdrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sngdrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(gldrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(rldrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(lgdrop, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sgshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ssgshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ngshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(sngshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(glshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(rlshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(lgshots, LOGVAR_PLAYER),
	LOGVAR_DEFINE(deaths, LOGVAR_PLAYER),
	LOGVAR_DEFINE(mhcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(quadcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(ringcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(pentcount, LOGVAR_PLAYER),
	LOGVAR_DEFINE(avgspeed, LOGVAR_PLAYER),
	LOGVAR_DEFINE(maxspeed, LOGVAR_PLAYER),
	LOGVAR_DEFINE(posx, LOGVAR_PLAYER),
	LOGVAR_DEFINE(posy, LOGVAR_PLAYER),
	LOGVAR_DEFINE(posz, LOGVAR_PLAYER),
	LOGVAR_DEFINE(pitch, LOGVAR_PLAYER),
	LOGVAR_DEFINE(yaw, LOGVAR_PLAYER),
	LOGVAR_DEFINE(distancemoved, LOGVAR_PLAYER),
	LOGVAR_DEFINE(topcolor, LOGVAR_PLAYER),
	LOGVAR_DEFINE(bottomcolor, LOGVAR_PLAYER),
	LOGVAR_DEFINE(droppedweapon, LOGVAR_PLAYER),
	LOGVAR_DEFINE(droppedweaponstr, LOGVAR_PLAYER)
};

#define LOGVARS_LIST_SIZE		(sizeof(logvar_list) / sizeof(logvar_t))
#define LOGVARS_HASHTABLE_SIZE	512

static logvar_t *logvar_hashtable[LOGVARS_HASHTABLE_SIZE];

logvar_t *LogVarHashTable_GetValue(const logvar_t **hashtable, const char *varname)
{
	unsigned long varhash = Com_HashKey(varname); 
	logvar_t *it = (logvar_t *)hashtable[varhash % LOGVARS_HASHTABLE_SIZE];

	while (it && (it->hash != varhash))
	{
		it = it->next;
	}

	return it;
}

void LogVarHashTable_AddValue(logvar_t **hashtable, logvar_t *logvar)
{
	logvar_t *it = NULL;
	unsigned long varhash;

	logvar->hash = Com_HashKey(logvar->name);
	varhash = logvar->hash % LOGVARS_HASHTABLE_SIZE;
	it = hashtable[varhash];

	if (!it)
	{
		hashtable[varhash] = logvar;
	}
	else
	{
		while (it->next)
		{
			it = it->next;
		}

		it->next = logvar;
	}
}

char *LogVarValueAsString(mvd_info_t *mvd, const char *varname, int player_num)
{
	logvar_t *logvar = LogVarHashTable_GetValue(logvar_hashtable, varname);

	if (!logvar)
	{
		return "";
	}

	#ifdef _DEBUG
	if (strcasecmp(varname, logvar->name))
	{
		Sys_Error("LogVarValueAsString: %s != %s\n", varname, logvar->name);
	}
	#endif // _DEBUG

	if (!logvar->func)
	{
		Sys_Error("LogVarValueAsString: The log variable function for %s is NULL\n", varname);
	}

	// A player specific var requires a valid player num.
	if ((logvar->type == LOGVAR_PLAYER) && ((player_num < 0) || (player_num > 32)))
	{
		return "";
	}

	return logvar->func(mvd, logvar->name, player_num);
}

void LogVarHashTable_Init(void)
{
	int i;

	for (i = 0; i < LOGVARS_LIST_SIZE; i++)
	{
		LogVarHashTable_AddValue(logvar_hashtable, &logvar_list[i]);
	}
}

void LogVarHashTable_Test(mvd_info_t *mvd)
{
#ifdef _DEBUG
	int i;
	int pnum;
	unsigned long hash;
	logvar_t *lv;

	for (i = 0; i < LOGVARS_LIST_SIZE; i++)
	{
		lv = LogVarHashTable_GetValue(logvar_hashtable, logvar_list[i].name);
		hash = Com_HashKey(logvar_list[i].name);

		for (pnum = 0; pnum < MAX_PLAYERS; pnum++)
		{
			if (PLAYER_ISVALID(&mvd->players[pnum]))
			{
				Sys_Print("%s = %s\n", lv->name, lv->func(mvd, lv->name, pnum));
			}
		}

		if (hash != lv->hash)
		{
			Sys_PrintError("%s (%i) != %s (%i)\n", logvar_list[i].name, hash, lv->name, lv->hash);
		}
	}
#endif // _DEBUG
}

