/* conf.h.  Generated from conf.h.in by configure.  */
/*
 * Copyright (c) 2000-2012, Paul Mattes.
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
 *	conf.h
 *              System-specific #defines for libraries and library functions.
 *		Automatically generated from conf.h.in by configure.
 */


/* Libraries. */
/* #undef HAVE_LIBNCURSESW */
#define HAVE_LIBNCURSES 1
/* #undef HAVE_LIBCURSES */
/* #undef HAVE_LIBREADLINE */
#define HAVE_LIBSSL 1

/* Header files. */
/* #undef HAVE_NCURSESW_NCURSES_H */
/* #undef HAVE_NCURSES_NCURSES_H */
#define HAVE_NCURSES_H 1
/* #undef HAVE_CURSES_H */
/* #undef HAVE_NCURSESW_TERM_H */
/* #undef HAVE_NCURSES_TERM_H */
#define HAVE_TERM_H 1
#define HAVE_SYS_SELECT_H 1
/* #undef HAVE_READLINE_HISTORY_H */
/* #define HAVE_PTY_H 1 hharte */
/* #undef HAVE_LIBUTIL_H */
/* #undef HAVE_UTIL_H */
#define HAVE_GETOPT_H 1

/* Uncommon functions. */
#define HAVE_VASPRINTF 1
#define HAVE_FSEEKO 1
// hharte #define HAVE_FORKPTY 1
#define HAVE_USE_DEFAULT_COLORS 1

/* Default pager. */
#define LESSPATH "/usr/bin/less"
#define MOREPATH "/bin/more"

/* Wide curses. */
//#define CURSES_WIDE /* hharte - added this. */

/* Configuration options. */
#define USE_ICONV /* hharte uncommented this. */

/* Broken stuff. */
/* #undef BROKEN_NEWTERM */

/* Optional parts. */
#define X3270_ANSI 1
#define X3270_APL 1
#define X3270_DBCS 1
#define X3270_FT 1
#define X3270_LOCAL_PROCESS 1
#define X3270_PRINTER 1
#define X3270_SCRIPT 1
#define X3270_TN3270E 1
#define X3270_TRACE 1
/* #undef X3270_PLUGIN */
