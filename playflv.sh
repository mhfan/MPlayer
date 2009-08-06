#!/bin/bash
 ################################################################
 # $ID: playflv.sh     Tue, 28 Apr 2009 16:34:13 +0800  mhfan $ #
 #                                                              #
 # Description:                                                 #
 #                                                              #
 # Maintainer:  ∑∂√¿ª‘(MeiHui FAN)  <mhfan@ustc.edu>            #
 #                                                              #
 # CopyLeft (c)  2009  M.H.Fan                                  #
 #   All rights reserved.                                       #
 #                                                              #
 # This file is free software;                                  #
 #   you are free to modify and/or redistribute it  	        #
 #   under the terms of the GNU General Public Licence (GPL).   #
 ################################################################

vlc ${1/http:/http/luapl:}; exit 0

#USER_AGENT="Mozilla/4.0"
USER_AGENT="Firefox/3.0.5"
#USER_AGENT="Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Trident/4.0)"
#USER_AGENT="Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.0.5) Gecko/2008120122 Firefox/3.0.5"

DLPROG="wget --quiet -O - -U"	# 
#DLPROG="curl -A"	# XXX:

DLPROG="$DLPROG $USER_AGENT"

set -o pipefail;    ulimit -p 16;	# XXX:

dlplay () {
    FOUND=0;	while read; do
	if [ "$REPLY" = "" ]; then continue; fi
	echo "Playing: $REPLY";   FOUND=1;
#continue;

	$DLPROG "$REPLY" | \
	vlc --no-playlist-enqueue --no-loop --play-and-exit \
	    --qt-display-mode 2 --no-video-title-show fd://0 # ...
#	mplayer -cache 1024 -dr -noslices -framedrop -mc 0 -autosync 0 \
#	    -ontop -vo xv -noconfig all -quiet -really-quiet - #-fixed-vo
	if [ $? -gt "0" ]; then break; fi	#echo $?
    done

    if [ $FOUND -eq "0" ]; then
	msg="NO media(s) URL found in this page!!"; echo "$msg";
	if [ "$LANG" = "zh_CN.UTF-8" ]; then
	    msg="Êó†Ê≥ïÊâæÂà∞Ê≠§È°µÈù∏≠ÁöÑËÈ¢";
	fi; notify-send "$msg";	# XXX:
    fi
}

if true; then

URL_PARSER="http://www.flvcd.com/parse.php?flag=&format=&kw=$1"
echo "Parsing: $URL_PARSER"

$DLPROG "$URL_PARSER" | grep "^<U>" | cut -d\> -f2 | dlplay
# xargs vlc --no-playlist-enqueue --play-and-exit --no-video-title-show

else

URL_PARSER="http://www.flvxz.com/getFlv.php?url="
URL_PARSER="$URL_PARSER$(echo -n ${1/http/} | base64)"
echo "Stage1: $URL_PARSER"

URL_PARSER=$($DLPROG $URL_PARSER | cut -d\' -f2)
echo "Stage2: $URL_PARSER"	# XXX:

$DLPROG $URL_PARSER | \
    perl -pe 's/.*?down\(\\"(.*?)\\"/\1\n/g' -e ';s/^(,|\$).*//m' \
	-e ';s/amp\;//g' | dlplay	# XXX:

fi

# http://www.tudou.com/playlist/id/557313/
# http://v.youku.com/v_show/id_XODc3MTk3MTI=.html

# vim:sts=4:ts=8:
