/*
 * Copyright (c) 1993-2012, Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	trace_ds.c
 *		3270 data stream tracing.
 *
 */

#include "globals.h"

#if defined(X3270_TRACE) /*[*/

#if defined(X3270_DISPLAY) /*[*/
#include <X11/StringDefs.h>
#include <X11/Xaw/Dialog.h>
#endif /*]*/
#if defined(_WIN32) /*[*/
#include <windows.h>
#endif /*]*/
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <fcntl.h>
#include "3270ds.h"
#include "appres.h"
#include "objects.h"
#include "resources.h"
#include "ctlr.h"

#include "ansic.h"
#include "charsetc.h"
#include "childc.h"
#include "ctlrc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "printc.h"
#include "savec.h"
#include "tablesc.h"
#include "telnetc.h"
#include "trace_dsc.h"
#include "utf8c.h"
#include "utilc.h"
#include "w3miscc.h"

#if defined(_MSC_VER) /*[*/
#include "Msc/deprecated.h"
#endif /*]*/

/* Maximum size of a tracefile header. */
#define MAX_HEADER_SIZE		(32*1024)

/* Minimum size of a trace file. */
#define MIN_TRACEFILE_SIZE	(64*1024)
#define MIN_TRACEFILE_SIZE_NAME	"64K"

/* System calls which may not be there. */
#if !defined(HAVE_FSEEKO) /*[*/
#define fseeko(s, o, w)	fseek(s, (long)o, w)
#define ftello(s)	(off_t)ftell(s)
#endif /*]*/

/* Statics */
static int      dscnt = 0;
#if !defined(_WIN32) /*[*/
static int      tracewindow_pid = -1;
#else /*][*/
static HANDLE	tracewindow_handle = NULL;
#endif /*]*/
static FILE    *tracef = NULL;
static FILE    *tracef_pipe = NULL;
static char    *tracef_bufptr = CN;
static off_t	tracef_size = 0;
static off_t	tracef_max = 0;
static char    *onetime_tracefile_name = CN;
static char    *onetime_screentracefile_name = CN;
static void	vwtrace(const char *fmt, va_list args);
static void	wtrace(const char *fmt, ...);
static char    *create_tracefile_header(const char *mode);
static void	stop_tracing(void);

/* Globals */
struct timeval   ds_ts;
Boolean          trace_skipping = False;
char		*tracefile_name = NULL;
char		*screentracefile_name = NULL;
Boolean  	 do_ts = True;

/* display a (row,col) */
const char *
rcba(int baddr)
{
	static char buf[16];

	(void) sprintf(buf, "(%d,%d)", baddr/COLS + 1, baddr%COLS + 1);
	return buf;
}

/* Data Stream trace print, handles line wraps */

static char *tdsbuf = CN;
#define TDS_LEN	75

/*
 * This function is careful to do line breaks based on wchar_t's, not
 * bytes, so multi-byte characters are traced properly.
 * However, it doesn't know that DBCS characters are two columns wide, so it
 * will get those wrong and break too late.  To get that right, it needs some
 * sort of function to tell it that a wchar_t is double-width, which we lack at
 * the moment.
 *
 * If wchar_t's are Unicode, it could perhaps use some sort of heuristic based
 * on which plane the character is in.
 */
static void
trace_ds_s(char *s, Boolean can_break)
{
    	int len = strlen(s);
	int len0 = len + 1;
	int wlen;
	Boolean nl = False;
	wchar_t *w_buf;		/* wchar_t translation of s */
	wchar_t *w_cur;		/* current wchar_t pointer */
	wchar_t *w_chunk;	/* transient wchar_t buffer */
	char *mb_chunk;		/* transient multibyte buffer */

	if (!toggled(DS_TRACE) || tracef == NULL || !len)
		return;

	/* Allocate buffers for chunks of output data. */
	mb_chunk = Malloc(len0);
	w_chunk = (wchar_t *)Malloc(len0 * sizeof(wchar_t));

	/* Convert the input string to wchar_t's. */
	w_buf = (wchar_t *)Malloc(len0 * sizeof(wchar_t));
	wlen = mbstowcs(w_buf, s, len);
	if (wlen < 0)
	    Error("trace_ds_s: mbstowcs failed");
	w_cur = w_buf;

	/* Check for a trailing newline. */
	if (len && s[len-1] == '\n') {
		wlen--;
		nl = True;
	}

	if (!can_break && dscnt + wlen >= 75) {
		wtrace("...\n... ");
		dscnt = 0;
	}

	while (dscnt + wlen >= 75) {
		int plen = 75-dscnt;
		int mblen;

		if (plen) {
		    memcpy(w_chunk, w_cur, plen * sizeof(wchar_t));
		    w_chunk[plen] = 0;
		    mblen = wcstombs(mb_chunk, w_chunk, len0);
		    if (mblen <= 0)
			Error("trace_ds_s: wcstombs 1 failed");
		} else {
		    mb_chunk[0] = '\0';
		    mblen = 0;
		}

		wtrace("%.*s ...\n... ", mblen, mb_chunk);
		dscnt = 4;
		w_cur += plen;
		wlen -= plen;
	}
	if (wlen) {
		int mblen;

		memcpy(w_chunk, w_cur, wlen * sizeof(wchar_t));
		w_chunk[wlen] = 0;
		mblen = wcstombs(mb_chunk, w_chunk, len0);
		if (mblen <= 0)
		    Error("trace_ds_s: wcstombs 2 failed");
		wtrace("%.*s", mblen, mb_chunk);
		dscnt += wlen;
	}
	if (nl) {
		wtrace("\n");
		dscnt = 0;
	}
	Free(mb_chunk);
	Free(w_buf);
	Free(w_chunk);
}

void
trace_ds(const char *fmt, ...)
{
	va_list args;

	if (!toggled(DS_TRACE) || tracef == NULL)
		return;

	va_start(args, fmt);

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	do_ts = False;
	(void) vsprintf(tdsbuf, fmt, args);
	trace_ds_s(tdsbuf, True);
	va_end(args);
}

void
trace_ds_nb(const char *fmt, ...)
{
	va_list args;

	if (!toggled(DS_TRACE) || tracef == NULL)
		return;

	va_start(args, fmt);

	/* allocate buffer */
	if (tdsbuf == CN)
		tdsbuf = Malloc(4096);

	/* print out remainder of message */
	(void) vsprintf(tdsbuf, fmt, args);
	trace_ds_s(tdsbuf, False);
	va_end(args);
}

/* Conditional event trace. */
void
trace_event(const char *fmt, ...)
{
	va_list args;

	if (!toggled(EVENT_TRACE) || tracef == NULL)
		return;

	/* print out message */
	va_start(args, fmt);
	vwtrace(fmt, args);
	va_end(args);
}

/* Conditional data stream trace, without line splitting. */
void
trace_dsn(const char *fmt, ...)
{
	va_list args;

	if (!toggled(DS_TRACE) || tracef == NULL)
		return;

	/* print out message */
	va_start(args, fmt);
	vwtrace(fmt, args);
	va_end(args);
}

/*
 * Write to the trace file, varargs style.
 * This is the only function that actually does output to the trace file --
 * all others are wrappers around this function.
 */
static void
vwtrace(const char *fmt, va_list args)
{
	if (tracef_bufptr != CN) {
		tracef_bufptr += vsprintf(tracef_bufptr, fmt, args);
	} else if (tracef != NULL) {
		int n2w, nw;
		char buf[16384];
		struct timeval tv;
		time_t t;
		struct tm *tm;

		/* Start with a timestamp. */
		if (do_ts) {
			(void) gettimeofday(&tv, NULL);
			t = tv.tv_sec;
			tm = localtime(&t);
			n2w = sprintf(buf, "%d%02d%02d.%02d%02d%02d.%03d ",
				tm->tm_year + 1900,
				tm->tm_mon + 1,
				tm->tm_mday,
				tm->tm_hour,
				tm->tm_min,
				tm->tm_sec,
				(int)(tv.tv_usec / 1000L));
			(void) fwrite(buf, n2w, 1, tracef);
			fflush(tracef);
			if (tracef_pipe != NULL) {
				(void) fwrite(buf, n2w, 1, tracef_pipe);
				fflush(tracef);
			}
			do_ts = False;
		}

		buf[0] = 0;
		(void) vsnprintf(buf, sizeof(buf), fmt, args);
		buf[sizeof(buf) - 1] = '\0';
		n2w = strlen(buf);
		if (n2w > 0 && buf[n2w - 1] == '\n')
		    	do_ts = True;

		nw = fwrite(buf, n2w, 1, tracef);
		if (nw == 1) {
			fflush(tracef);
		} else {
			if (errno != EPIPE
#if defined(EILSEQ) /*[*/
					   && errno != EILSEQ
#endif /*]*/
					                     )
				popup_an_errno(errno,
				    "Write to trace file failed");
#if defined(EILSEQ) /*[*/
			if (errno != EILSEQ)
#endif /*]*/
			{
				stop_tracing();
				return;
			}
		}
		tracef_size = ftello(tracef);
		if (tracef_pipe != NULL) {
			nw = fwrite(buf, n2w, 1, tracef_pipe);
			if (nw != 1) {
				(void) fclose(tracef_pipe);
				tracef_pipe = NULL;
			} else {
			    	fflush(tracef_pipe);
			}
		}
	}
}

/* Write to the trace file. */
static void
wtrace(const char *fmt, ...)
{
	if (tracef != NULL) {
		va_list args;

		va_start(args, fmt);
		vwtrace(fmt, args);
		va_end(args);
	}
}

static void
stop_tracing(void)
{
	if (tracef != NULL && tracef != stdout)
		(void) fclose(tracef);
	tracef = NULL;
	if (tracef_pipe != NULL) {
		(void) fclose(tracef_pipe);
		tracef_pipe = NULL;
	}
	if (toggled(DS_TRACE)) {
		toggle_toggle(&appres.toggle[DS_TRACE]);
		menubar_retoggle(&appres.toggle[DS_TRACE], DS_TRACE);
	}
	if (toggled(EVENT_TRACE)) {
		toggle_toggle(&appres.toggle[EVENT_TRACE]);
		menubar_retoggle(&appres.toggle[EVENT_TRACE],  EVENT_TRACE);
	}
}

/* Check for a trace file rollover event. */
void
trace_rollover_check(void)
{
	if (tracef == NULL || tracef_max == 0)
		return;

	/* See if we've reached a rollover point. */
	if (tracef_size >= tracef_max) {
	    	char *alt_filename;
		char *new_header;
#if defined(_WIN32) /*[*/
		char *dot;
#endif /*]*/

		/* Close up this file. */
		wtrace("Trace rolled over\n");
		fclose(tracef);
		tracef = NULL;

		/* Unlink and rename the alternate file. */
#if defined(_WIN32) /*[*/
		dot = strrchr(tracefile_name, '.');
		if (dot != CN)
			alt_filename = xs_buffer("%.*s-%s",
				dot - tracefile_name,
				tracefile_name,
				dot);
		else
#endif /*]*/
			alt_filename = xs_buffer("%s-", tracefile_name);
		(void) unlink(alt_filename);
		(void) rename(tracefile_name, alt_filename);
		Free(alt_filename);
		alt_filename = CN;
		tracef = fopen(tracefile_name, "w");
		if (tracef == (FILE *)NULL) {
			popup_an_errno(errno, "%s", tracefile_name);
			return;
		}

		/* Initialize it. */
		tracef_size = 0L;
		(void) SETLINEBUF(tracef);
		new_header = create_tracefile_header("rolled over");
		do_ts = True;
		wtrace(new_header);
		Free(new_header);
	}
}

#if defined(X3270_DISPLAY) /*[*/
static Widget trace_shell = (Widget)NULL;
#endif /*]*/
static int trace_reason;

/* Create a trace file header. */
static char *
create_tracefile_header(const char *mode)
{
	char *buf;

	/* Create a buffer and redirect output. */
	buf = Malloc(MAX_HEADER_SIZE);
	tracef_bufptr = buf;

	/* Display current status */
	wtrace("Trace %s\n", mode);
	wtrace(" Version: %s\n", build);
	wtrace(" %s\n", build_options());
	save_yourself();
	wtrace(" Command: %s\n", command_string);
	wtrace(" Model %s, %d rows x %d cols", model_name, maxROWS, maxCOLS);
#if defined(X3270_DISPLAY) || (defined(C3270) && !defined(_WIN32)) /*[*/
	wtrace(", %s display", appres.mono ? "monochrome" : "color");
#endif /*]*/
	if (appres.extended)
		wtrace(", extended data stream");
	wtrace(", %s emulation", appres.m3279 ? "color" : "monochrome");
	wtrace(", %s charset", get_charset_name());
	if (appres.apl_mode)
		wtrace(", APL mode");
	wtrace("\n");
#if !defined(_WIN32) /*[*/
	wtrace(" Locale codeset: %s\n", locale_codeset);
#else /*][*/
	wtrace(" ANSI codepage: %d\n", GetACP());
# if defined(WS3270) /*[*/
	wtrace(" Local codepage: %d\n", appres.local_cp);
# endif /*]*/
#endif /*]*/
	wtrace(" Host codepage: %d", (int)(cgcsgid & 0xffff));
#if defined(X3270_DBCS) /*[*/
	if (dbcs)
		wtrace("+%d", (int)(cgcsgid_dbcs & 0xffff));
#endif /*]*/
	wtrace("\n");
	if (CONNECTED)
		wtrace(" Connected to %s, port %u\n",
		    current_host, current_port);

	/* Snap the current TELNET options. */
	if (net_snap_options()) {
		wtrace(" TELNET state:\n");
		trace_netdata('<', obuf, obptr - obuf);
	}

	/* Dump the screen contents and modes into the trace file. */
	if (CONNECTED) {
		/*
		 * Note that if the screen is not formatted, we do not
		 * attempt to save what's on it.  However, if we're in
		 * 3270 SSCP-LU or NVT mode, we'll do a dummy, empty
		 * write to ensure that the display is in the right
		 * mode.
		 */
		if (formatted) {
			wtrace(" Screen contents (3270):\n");
			obptr = obuf;
#if defined(X3270_TN3270E) /*[*/
			(void) net_add_dummy_tn3270e();
#endif /*]*/
			ctlr_snap_buffer();
			space3270out(2);
			net_add_eor(obuf, obptr - obuf);
			obptr += 2;
			trace_netdata('<', obuf, obptr - obuf);

			obptr = obuf;
#if defined(X3270_TN3270E) /*[*/
			(void) net_add_dummy_tn3270e();
#endif /*]*/
			if (ctlr_snap_modes()) {
				wtrace(" 3270 modes:\n");
				space3270out(2);
				net_add_eor(obuf, obptr - obuf);
				obptr += 2;
				trace_netdata('<', obuf, obptr - obuf);
			}
		}
#if defined(X3270_TN3270E) /*[*/
		else if (IN_E) {
			obptr = obuf;
			(void) net_add_dummy_tn3270e();
			wtrace(" Screen contents (%s):\n",
				IN_SSCP? "SSCP-LU": "TN3270E-NVT");
			if (IN_SSCP) 
			    	ctlr_snap_buffer_sscp_lu();
			else if (IN_ANSI)
			    	ansi_snap();
			space3270out(2);
			net_add_eor(obuf, obptr - obuf);
			obptr += 2;
			trace_netdata('<', obuf, obptr - obuf);
			if (IN_ANSI) {
				wtrace(" NVT modes:\n");
				obptr = obuf;
				ansi_snap_modes();
				trace_netdata('<', obuf, obptr - obuf);
			}
		}
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
		else if (IN_ANSI) {
			obptr = obuf;
			wtrace(" Screen contents (NVT):\n");
			ansi_snap();
			trace_netdata('<', obuf, obptr - obuf);
			wtrace(" NVT modes:\n");
			obptr = obuf;
			ansi_snap_modes();
			trace_netdata('<', obuf, obptr - obuf);
		}
#endif /*]*/
	}

	wtrace(" Data stream:\n");

	/* Return the buffer. */
	tracef_bufptr = CN;
	return buf;
}

