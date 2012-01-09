/*****************************************************************************
 * pce                                                                       *
 *****************************************************************************/

/*****************************************************************************
 * File name:   src/lib/monitor.c                                            *
 * Created:     2006-12-13 by Hampa Hug <hampa@hampa.ch>                     *
 * Copyright:   (C) 2006-2012 Hampa Hug <hampa@hampa.ch>                     *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "monitor.h"
#include "cmd.h"
#include "console.h"


void mon_init (monitor_t *mon)
{
	mon->cmdext = NULL;
	mon->docmd = NULL;

	mon->msgext = NULL;
	mon->setmsg = NULL;

	mon->get_mem8_ext = NULL;
	mon->get_mem8 = NULL;

	mon->set_mem8_ext = NULL;
	mon->set_mem8 = NULL;

	mon->memory_mode = 0;

	mon->default_seg = 0;

	mon->last_addr = 0;
	mon->last_ofs = 0;

	mon->terminate = 0;
	mon->prompt = NULL;
}

void mon_free (monitor_t *mon)
{
}

monitor_t *mon_new (void)
{
	monitor_t *mon;

	mon = malloc (sizeof (monitor_t));
	if (mon == NULL) {
		return (NULL);
	}

	mon_init (mon);

	return (mon);
}

void mon_del (monitor_t *mon)
{
	if (mon != NULL) {
		mon_free (mon);
		free (mon);
	}
}

void mon_set_cmd_fct (monitor_t *mon, void *fct, void *ext)
{
	mon->cmdext = ext;
	mon->docmd = fct;
}

void mon_set_msg_fct (monitor_t *mon, void *fct, void *ext)
{
	mon->msgext = ext;
	mon->setmsg = fct;
}

void mon_set_get_mem_fct (monitor_t *mon, void *ext, void *fct)
{
	mon->get_mem8_ext = ext;
	mon->get_mem8 = fct;
}

void mon_set_set_mem_fct (monitor_t *mon, void *ext, void *fct)
{
	mon->set_mem8_ext = ext;
	mon->set_mem8 = fct;
}

void mon_set_memory_mode (monitor_t *mon, unsigned mode)
{
	mon->memory_mode = mode;
}

void mon_set_terminate (monitor_t *mon, int val)
{
	mon->terminate = (val != 0);
}

void mon_set_prompt (monitor_t *mon, const char *str)
{
	mon->prompt = str;
}

static
unsigned char mon_get_mem8 (monitor_t *mon, unsigned long addr)
{
	if (mon->get_mem8 != NULL) {
		return (mon->get_mem8 (mon->get_mem8_ext, addr));
	}

	return (0);
}

static
void mon_set_mem8 (monitor_t *mon, unsigned long addr, unsigned char val)
{
	if (mon->set_mem8 != NULL) {
		mon->set_mem8 (mon->set_mem8_ext, addr, val);
	}
}

#if 0
static
int mon_set_msg (monitor_t *mon, const char *msg, const char *val)
{
	if (mon->setmsg != NULL) {
		return (mon->setmsg (mon->msgext, msg, val));
	}

	return (1);
}

static
int mon_get_msg (monitor_t *mon, const char *msg, char *val, unsigned max)
{
	if (mon->getmsg != NULL) {
		return (mon->getmsg (mon->msgext, msg, val, max));
	}

	return (1);
}
#endif

static
int mon_match_address (monitor_t *mon, cmd_t *cmd, unsigned long *addr, unsigned short *seg, unsigned short *ofs)
{
	unsigned short tseg, tofs;

	if (mon->memory_mode == 0) {
		return (cmd_match_uint32 (cmd, addr));
	}
	else {
		tseg = mon->default_seg;

		if (!cmd_match_uint16_16 (cmd, &tseg, &tofs)) {
			return (0);
		}

		mon->default_seg = tseg;

		*addr = ((unsigned long) tseg << 4) + tofs;

		if (seg != NULL) {
			*seg = tseg;
		}

		if (ofs != NULL) {
			*ofs = tofs;
		}
	}

	return (1);
}

/*
 * d - dump memory
 */
static
void mon_cmd_d (monitor_t *mon, cmd_t *cmd)
{
	unsigned       i, j;
	unsigned long  addr, addr2, cnt;
	unsigned short ofs;
	unsigned long  p1, p2, o1;
	unsigned char  val, val1, val2;
	char           str1[64], str2[32];

	addr = mon->last_addr;
	ofs = mon->last_ofs;
	cnt = 256;

	if (mon_match_address (mon, cmd, &addr, NULL, &ofs)) {
		cmd_match_uint32 (cmd, &cnt);
	}

	if (!cmd_match_end (cmd)) {
		return;
	}

	if (cnt == 0) {
		return;
	}

	addr2 = (addr + cnt - 1) & 0xffffffff;

	if (addr2 < addr) {
		addr2 = 0xffffffff;
		cnt = addr2 - addr + 1;
	}

	p1 = addr & 0xfffffff0;
	p2 = (addr2 + 16) & 0xfffffff0;
	o1 = ofs & 0xfff0;

	while (p1 != p2) {
		i = p1 & 15;
		j = 3 * i;

		if (i == 0) {
			if (mon->memory_mode == 0) {
				pce_printf ("%08lX", p1);
			}
			else {
				pce_printf ("%04lX:%04lX", (p1 - o1) >> 4, o1);
			}
		}

		str1[j] = (i == 8) ? '-' : ' ';

		if ((p1 < addr) || (p1 > addr2)) {
			str1[j + 1] = ' ';
			str1[j + 2] = ' ';
			str2[i] = ' ';
		}
		else {
			val = mon_get_mem8 (mon, p1);

			val1 = (val >> 4) & 0x0f;
			val2 = val & 0x0f;

			str1[j + 1] = (val1 < 10) ? ('0' + val1) : ('A' + val1 - 10);
			str1[j + 2] = (val2 < 10) ? ('0' + val2) : ('A' + val2 - 10);

			if ((val >= 32) && (val <= 127)) {
				str2[i] = val;
			}
			else {
				str2[i] = '.';
			}
		}

		if (((p1 + 1) & 15) == 0) {
			str1[j + 3] = 0;
			str2[i + 1] = 0;

			pce_printf (" %s  %s\n", str1, str2);
		}

		p1 = (p1 + 1) & 0xffffffff;
		o1 = (o1 + 1) & 0xffff;
	}

	mon->last_addr = (addr + cnt) & 0xffffffff;
	mon->last_ofs = (ofs + cnt) & 0xffff;
}

/*
 * e - enter bytes into memory
 */
static
void mon_cmd_e (monitor_t *mon, cmd_t *cmd)
{
	unsigned       i;
	unsigned long  addr;
	unsigned short val;
	char           str[256];

	if (!mon_match_address (mon, cmd, &addr, NULL, NULL)) {
		cmd_error (cmd, "need an address");
		return;
	}

	while (1) {
		if (cmd_match_uint16 (cmd, &val)) {
			mon_set_mem8 (mon, addr, val);
			addr += 1;
		}
		else if (cmd_match_str (cmd, str, 256)) {
			i = 0;
			while (str[i] != 0) {
				mon_set_mem8 (mon, addr, str[i]);
				addr += 1;
				i += 1;
			}
		}
		else {
			break;
		}
	}

	cmd_match_end (cmd);
}

/*
 * f - find bytes in memory
 */
static
void mon_cmd_f (monitor_t *mon, cmd_t *cmd)
{
	unsigned       i, n;
	unsigned short seg, ofs;
	unsigned long  addr, cnt;
	unsigned short val;
	unsigned char  buf[256];
	char           str[256];

	if (!mon_match_address (mon, cmd, &addr, &seg, &ofs)) {
		cmd_error (cmd, "need an address");
		return;
	}

	if (!cmd_match_uint32 (cmd, &cnt)) {
		cmd_error (cmd, "need a byte count");
		return;
	}

	n = 0;

	while (n < 256) {
		if (cmd_match_uint16 (cmd, &val)) {
			buf[n++] = val;
		}
		else if (cmd_match_str (cmd, str, 256)) {
			i = 0;
			while ((n < 256) && (str[i] != 0)) {
				buf[n++] = str[i++];
			}
		}
		else {
			break;
		}
	}

	if (!cmd_match_end (cmd)) {
		return;
	}

	cnt = (cnt < n) ? 0 : (cnt - n);

	while (cnt > 0) {
		for (i = 0; i < n; i++) {
			if (mon_get_mem8 (mon, addr + i) != buf[i]) {
				break;
			}
		}

		if (i >= n) {
			if (mon->memory_mode == 0) {
				pce_printf ("%08lX\n", addr);
			}
			else {
				pce_printf ("%04X:%04X\n", seg, ofs);
			}
		}

		ofs = (ofs + 1) & 0xffff;

		if (ofs == 0) {
			seg += 0x1000;
		}

		addr += 1;

		cnt -= 1;
	}
}

static
void mon_cmd_m (monitor_t *mon, cmd_t *cmd)
{
	char msg[256];
	char val[256];

	if (!cmd_match_str (cmd, msg, 256)) {
		strcpy (msg, "");
	}

	if (!cmd_match_str (cmd, val, 256)) {
		strcpy (val, "");
	}

	if (!cmd_match_end (cmd)) {
		return;
	}

	if (mon->setmsg != NULL) {
		if (mon->setmsg (mon->msgext, msg, val)) {
			pce_puts ("error\n");
		}
	}
	else {
		pce_puts ("monitor: no set message function\n");
	}
}

static
void mon_cmd_redir_inp (monitor_t *mon, cmd_t *cmd)
{
	int  close;
	char fname[256];

	close = 0;

	if (!cmd_match_str (cmd, fname, 256)) {
		close = 1;
	}

	if (!cmd_match_end (cmd)) {
		return;
	}

	if (close) {
		pce_set_redir_inp (NULL);
	}
	else {
		if (pce_set_redir_inp (fname)) {
			pce_puts ("error setting redirection\n");
		}
		else {
			pce_printf ("redirecting from \"%s\"\n", fname);
		}
	}
}

static
void mon_cmd_redir_out (monitor_t *mon, cmd_t *cmd)
{
	int        close;
	const char *mode;
	char       fname[256];

	close = 0;
	mode = "w";

	if (cmd_match (cmd, ">")) {
		mode = "a";
	}

	if (!cmd_match_str (cmd, fname, 256)) {
		close = 1;
	}

	if (!cmd_match_end (cmd)) {
		return;
	}

	if (close) {
		pce_set_redir_out (NULL, NULL);
	}
	else {
		if (pce_set_redir_out (fname, mode)) {
			pce_puts ("error setting redirection\n");
		}
		else {
			pce_printf ("redirecting to \"%s\"\n", fname);
		}
	}
}

static
void mon_cmd_v (cmd_t *cmd)
{
	unsigned long val;

	if (cmd_match_eol (cmd)) {
		cmd_list_syms (cmd);
		return;
	}

	while (cmd_match_uint32 (cmd, &val)) {
		pce_printf ("%lX\n", val);
	}

	if (!cmd_match_end (cmd)) {
		return;
	}
}


int mon_run (monitor_t *mon)
{
	int   r;
	cmd_t cmd;

	while (mon->terminate == 0) {
		if (mon->setmsg != NULL) {
			mon->setmsg (mon->msgext, "term.release", "1");
			mon->setmsg (mon->msgext, "term.fullscreen", "0");
		}

		cmd_get (&cmd, mon->prompt);

		if (mon->docmd != NULL) {
			r = mon->docmd (mon->cmdext, &cmd);
		}
		else {
			r = 1;
		}

		if (r != 0) {
			if (cmd_match (&cmd, "d")) {
				mon_cmd_d (mon, &cmd);
			}
			else if (cmd_match (&cmd, "e")) {
				mon_cmd_e (mon, &cmd);
			}
			else if (cmd_match (&cmd, "f")) {
				mon_cmd_f (mon, &cmd);
			}
			else if (cmd_match (&cmd, "m")) {
				mon_cmd_m (mon, &cmd);
			}
			else if (cmd_match (&cmd, "q")) {
				break;
			}
			else if (cmd_match (&cmd, "v")) {
				mon_cmd_v (&cmd);
			}
			else if (cmd_match (&cmd, "<")) {
				mon_cmd_redir_inp (mon, &cmd);
			}
			else if (cmd_match (&cmd, ">")) {
				mon_cmd_redir_out (mon, &cmd);
			}
			else if (!cmd_match_eol (&cmd)) {
				cmd_error (&cmd, "unknown command");
			}
		}
	};

	return (0);
}
