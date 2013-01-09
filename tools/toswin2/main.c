
#include <mintbind.h>
#include <string.h>
#include <support.h>
#include <unistd.h>

#include "global.h"
#include "textwin.h"
#include "proc.h"
#include "av.h"
#include "version.h"
#include "event.h"
#include "console.h"
#include "environ.h"
#include "toswin2.h"

#ifdef DEBUG
# include <syslog.h>
int do_debug = 0;
#endif

/* from mintlib:mintlib/main.c */
extern int __mint;

#ifdef __GNUC__
long _stksize = 32768;
#endif


static void term_tw(int ret_code)
{
	config_term();
	textwin_term();
	window_term();
	av_term();
	global_term();
	exit_app(ret_code);
}

int main(int argc, char *argv[])
{
	short gi_wind, gi_msgs, d;
	char str[25];
	OBJECT *tmp;

	if (getenv("DEBUG") || (argc >=2 && strcmp(argv[1], "--debug") == 0))
	{
#ifdef DEBUG
		do_debug = 1;
#endif
		debug_init("ntw", Datei, "\\tmp\\toswin2.log");
		/* debug_init("ntw", Con, NULL);  */
	}

	(void)Pdomain(1);
	init_app(RSCNAME);

#ifdef DEBUG
	if (do_debug) {
		openlog ("TosWin2", LOG_CONS | LOG_PID, LOG_KERN);
		syslog (LOG_ERR, "TosWin2 started");
	}
#endif

	/* Stimmt die RSC-Version? */
	rsrc_gaddr(R_TREE, VERSION, &tmp);
	get_string(tmp, RSC_VERSION, str);
	if (strcmp(str, TWVERSION) != 0)
	{
		do_alert(1, 0, "[3][Falsche RSC-Version!|Wrong RSC version!][Exit]");
		exit_app(1);
	}


	global_init();
	
	if (!gl_mint || gl_gem <= 0x400)
	{
		if (alert(1, 0, NOAES41) == 2)
			term_tw(1);
	}
	
	appl_xgetinfo(11, &gi_wind, &d, &d, &d);
	appl_xgetinfo(12, &gi_msgs, &d, &d, &d);

	wco = 0;
	if (gi_wind & 0x0800)	/* WF_OPTS */
	{
		short wopts, new_wopts = 0;

		appl_xgetinfo(97, &wopts, &d, &d, &d);
		
		if (gi_msgs & 0x0400) /* WM_REPOSED */
			new_wopts |= WO0_SENDREPOS; //wind_set(-1, WF_OPTS, 1, WO0_SENDREPOS, 0, 0);
		if (wopts & 0x0020) /* WCOWORK */
		{
			new_wopts |= 0x0020;  /* WO0_WCOWORK */
			wco = 1;
		}
		wind_set(-1, WF_OPTS, 1, new_wopts, 0, 0);
	}
#ifdef ONLY_XAAES
	if (!wco)
	{
		do_alert(1, 0, "[3][This version of TosWin2 needs WCOWORK,|only provided by XaAES atm!][Exit]");
		return FALSE;
	}
#endif	
	
	if (gi_msgs & 0x0008) /* AP_TERM */
		shel_write(9, 1, 1, 0L, 0L);
	
	menu_register(-1, TW2NAME);	/* damit tw-call immer fndig wird. */
	if (gl_debug)
		menu_register(gl_apid, "  TosWin2 (debug)");
	else	
		menu_register(gl_apid, "  TosWin2");

	config_init();
	proc_init();
	av_init();

	config_load();
	event_init();

	if (gl_con_auto)
		open_console();
	
	if (gl_debug)
	{
		if (gl_magx)
		{
			WINCFG	*cfg;
			TEXTWIN	*t;
			
			cfg = get_wincfg("C:\\TMP\\DUM.TOS");
			t = new_proc("C:\\TMP\\DUM.TOS", "", NULL, "C:\\TMP", cfg, -1, NULL);
			if (t)
				open_window(t->win, FALSE);
		}	
		else
			new_shell();	
	}
	
	if (argc >= 2)
	{
		if (argv[1][0] == '-')		/* filter options */
		{
			if (strcmp(argv[1], "-l") == 0)
				new_shell();
		}
		else
		{
			TEXTWIN	*t;
			char filename[256] = "", *p, path[256] = "";
			char *env, arg[125] = "";
			int i;
			WINCFG	*cfg;
			
			unx2dos(argv[1], filename);
			strcpy(path, filename);
			p = strrchr(path, '\\');
			if (p == NULL)
				getcwd(path, (int)sizeof(path));
			else
				*p = '\0';
			cfg = get_wincfg(filename);
			for (i = 2; i < argc; i++)
			{
				strcat(arg, " ");
				strcat(arg, argv[i]);
			}
			arg[0] = strlen(arg);
			env = normal_env(cfg->col, cfg->row, cfg->vt_mode);
			t = new_proc(filename, arg, env, path, cfg, -1, NULL);
			if (t)
				open_window(t->win, cfg->iconified);
			free(env);
		}
	}

	event_loop();
	
	term_tw(0);

	return 0;
}