/* Calculate the tracefile maximum size. */
static void
get_tracef_max(void)
{
	static Boolean calculated = False;
	char *ptr;
	Boolean bad = False;

	if (calculated)
		return;

	calculated = True;

	if (appres.trace_file_size == CN ||
	    !strcmp(appres.trace_file_size, "0") ||
	    !strncasecmp(appres.trace_file_size, "none",
			 strlen(appres.trace_file_size))) {
		tracef_max = 0;
		return;
	}

	tracef_max = strtoul(appres.trace_file_size, &ptr, 0);
	if (tracef_max == 0 || ptr == appres.trace_file_size || *(ptr + 1)) {
		bad = True;
	} else switch (*ptr) {
	case 'k':
	case 'K':
		tracef_max *= 1024;
		break;
	case 'm':
	case 'M':
		tracef_max *= 1024 * 1024;
		break;
	case '\0':
		break;
	default:
		bad = True;
		break;
	}

	if (bad) {
		tracef_max = MIN_TRACEFILE_SIZE;
#if defined(X3270_DISPLAY) /*[*/
		popup_an_info("Invalid %s '%s', assuming "
		    MIN_TRACEFILE_SIZE_NAME,
		    ResTraceFileSize,
		    appres.trace_file_size);
#endif /*]*/
	} else if (tracef_max < MIN_TRACEFILE_SIZE) {
		tracef_max = MIN_TRACEFILE_SIZE;
	}
}

/* Parse the name '/dev/fd<n>', so we can simulate it. */
static int
get_devfd(const char *pathname)
{
	unsigned long fd;
	char *ptr;

	if (strncmp(pathname, "/dev/fd/", 8))
		return -1;
	fd = strtoul(pathname + 8, &ptr, 10);
	if (ptr == pathname + 8 || *ptr != '\0' || fd < 0)
		return -1;
	return fd;
}

/* Callback for "OK" button on trace popup */
static void
tracefile_callback(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
	char *tfn = CN;
	int devfd = -1;
#if defined(X3270_DISPLAY) /*[*/
	int pipefd[2];
	Boolean just_piped = False;
#endif /*]*/
	char *buf;

#if defined(X3270_DISPLAY) /*[*/
	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
#endif /*]*/
		tfn = (char *)client_data;
	tfn = do_subst(tfn, True, True);
	if (strchr(tfn, '\'') ||
	    ((int)strlen(tfn) > 0 && tfn[strlen(tfn)-1] == '\\')) {
		popup_an_error("Illegal file name: %s", tfn);
		Free(tfn);
		return;
	}

	tracef_max = 0;

	if (!strcmp(tfn, "stdout")) {
		tracef = stdout;
	} else {
#if defined(X3270_DISPLAY) /*[*/
		FILE *pipefile = NULL;

		if (!strcmp(tfn, "none") || !tfn[0]) {
			just_piped = True;
			if (!appres.trace_monitor) {
				popup_an_error("Must specify a trace file "
				    "name");
				free(tfn);
				return;
			}
		}

		if (appres.trace_monitor) {
			if (pipe(pipefd) < 0) {
				popup_an_errno(errno, "pipe() failed");
				Free(tfn);
				return;
			}
			pipefile = fdopen(pipefd[1], "w");
			if (pipefile == NULL) {
				popup_an_errno(errno, "fdopen() failed");
				(void) close(pipefd[0]);
				(void) close(pipefd[1]);
				Free(tfn);
				return;
			}
			(void) SETLINEBUF(pipefile);
			(void) fcntl(pipefd[1], F_SETFD, 1);
		}

		if (just_piped) {
			tracef = pipefile;
		} else
#endif /*]*/
		{
		    	Boolean append = False;

#if defined(X3270_DISPLAY) /*[*/
			tracef_pipe = pipefile;
#endif /*]*/
			/* Get the trace file maximum. */
			get_tracef_max();

			/* Open and configure the file. */
			if ((devfd = get_devfd(tfn)) >= 0)
				tracef = fdopen(dup(devfd), "a");
			else if (!strncmp(tfn, ">>", 2)) {
			    	append = True;
				tracef = fopen(tfn + 2, "a");
			} else
				tracef = fopen(tfn, "w");
			if (tracef == (FILE *)NULL) {
				popup_an_errno(errno, "%s", tfn);
#if defined(X3270_DISPLAY) /*[*/
				fclose(tracef_pipe);
				(void) close(pipefd[0]);
				(void) close(pipefd[1]);
#endif /*]*/
				Free(tfn);
				return;
			}
			tracef_size = ftello(tracef);
			Replace(tracefile_name,
				NewString(append? tfn + 2: tfn));
			(void) SETLINEBUF(tracef);
#if !defined(_WIN32) /*[*/
			(void) fcntl(fileno(tracef), F_SETFD, 1);
#endif /*]*/
		}
	}

#if defined(X3270_DISPLAY) /*[*/
	/* Start the monitor window */
	if (tracef != stdout && appres.trace_monitor) {
		switch (tracewindow_pid = fork_child()) {
		    case 0:	/* child process */
			{
				char cmd[64];

				(void) sprintf(cmd, "cat <&%d", pipefd[0]);
				(void) execlp("xterm", "xterm",
				    "-title", just_piped? "trace": tfn,
				    "-sb", "-e", "/bin/sh", "-c",
				    cmd, CN);
			}
			(void) perror("exec(xterm) failed");
			_exit(1);
		    default:	/* parent */
			(void) close(pipefd[0]);
			++children;
			break;
		    case -1:	/* error */
			popup_an_errno(errno, "fork() failed");
			break;
		}
	}
#endif /*]*/

#if defined(_WIN32) && defined(C3270) /*[*/
	/* Start the monitor window. */
	if (tracef != stdout && appres.trace_monitor && is_installed) {
		STARTUPINFO startupinfo;
		PROCESS_INFORMATION process_information;
		char *path;
		char *args;

	    	(void) memset(&startupinfo, '\0', sizeof(STARTUPINFO));
		startupinfo.cb = sizeof(STARTUPINFO);
		startupinfo.lpTitle = tfn;
		(void) memset(&process_information, '\0',
			      sizeof(PROCESS_INFORMATION));
		path = xs_buffer("%scatf.exe", instdir);
		args = xs_buffer("\"%scatf.exe\" \"%s\"", instdir, tfn);
		if (CreateProcess(
		    path,
		    args,
		    NULL,
		    NULL,
		    FALSE,
		    CREATE_NEW_CONSOLE,
		    NULL,
		    NULL,
		    &startupinfo,
		    &process_information) == 0) {
		    	popup_an_error("CreateProcess(%s) failed: %s",
				path, win32_strerror(GetLastError()));
			Free(path);
			Free(args);
		} else {
			Free(path);
		    	Free(args);
			tracewindow_handle = process_information.hProcess;
			CloseHandle(process_information.hThread);
		}
	}
#endif /*]*/

	Free(tfn);

	/* We're really tracing, turn the flag on. */
	appres.toggle[trace_reason].value = True;
	appres.toggle[trace_reason].changed = True;
	menubar_retoggle(&appres.toggle[trace_reason], trace_reason);

	/* Display current status. */
	buf = create_tracefile_header("started");
	do_ts = True;
	wtrace("%s", buf);
	Free(buf);

#if defined(X3270_DISPLAY) /*[*/
	if (w)
		XtPopdown(trace_shell);
#endif /*]*/
}

