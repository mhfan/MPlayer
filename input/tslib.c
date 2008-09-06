//#!/usr/bin/tcc -run
/****************************************************************
 * $ID: tslib.c        Mon, 01 Sep 2008 08:47:59 +0800  mhfan $ *
 *                                                              *
 * Description:                                                 *
 *                                                              *
 * Maintainer:  ∑∂√¿ª‘(MeiHui FAN)  <mhfan@ustc.edu>            *
 *                                                              *
 * CopyLeft (c)  2008  M.H.Fan                                  *
 *   All rights reserved.                                       *
 *                                                              *
 * This file is free software;                                  *
 *   you are free to modify and/or redistribute it   	        *
 *   under the terms of the GNU General Public Licence (GPL).   *
 ****************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <tslib.h>

#include "config.h"
#include "mp_msg.h"

#include "osdep/keycodes.h"
#include "input.h"

extern int use_menu;
static struct tsdev* tsdev;

int mp_input_ts_exit(int fd) { if (tsdev) ts_close(tsdev); }

int mp_input_ts_init(char* dev)
{
    if (dev || (dev = getenv("TSLIB_TSDEVICE"))) {
	mp_msg(MSGT_INPUT, MSGL_V, "initialize touchscreen: %s\n", dev);
	//if (setenv("TSLIB_CONFFILE", "/mnt/disk/mplayer/ts.conf", 1)) ;
	if ((tsdev = ts_open(dev, 1))) {	// XXX: nonblock
	    if (ts_config(tsdev)) ;
	    return  ts_fd(tsdev);
	}
    }

    mp_msg(MSGT_INPUT, MSGL_ERR, "fail to init TS: %s\n", dev);

    return -1;
}

int mp_input_ts_read(int fd, char* dest, int size)
{
    static unsigned dc = 0;
    static struct ts_sample dfs;
    int r = MP_INPUT_NOTHING;
    struct ts_sample sam;
    unsigned key = 0;
    short dx, dy;

    if (ts_read(tsdev, &sam, 1) != 1) return r;

    if (sam.pressure) {
	if (!dc++) dfs = sam; else {
	    dx = sam.x - dfs.x, dy = sam.y - dfs.y;
	    if (dx * dx + dy * dy < 25) return r;	// XXX:
	}
    } else {	key = BTN_TOUCH;	// XXX:
	if (1 < (dy = sam.tv.tv_sec - dfs.tv.tv_sec) ||
		dfs.tv.tv_usec + 20 * 1000 <	// XXX:
		sam.tv.tv_usec + dy * 1000 * 1000) {

	    unsigned short adx = abs(dx = sam.x - dfs.x),
			   ady = abs(dy = sam.y - dfs.y);

	    if (10 < adx && ady * 2 < adx * 1) {	// XXX:
		if (0 < dx) key = KEY_RIGHT; else
		if (dx < 0) key = KEY_LEFT;
	    }   else
	    if (10 < ady && adx * 2 < ady * 1) {
		if (0 < dy) key = KEY_NEXT; else
		if (dy < 0) key = KEY_PREV;
		// XXX: KEY_DOWN/KEY_UP
	    }
	}	dc = 0;
    }

#if 0//def CONFIG_MENU
    if (use_menu)	// XXX:
#endif
    if (31 < size) r = sprintf(dest,
	    "set_mouse_pos %i %i\n", sam.x, sam.y);

    if (key) mplayer_put_key(key);

    return r;
}

// vim:sts=4:ts=8:
