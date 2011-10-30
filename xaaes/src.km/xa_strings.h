/*
 * Output-strings
 */

enum
{
 /* window-titles */
	RS_ABOUT,
	RS_TM,
	RS_SYS,
	RS_APPLST,
	/* system-window*/
	RS_ALERTS,
	RS_ENV,
	RS_SYSTEM,
	RS_VIDEO,
	RS_MEMPROT,
	RS_OFF,
	RS_ON,

	SW_KEYBD,

	UNKNOWN,

	/* alert-text */
	ASK_QUITALL_ALERT,
	ASK_QUIT_ALERT,
	ASK_RESTART_ALERT,
	ASK_SHUTDOWN_ALERT,
	HAD_A_PROBLEM,
	SNAP_ERR1,
	SNAP_ERR2,
	SNAP_ERR3,
	SDALERT,
	RS_RSCSZ,
	RS_LAUNCH,
	RS_LDGRAD,
	RS_LDIMG,
	RS_LDPAL,
	AL_SDMASTER,
	AL_KBD,
	AL_TERMAES,
	AL_KILLAES,
	AL_NOPB,
	AL_NOAESPR,
	AL_VALOBTREE,
	AL_ATTACH,
	AL_MEM,
	AL_PDLG,
	AL_STMD,
	AL_NOGLOBPTR,
	AL_INVALIDP,
	AL_SHELLRUNS,
	AL_NOSHELL,

	MNU_KEY_SPACE,
	MNU_KEY_DELETE,
	MNU_CLIENTS,

	XA_HELP_FILE
};

extern char *xa_strings[];
extern char *KeyNames[];
extern char *WidgNames[];
extern char **trans_strings[];