#if defined(X3270_DISPLAY) /*[*/
/* Callback for "No File" button on trace popup */
static void
no_tracefile_callback(Widget w, XtPointer client_data,
	XtPointer call_data _is_unused)
{
	tracefile_callback((Widget)NULL, "", PN);
	XtPopdown(trace_shell);
}
#endif /*]*/

/* Open the trace file. */
static void
tracefile_on(int reason, enum toggle_type tt)
{
	char *tracefile_buf = NULL;
	char *tracefile;

	if (tracef != (FILE *)NULL)
		return;

	trace_reason = reason;
	if (appres.secure && tt != TT_INITIAL) {
		tracefile_callback((Widget)NULL, "none", PN);
		return;
	}
	if (onetime_tracefile_name != NULL) {
	    	tracefile = tracefile_buf = onetime_tracefile_name;
		onetime_tracefile_name = NULL;
	} else if (appres.trace_file)
		tracefile = appres.trace_file;
	else {
#if defined(_WIN32) /*[*/
		tracefile_buf = xs_buffer("%s%sx3trc.%u.txt",
			(appres.trace_dir != CN)? appres.trace_dir: myappdata,
			(appres.trace_dir != CN)? "\\": "",
			getpid());
#else /*][*/
		tracefile_buf = xs_buffer("%s/x3trc.%u", appres.trace_dir,
			getpid());
#endif /*]*/
		tracefile = tracefile_buf;
	}

#if defined(X3270_DISPLAY) /*[*/
	if (tt == TT_INITIAL || tt == TT_ACTION)
#endif /*]*/
	{
		tracefile_callback((Widget)NULL, tracefile, PN);
		if (tracefile_buf != NULL)
		    	Free(tracefile_buf);
		return;
	}
#if defined(X3270_DISPLAY) /*[*/
	if (trace_shell == NULL) {
		trace_shell = create_form_popup("trace",
		    tracefile_callback,
		    appres.trace_monitor? no_tracefile_callback: NULL,
		    FORM_NO_WHITE);
		XtVaSetValues(XtNameToWidget(trace_shell, ObjDialog),
		    XtNvalue, tracefile,
		    NULL);
	}

	/* Turn the toggle _off_ until the popup succeeds. */
	appres.toggle[reason].value = False;
	appres.toggle[reason].changed = True;

	popup_popup(trace_shell, XtGrabExclusive);
#endif /*]*/

	if (tracefile_buf != NULL)
		Free(tracefile_buf);
}

