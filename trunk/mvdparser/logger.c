#include <stdio.h>
#include <string.h>
#include "maindef.h"
#include "qw_protocol.h"
#include "logger.h"

#define LOG_CHECK_NUMARGS(num)																						\
{																													\
	if (arg_count != num)																							\
	{																												\
		Sys_PrintError("Output templates parsing: %i tokens expected %i found (line %i)\n", num, arg_count, line);	\
		line_start = line_end + 1;																					\
		continue;																									\
	}																												\
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
	#define LOG_NEXTLINE() { if (!(*line_end)) break; line_start = line_end + 1; continue; } 

	int eventlogger_count	= 0;
	int file_template_count	= 0;
	char *text				= NULL;
	char *line_start		= NULL; // Start of the current line.
	char *line_end			= NULL; // End of the current line.
	long filelen			= 0;
	int line				= 0;
	int arg_count			= 0;

	Log_ClearLogger(logger);

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
				int event_end_len = strlen(EVENT_END_STR);
				line_start = line_end + 1;

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

static char *Log_ExpandTemplateString(logger_t *logger, mvd_info_t *mvd, const char *template_string, int player_num)
{
	// TODO : Expand the string by going through it and resolving any variables in it.
}

void Log_Event(logger_t *logger, log_eventlogger_type_t type, int player_num)
{
	// TODO : Find all event loggers that has the type specified.
		// TODO : Expand the event loggers template string into an output string.
		// TODO : Loop through and expand the filename template strings.
			// TODO : Search for the expanded filename in the output_hashtable
				// TODO : If found, write the output string to the file
				// TODO : else open the file first
}

// ============================================================================
//								Log variables
// ============================================================================

static char *LogVar_name(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].name;
}

static char *LogVar_team(mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].team;
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

static char *LogVar_userid(mvd_info_t *mvd, const char *varname, int player_num)
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
	return va("%g", mvd->players[player_num].acc_average_speed / mvd->players[player_num].speed_frame_count);
}

static char *LogVar_maxspeed(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].speed_highest);
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
	return va("%g", mvd->players[player_num].distance_moved);
}

static char *LogVar_topcolor(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].topcolor);
}

static char *LogVar_bottomcolor(mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].bottomcolor);
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

	// Player specific variables.
	LOGVAR_DEFINE(name, LOGVAR_PLAYER),
	LOGVAR_DEFINE(team, LOGVAR_PLAYER),
	LOGVAR_DEFINE(userinfo, LOGVAR_PLAYER),
	LOGVAR_DEFINE(playernum, LOGVAR_PLAYER),
	LOGVAR_DEFINE(animframe, LOGVAR_PLAYER),
	LOGVAR_DEFINE(userid, LOGVAR_PLAYER),
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
	LOGVAR_DEFINE(bottomcolor, LOGVAR_PLAYER)
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

const char *LogVarValueAsString(mvd_info_t *mvd, const char *varname, int player_num)
{
	logvar_t *logvar = LogVarHashTable_GetValue(logvar_hashtable, varname);

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
}