/* Close the trace file. */
static void
tracefile_off(void)
{
	wtrace("Trace stopped\n");
#if !defined(_WIN32) /*[*/
	if (tracewindow_pid != -1)
		(void) kill(tracewindow_pid, SIGKILL);
	tracewindow_pid = -1;
#else /*][*/
	if (tracewindow_handle != NULL) {
	    	TerminateProcess(tracewindow_handle, 0);
		CloseHandle(tracewindow_handle);
		tracewindow_handle = NULL;
	}

#endif /*]*/
	stop_tracing();
}

void
trace_set_trace_file(const char *path)
{
    	Replace(onetime_tracefile_name, NewString(path));
}

void
toggle_dsTrace(struct toggle *t _is_unused, enum toggle_type tt)
{
	/* If turning on trace and no trace file, open one. */
	if (toggled(DS_TRACE) && tracef == NULL)
		tracefile_on(DS_TRACE, tt);

	/* If turning off trace and not still tracing events, close the
	   trace file. */
	else if (!toggled(DS_TRACE) && !toggled(EVENT_TRACE))
		tracefile_off();

	if (toggled(DS_TRACE))
		(void) gettimeofday(&ds_ts, (struct timezone *)NULL);
}

void
toggle_eventTrace(struct toggle *t _is_unused, enum toggle_type tt)
{
	/* If turning on event debug, and no trace file, open one. */
	if (toggled(EVENT_TRACE) && tracef == NULL)
		tracefile_on(EVENT_TRACE, tt);

	/* If turning off event debug, and not tracing the data stream,
	   close the trace file. */
	else if (!toggled(EVENT_TRACE) && !toggled(DS_TRACE))
		tracefile_off();
}

/* Screen trace file support. */

#if defined(X3270_DISPLAY) /*[*/
static Widget screentrace_shell = (Widget)NULL;
#endif /*]*/
static FILE *screentracef = (FILE *)0;

/*
 * Screen trace function, called when the host clears the screen.
 */
static void
do_screentrace(void)
{
	register int i;

	if (fprint_screen(screentracef, P_TEXT, 0, NULL)) {
		for (i = 0; i < COLS; i++)
			(void) fputc('=', screentracef);
		(void) fputc('\n', screentracef);
	}
}

void
trace_screen(void)
{
	trace_skipping = False;

	if (!toggled(SCREEN_TRACE) || !screentracef)
		return;
	do_screentrace();
}

/* Called from ANSI emulation code to log a single character. */
void
trace_char(char c)
{
	if (!toggled(SCREEN_TRACE) || !screentracef)
		return;
	(void) fputc(c, screentracef);
}

/*
 * Called when disconnecting in ANSI mode, to finish off the trace file
 * and keep the next screen clear from re-recording the screen image.
 * (In a gross violation of data hiding and modularity, trace_skipping is
 * manipulated directly in ctlr_clear()).
 */
void
trace_ansi_disc(void)
{
	int i;

	(void) fputc('\n', screentracef);
	for (i = 0; i < COLS; i++)
		(void) fputc('=', screentracef);
	(void) fputc('\n', screentracef);

	trace_skipping = True;
}

/*
 * Screen tracing callback.
 * Returns True for success, False for failure.
 */
static Boolean
screentrace_cb(char *tfn)
{
	tfn = do_subst(tfn, True, True);
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(errno, "%s", tfn);
		Free(tfn);
		return False;
	}
	Replace(screentracefile_name, NewString(tfn));
	Free(tfn);
	(void) SETLINEBUF(screentracef);
#if !defined(_WIN32) /*[*/
	(void) fcntl(fileno(screentracef), F_SETFD, 1);
#endif /*]*/

	/* We're really tracing, turn the flag on. */
	appres.toggle[SCREEN_TRACE].value = True;
	appres.toggle[SCREEN_TRACE].changed = True;
	menubar_retoggle(&appres.toggle[SCREEN_TRACE], SCREEN_TRACE);
	return True;
}

#if defined(X3270_DISPLAY) /*[*/
/* Callback for "OK" button on screentrace popup */
static void
screentrace_callback(Widget w _is_unused, XtPointer client_data,
    XtPointer call_data _is_unused)
{
	if (screentrace_cb(XawDialogGetValueString((Widget)client_data)))
		XtPopdown(screentrace_shell);
}

/* Callback for second "OK" button on screentrace popup */
static void
onescreen_callback(Widget w, XtPointer client_data, XtPointer call_data _is_unused)
{
	char *tfn;

	if (w)
		tfn = XawDialogGetValueString((Widget)client_data);
	else
		tfn = (char *)client_data;
	tfn = do_subst(tfn, True, True);
	screentracef = fopen(tfn, "a");
	if (screentracef == (FILE *)NULL) {
		popup_an_errno(errno, "%s", tfn);
		XtFree(tfn);
		return;
	}
	(void) fcntl(fileno(screentracef), F_SETFD, 1);
	XtFree(tfn);

	/* Save the current image, once. */
	do_screentrace();

	/* Close the file, we're done. */
	(void) fclose(screentracef);
	screentracef = (FILE *)NULL;

	if (w)
		XtPopdown(screentrace_shell);
}
#endif /*]*/

void
trace_set_screentrace_file(const char *path)
{
    	Replace(onetime_screentracefile_name, NewString(path));
}

void
toggle_screenTrace(struct toggle *t _is_unused, enum toggle_type tt)
{
	char *tracefile_buf = NULL;
	char *tracefile;

	if (toggled(SCREEN_TRACE)) {
	    	if (onetime_screentracefile_name != NULL) {
		    	tracefile = tracefile_buf =
			    onetime_screentracefile_name;
			onetime_screentracefile_name = NULL;
		} else if (appres.screentrace_file)
			tracefile = appres.screentrace_file;
		else {
#if defined(_WIN32) /*[*/
			tracefile_buf = xs_buffer("%s%sx3scr.%u.txt",
				(appres.trace_dir != CN)?
				    appres.trace_dir: myappdata,
				(appres.trace_dir != CN)? "\\": "",
				getpid());
#else /*][*/
			tracefile_buf = xs_buffer("%s/x3scr.%u",
				appres.trace_dir, getpid());
#endif /*]*/
			tracefile = tracefile_buf;
		}
		if (tt == TT_INITIAL || tt == TT_ACTION || tt == TT_INTERACTIVE) {
			(void) screentrace_cb(NewString(tracefile));
			if (tracefile_buf != NULL)
				Free(tracefile_buf);
			return;
		}
#if defined(X3270_DISPLAY) /*[*/
		if (screentrace_shell == NULL) {
			screentrace_shell = create_form_popup("screentrace",
			    screentrace_callback, onescreen_callback,
			    FORM_NO_WHITE);
			XtVaSetValues(XtNameToWidget(screentrace_shell,
					ObjDialog),
			    XtNvalue, tracefile,
			    NULL);
		}
		appres.toggle[SCREEN_TRACE].value = False;
		appres.toggle[SCREEN_TRACE].changed = True;
		popup_popup(screentrace_shell, XtGrabExclusive);
#endif /*]*/
	} else {
		if (ctlr_any_data() && !trace_skipping)
			do_screentrace();
		(void) fclose(screentracef);
		screentracef = NULL;
	}

	if (tracefile_buf != NULL)
		Free(tracefile_buf);
}

#endif /*]*/
