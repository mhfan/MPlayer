#!/bin/bash
 ################################################################
 # $ID: build-arm.sh   Tue, 19 Aug 2008 01:10:46 +0800  mhfan $ #
 #                                                              #
 # Description:                                                 #
 #                                                              #
 # Maintainer:  ∑∂√¿ª‘(MeiHui FAN)  <mhfan@ustc.edu>            #
 #                                                              #
 # CopyLeft (c)  2008~2009  M.H.Fan                             #
 #   All rights reserved.                                       #
 #                                                              #
 # This file is free software;                                  #
 #   you are free to modify and/or redistribute it  	        #
 #   under the terms of the GNU General Public Licence (GPL).   #
 ################################################################

SRCDIR=$(dirname $0)

ARCH=$(uname -m); PREFIX=/usr; CROSS_COMPILE=; #CPU=core2;
source $SRCDIR/../scripts/hhtech_config.sh || \
source $SRCDIR/../../scripts/hhtech_config.sh

#CPPFLAGS="$CPPFLAGS -DCONFIG_NO_LFS=1"
CPPFLAGS="$CPPFLAGS -DFREETYPE_TTONLY=1"
CPPFLAGS="$CPPFLAGS -DCONFIG_NO_ULAW_ALAW=1"
CPPFLAGS="$CPPFLAGS -DCONFIG_DISCARD_CODEC=1"
#CPPFLAGS="$CPPFLAGS -DCONFIG_DISCARD_RARE_PIECES=1"

#CPPFLAGS="$CPPFLAGS -DCONFIG_HACK_FOR_TCCVPU=1"	# XXX:

#CPPFLAGS="$CPPFLAGS -DARCH_S3C64XX=1 -I/opt/mm-s3c/include/mfc"
#LDFLAGS="$LDFLAGS -L/opt/mm-s3c/lib -lmfc"

PATH="$PREFIX/bin:$PATH" \
$SRCDIR/configure \
  --prefix=$PREFIX \
  \
  --confdir=/etc/mplayer \
  --libdir=/usr/lib \
  \
  --disable-mencoder \
  \
  --enable-largefiles \
  \
  --disable-tv \
  \
    --enable-networking \
  \
    --disable-live \
  \
    --enable-dl \
  --disable-vcd \
  \
  --disable-dvdnav \
  \
  --disable-dvdread-internal \
  --disable-libdvdcss-internal \
  \
    --disable-bitmap-font \
  \
  --disable-unrarexec \
    --enable-menu \
  --disable-sortsub \
  \
  --disable-pthreads \
  \
    --disable-ass-internal \
    --enable-ass \
  \
  --disable-win32dll \
  --disable-qtx \
  --disable-xanim \
    --disable-real \
  \
  --disable-gif \
    --disable-png \
  --disable-mng \
    --disable-jpeg \
  \
  --disable-ffmpeg_a \
  \
  --disable-libavcodec_mpegaudio_hp \
  \
  --disable-tremor-internal \
  --enable-tremor-low \
  \
  --disable-mp3lib \
  \
  --disable-libmpeg2-internal \
  \
  --disable-decoder=all \
  --enable-decoder="H264 H263 H261 MPEG4 MPEG1VIDEO MSMPEG4V3 WMV1 WMV2 WMV3" \
  --enable-decoder="FLV FLASHSV VP6F VP6A ADPCM_SWF  VC1 RV10 RV20 RV30 RV40" \
  --enable-decoder="VP6 VP3  APE FLAC  ADPCM_IMA_WAV ADPCM_MS PCM_S16LE AC3" \
  --enable-decoder="LIBOPENCORE_AMRNB LIBOPENCORE_AMRWB  MP3 MP3ADU MP3ON4" \
  --enable-decoder="WMAV1I WMAV2I WMAPRO  COOK  PNG MJPEG H263I AAC MP2" \
  --disable-encoder=all \
  --enable-encoder="PNG" \
  --disable-parser=all \
  --enable-parser="H264 H263 H261 MPEG4VIDEO MPEGVIDEO VC1 VP3 MJPEG" \
  --enable-parser="MPEGAUDIO AAC AC3 DCA" \
  --disable-demuxer=all \
  --enable-demuxer="FLV SWF MOV MATROSKA NSV AVI ASF OGG RM H264 H263 H261" \
  --enable-demuxer="MPEGVIDEO MPEGTS MPEGPS RAWVIDEO VC1 VC1T MJPEG IMAGE2" \
  --enable-demuxer="MP3 WAV APE FLAC  PCM_S16LE AAC AC3 DTS" \
  --disable-muxer=all \
  --enable-muxer=none \
  --disable-protocol=all \
  --enable-protocol="FILE" \
  --disable-bsf=all \
  --enable-bsf="MP3_HEADER_DECOMPRESS H264_MP4TOANNEXB DUMP_EXTRADATA" \
  \
  --disable-dvb \
  \
  --disable-tga \
  --disable-pnm \
  --disable-md5sum \
  --disable-yuv4mpeg \
  \
  --enable-hardcoded-tables \
  \
  --cc=${CROSS_COMPILE}gcc \
  --host-cc=gcc \
  --as=${CROSS_COMPILE}gcc \
  --ar=${CROSS_COMPILE}ar \
  \
  --target=${TARGET} \
  \
  $* \
  \
&& sed -i -e 's/\(.*HAVE_FAST_UNALIGNED\).*/\1 1/' config.h \
&& sed -i -e 's/\(.*CONFIG_SMALL\).*/\1 1/' config.h \
&& touch configure-stamp \
#&& make all && make PREFIX=$_PREFIX install \
#&& $STRIP $_PREFIX/bin/mplayer

#  --enable-encoder="MPEG4_SSBMFC H264_SSBMFC H263_SSBMFC MJPEG_SSBMFC" \
#  --enable-decoder="MPEG4_SSBMFC H264_SSBMFC H263_SSBMFC VC1_SSBMFC" \
#  --enable-decoder="MPEG2_SSBMFC MPEG1_SSBMFC MJPEG_SSBMFC" \
#  --extra-cflags="-DCONFIG_SSBMFC=1" \
#  \
#  --enable-decoder="MPEG4_MFC H264_MFC H263_MFC  WMV3_MFC VC1_MFC" \
#  --extra-ldflags="-Wl,-rpath-link,/opt/mm-s3c/lib" \
#  --extra-cflags="-DCONFIG_LIBMFC=1" \
#  \
#  --enable-decoder="MPEG4_CDK H264_CDK WMV3_CDK VC1_CDK MPEG2_CDK MPEG1_CDK" \
#  --enable-decoder="RV40_CDK RV30_CDK RV20_CDK  FLV_CDK H263_CDK" \
#  --extra-ldflags="-Wl,-rpath-link,/opt/vpu-tcc/lib" \
#  --extra-cflags="-DCONFIG_TCCCDK=1" \

#  --with-freetype-config=$PREFIX/bin/freetype-config \
#&& sed -i -e "s/^\(INSTALLSTRIP.*\)/#\1/" config.mak \
#&& sed -i -e "s/^#undef\( FAST_OSD\w*\).*/#define\1 1/" config.h \

#sed -n 's/^[^#]*DEC.*(.*, *\(.*\)).*/\1/p' libavcodec/allcodecs.c | tr '[a-z]' '[A-Z]'
#sed -n 's/^[^#]*BSF.*(.*, *\(.*\)).*/\1/p' libavcodec/allcodecs.c | tr '[a-z]' '[A-Z]'
#sed -n 's/^[^#]*PARSER.*(.*, *\(.*\)).*/\1/p' libavcodec/allcodecs.c | tr '[a-z]' '[A-Z]'
#sed -n 's/^[^#]*DEMUX.*(.*, *\(.*\)).*/\1/p' libavformat/allformats.c | tr '[a-z]' '[A-Z]'

################################################################################
#Usage: ./configure [OPTIONS]...
#
#Configuration:
#  -h, --help             display this help and exit
#
#Installation directories:
#  --prefix=DIR           prefix directory for installation [/usr/local]
#  --bindir=DIR           directory for installing binaries [PREFIX/bin]
#  --datadir=DIR          directory for installing machine independent
#                         data files (skins, etc) [PREFIX/share/mplayer]
#  --mandir=DIR           directory for installing man pages [PREFIX/share/man]
#  --confdir=DIR          directory for installing configuration files
#                         [PREFIX/etc/mplayer]
#  --libdir=DIR           directory for object code libraries [PREFIX/lib]
#  --codecsdir=DIR        directory for binary codecs [LIBDIR/codecs]
#
#Optional features:
#  --disable-mencoder     disable MEncoder (A/V encoder) compilation [enable]
#  --disable-mplayer      disable MPlayer compilation [enable]
#  --enable-gui           enable GMPlayer compilation (GTK+ GUI) [disable]
#  --enable-gtk1          force using GTK 1.2 for the GUI  [disable]
#  --disable-largefiles   disable support for files > 2GB [enable]
#  --enable-termcap       use termcap database for key codes [autodetect]
#  --enable-termios       use termios database for key codes [autodetect]
#  --disable-iconv        disable iconv for encoding conversion [autodetect]
#  --disable-langinfo     do not use langinfo [autodetect]
#  --enable-lirc          enable LIRC (remote control) support [autodetect]
#  --enable-lircc         enable LIRCCD (LIRC client daemon) input [autodetect]
#  --enable-joystick      enable joystick support [disable]
#  --enable-apple-remote  enable Apple Remote input (Mac OS X only) [autodetect]
#  --enable-apple-ir      enable Apple IR Remote input (Linux only) [autodetect]
#  --enable-tslib         enable touchscreen input via tslib [autodetect]
#  --disable-vm           disable X video mode extensions [autodetect]
#  --disable-xf86keysym   disable support for multimedia keys [autodetect]
#  --enable-radio         enable radio interface [disable]
#  --enable-radio-capture enable radio capture (through PCI/line-in) [disable]
#  --disable-radio-v4l2   disable Video4Linux2 radio interface [autodetect]
#  --disable-radio-bsdbt848   disable BSD BT848 radio interface [autodetect]
#  --disable-tv           disable TV interface (TV/DVB grabbers) [enable]
#  --disable-tv-v4l1      disable Video4Linux TV interface [autodetect]
#  --disable-tv-v4l2      disable Video4Linux2 TV interface [autodetect]
#  --disable-tv-bsdbt848  disable BSD BT848 interface [autodetect]
#  --disable-pvr          disable Video4Linux2 MPEG PVR [autodetect]
#  --disable-rtc          disable RTC (/dev/rtc) on Linux [autodetect]
#  --disable-networking   disable networking [enable]
#  --enable-winsock2_h    enable winsock2_h [autodetect]
#  --enable-smb           enable Samba (SMB) input [autodetect]
#  --enable-live          enable LIVE555 Streaming Media [autodetect]
#  --enable-nemesi        enable Nemesi Streaming Media [autodetect]
#  --enable-librtmp       enable RTMPDump Streaming Media [autodetect]
#  --disable-dl		  disable dynamic loader [autodetect]
#  --disable-vcd          disable VCD support [autodetect]
#  --disable-bluray       disable Blu-ray support [autodetect]
#  --disable-dvdnav       disable libdvdnav [autodetect]
#  --disable-dvdread      disable libdvdread [autodetect]
#  --disable-dvdread-internal  disable internal libdvdread [autodetect]
#  --disable-libdvdcss-internal  disable internal libdvdcss [autodetect]
#  --disable-cdparanoia   disable cdparanoia [autodetect]
#  --disable-cddb         disable cddb [autodetect]
#  --disable-bitmap-font  disable bitmap font support [enable]
#  --disable-freetype     disable FreeType 2 font rendering [autodetect]
#  --disable-fontconfig   disable fontconfig font lookup [autodetect]
#  --disable-unrarexec    disable using of UnRAR executable [enabled]
#  --enable-menu          enable OSD menu (not DVD menu) [disabled]
#  --disable-sortsub      disable subtitle sorting [enabled]
#  --enable-fribidi       enable the FriBiDi libs [autodetect]
#  --disable-enca         disable ENCA charset oracle library [autodetect]
#  --disable-maemo        disable maemo specific features [autodetect]
#  --enable-macosx-finder enable Mac OS X Finder invocation parameter
#                         parsing [disabled]
#  --enable-macosx-bundle enable Mac OS X bundle file locations [autodetect]
#  --disable-inet6        disable IPv6 support [autodetect]
#  --disable-gethostbyname2  gethostbyname2 part of the C library [autodetect]
#  --disable-ftp          disable FTP support [enabled]
#  --disable-vstream      disable TiVo vstream client support [autodetect]
#  --disable-pthreads     disable Posix threads support [autodetect]
#  --disable-w32threads   disable Win32 threads support [autodetect]
#  --enable-ass-internal  enable internal SSA/ASS subtitle support [autodetect]
#  --disable-ass          disable SSA/ASS subtitle support [autodetect]
#  --enable-rpath         enable runtime linker path for extra libs [disabled]
#
#Codecs:
#  --enable-gif		    enable GIF support [autodetect]
#  --enable-png		    enable PNG input/output support [autodetect]
#  --enable-mng		    enable MNG input support [autodetect]
#  --enable-jpeg		    enable JPEG input/output support [autodetect]
#  --enable-libcdio	    enable libcdio support [autodetect]
#  --enable-liblzo	    enable liblzo support [autodetect]
#  --disable-win32dll        disable Win32 DLL support [autodetect]
#  --disable-qtx             disable QuickTime codecs support [enabled]
#  --disable-xanim           disable XAnim codecs support [enabled]
#  --disable-real            disable RealPlayer codecs support [enabled]
#  --disable-xvid            disable Xvid [autodetect]
#  --disable-xvid-lavc       disable Xvid in libavcodec [autodetect]
#  --disable-x264            disable x264 [autodetect]
#  --disable-x264-lavc       disable x264 in libavcodec [autodetect]
#  --disable-libdirac-lavc   disable Dirac in libavcodec [autodetect]
#  --disable-libschroedinger-lavc   disable Dirac in libavcodec (Schroedinger
#                                   decoder) [autodetect]
#  --disable-libvpx-lavc     disable libvpx in libavcodec [autodetect]
#  --disable-libnut          disable libnut [autodetect]
#  --disable-ffmpeg_a        disable static FFmpeg [autodetect]
#  --disable-ffmpeg_so       disable shared FFmpeg [autodetect]
#  --disable-libavcodec_mpegaudio_hp disable high precision audio decoding
#                                    in libavcodec [enabled]
#  --disable-tremor-internal disable internal Tremor [enabled]
#  --enable-tremor-low       enable lower accuracy internal Tremor [disabled]
#  --enable-tremor           enable external Tremor [autodetect]
#  --disable-libvorbis       disable libvorbis support [autodetect]
#  --disable-speex           disable Speex support [autodetect]
#  --disable-libgsm          disable libgsm support [autodetect]
#  --enable-theora           enable OggTheora libraries [autodetect]
#  --enable-faad             enable FAAD2 (AAC) [autodetect]
#  --disable-faac            disable support for FAAC (AAC encoder) [autodetect]
#  --disable-faac-lavc       disable support for FAAC in libavcodec [autodetect]
#  --disable-ladspa          disable LADSPA plugin support [autodetect]
#  --disable-libbs2b         disable libbs2b audio filter support [autodetect]
#  --disable-libdv           disable libdv 0.9.5 en/decoding support [autodetect]
#  --disable-mpg123          disable libmpg123 MP3 decoding support [autodetect]
#  --disable-mad             disable libmad (MPEG audio) support [autodetect]
#  --disable-mp3lame         disable LAME MP3 encoding support [autodetect]
#  --disable-mp3lame-lavc    disable LAME in libavcodec [autodetect]
#  --disable-toolame         disable Toolame (MPEG layer 2) encoding [autodetect]
#  --disable-twolame         disable Twolame (MPEG layer 2) encoding [autodetect]
#  --enable-xmms             enable XMMS input plugin support [disabled]
#  --enable-libdca           enable libdca support [autodetect]
#  --disable-mp3lib          disable builtin mp3lib [autodetect]
#  --disable-liba52          disable liba52 [autodetect]
#  --disable-libmpeg2        disable libmpeg2 [autodetect]
#  --disable-libmpeg2-internal disable builtin libmpeg2 [autodetect]
#  --disable-musepack        disable musepack support [autodetect]
#  --disable-libopencore_amrnb disable libopencore_amr narrowband [autodetect]
#  --disable-libopencore_amrwb disable libopencore_amr wideband [autodetect]
#  --disable-libopenjpeg     disable OpenJPEG (JPEG2000) input/output support [autodetect]
#  --disable-id3tag          disable libid3tag support [autodetect]
#  --disable-decoder=DECODER disable specified FFmpeg decoder
#  --enable-decoder=DECODER  enable specified FFmpeg decoder
#  --disable-encoder=ENCODER disable specified FFmpeg encoder
#  --enable-encoder=ENCODER  enable specified FFmpeg encoder
#  --disable-parser=PARSER   disable specified FFmpeg parser
#  --enable-parser=PARSER    enable specified FFmpeg parser
#  --disable-protocol=PROTO  disable specified FFmpeg protocol
#  --enable-protocol=PROTO   enable specified FFmpeg protocol
#  --disable-demuxer=DEMUXER disable specified FFmpeg demuxer
#  --enable-demuxer=DEMUXER  enable specified FFmpeg demuxer
#  --disable-muxer=MUXER     disable specified FFmpeg muxer
#  --enable-muxer=MUXER      enable specified FFmpeg muxer
#  --disable-bsf=BSF         disable specified FFmpeg bsf
#  --enable-bsf=BSF          enable specified FFmpeg bsf
#
#Video output:
#  --disable-vidix          disable VIDIX [for x86 *nix]
#  --with-vidix-drivers[=*] list of VIDIX drivers to be compiled in
#                           Available: cyberblade, ivtv, mach64, mga, mga_crtc2,
#                           nvidia, pm2, pm3, radeon, rage128, s3, sis, unichrome
#  --disable-vidix-pcidb    disable VIDIX PCI device name database
#  --enable-dhahelper       enable VIDIX dhahelper support
#  --enable-svgalib_helper  enable VIDIX svgalib_helper support
#  --enable-gl              enable OpenGL video output [autodetect]
#  --disable-matrixview     disable OpenGL MatrixView video output [autodetect]
#  --enable-dga2            enable DGA 2 support [autodetect]
#  --enable-dga1            enable DGA 1 support [autodetect]
#  --enable-vesa            enable VESA video output [autodetect]
#  --enable-svga            enable SVGAlib video output [autodetect]
#  --enable-sdl             enable SDL video output [autodetect]
#  --enable-kva             enable KVA video output [autodetect]
#  --enable-aa              enable AAlib video output [autodetect]
#  --enable-caca            enable CACA  video output [autodetect]
#  --enable-ggi             enable GGI video output [autodetect]
#  --enable-ggiwmh          enable GGI libggiwmh extension [autodetect]
#  --enable-direct3d        enable Direct3D video output [autodetect]
#  --enable-directx         enable DirectX video output [autodetect]
#  --enable-dxr2            enable DXR2 video output [autodetect]
#  --enable-dxr3            enable DXR3/H+ video output [autodetect]
#  --enable-ivtv            enable IVTV TV-Out video output [autodetect]
#  --enable-v4l2            enable V4L2 Decoder audio/video output [autodetect]
#  --enable-dvb             enable DVB video output [autodetect]
#  --enable-mga             enable mga_vid video output [autodetect]
#  --enable-xmga            enable mga_vid X11 video output [autodetect]
#  --enable-xv              enable Xv video output [autodetect]
#  --enable-xvmc            enable XvMC acceleration [disable]
#  --enable-vdpau           enable VDPAU acceleration [autodetect]
#  --enable-vm              enable XF86VidMode support [autodetect]
#  --enable-xinerama        enable Xinerama support [autodetect]
#  --enable-x11             enable X11 video output [autodetect]
#  --enable-xshape          enable XShape support [autodetect]
#  --disable-xss            disable screensaver support via xss [autodetect]
#  --enable-fbdev           enable FBDev video output [autodetect]
#  --enable-mlib            enable mediaLib video output (Solaris) [disable]
#  --enable-3dfx            enable obsolete /dev/3dfx video output [disable]
#  --enable-tdfxfb          enable tdfxfb video output [disable]
#  --enable-s3fb            enable s3fb (S3 ViRGE) video output [disable]
#  --enable-wii             enable Nintendo Wii/GameCube video output [disable]
#  --enable-directfb        enable DirectFB video output [autodetect]
#  --enable-zr              enable ZR360[56]7/ZR36060 video output [autodetect]
#  --enable-bl		   enable Blinkenlights video output [disable]
#  --enable-tdfxvid         enable tdfx_vid video output [disable]
#  --enable-xvr100          enable SUN XVR-100 video output [autodetect]
#  --disable-tga            disable Targa video output [enable]
#  --disable-pnm		   disable PNM video output [enable]
#  --disable-md5sum	   disable md5sum video output [enable]
#  --disable-yuv4mpeg	   disable yuv4mpeg video output [enable]
#  --disable-corevideo      disable CoreVideo video output [autodetect]
#  --disable-quartz         disable Quartz video output [autodetect]
#
#Audio output:
#  --disable-alsa         disable ALSA audio output [autodetect]
#  --disable-ossaudio     disable OSS audio output [autodetect]
#  --disable-arts         disable aRts audio output [autodetect]
#  --disable-esd          disable esd audio output [autodetect]
#  --disable-pulse        disable Pulseaudio audio output [autodetect]
#  --disable-jack         disable JACK audio output [autodetect]
#  --disable-openal       disable OpenAL audio output [autodetect]
#  --disable-nas          disable NAS audio output [autodetect]
#  --disable-sgiaudio     disable SGI audio output [autodetect]
#  --disable-sunaudio     disable Sun audio output [autodetect]
#  --disable-kai          disable KAI audio output [autodetect]
#  --disable-dart         disable DART audio output [autodetect]
#  --disable-win32waveout disable Windows waveout audio output [autodetect]
#  --disable-coreaudio    disable CoreAudio audio output [autodetect]
#  --disable-select       disable using select() on the audio device [enable]
#
#Language options:
#  --charset=charset      convert the console messages to this character set
#  --language-doc=lang    language to use for the documentation [en]
#  --language-man=lang    language to use for the man pages [en]
#  --language-msg=lang    language to use for the messages and the GUI [en]
#  --language=lang        default language to use [en]
#Specific options override --language. You can pass a list of languages separated
#by whitespace or commas instead of a single language. Nonexisting translations
#will be dropped from each list. All documentation and man page translations
#available in the list will be installed, for the messages the first available
#translation will be used. The value "all" will activate all translations. The
#LINGUAS environment variable is honored. In all cases the fallback is English.
#Available values are: all bg cs de dk el en es fr hu it ja ko mk nb nl pl ro ru sk sv tr uk pt_BR zh_CN zh_TW
#
#Miscellaneous options:
#  --enable-runtime-cpudetection    enable runtime CPU detection [disable]
#  --enable-cross-compile enable cross-compilation [autodetect]
#  --cc=COMPILER          C compiler to build MPlayer [gcc]
#  --host-cc=COMPILER     C compiler for tools needed while building [gcc]
#  --as=ASSEMBLER         assembler to build MPlayer [as]
#  --nm=NM                nm tool to build MPlayer [nm]
#  --yasm=YASM            Yasm assembler to build MPlayer [yasm]
#  --ar=AR                librarian to build MPlayer [ar]
#  --ranlib=RANLIB        ranlib to build MPlayer [ranlib]
#  --windres=WINDRES      windres to build MPlayer [windres]
#  --target=PLATFORM      target platform (i386-linux, arm-linux, etc)
#  --enable-static        build a statically linked binary
#  --with-install=PATH    path to a custom install program
#
#Advanced options:
#  --enable-mmx              enable MMX [autodetect]
#  --enable-mmxext           enable MMX2 (Pentium III, Athlon) [autodetect]
#  --enable-3dnow            enable 3DNow! [autodetect]
#  --enable-3dnowext         enable extended 3DNow! [autodetect]
#  --enable-sse              enable SSE [autodetect]
#  --enable-sse2             enable SSE2 [autodetect]
#  --enable-ssse3            enable SSSE3 [autodetect]
#  --enable-shm              enable shm [autodetect]
#  --enable-altivec          enable AltiVec (PowerPC) [autodetect]
#  --enable-armv5te          enable DSP extensions (ARM) [autodetect]
#  --enable-armv6            enable ARMv6 (ARM) [autodetect]
#  --enable-armv6t2          enable ARMv6t2 (ARM) [autodetect]
#  --enable-armvfp           enable ARM VFP (ARM) [autodetect]
#  --enable-neon             enable NEON (ARM) [autodetect]
#  --enable-iwmmxt           enable iWMMXt (ARM) [autodetect]
#  --disable-fastmemcpy      disable 3DNow!/SSE/MMX optimized memcpy [enable]
#  --enable-hardcoded-tables put tables in binary instead of calculating them at startup [disable]
#  --enable-big-endian       force byte order to big-endian [autodetect]
#  --enable-debug[=1-3]      compile-in debugging information [disable]
#  --enable-profile          compile-in profiling information [disable]
#  --disable-sighandler      disable sighandler for crashes [enable]
#  --enable-crash-debug      enable automatic gdb attach on crash [disable]
#  --enable-dynamic-plugins  enable dynamic A/V plugins [disable]
#
#Use these options if autodetection fails:
#  --extra-cflags=FLAGS        extra CFLAGS
#  --extra-ldflags=FLAGS       extra LDFLAGS
#  --extra-libs=FLAGS          extra linker flags
#  --extra-libs-mplayer=FLAGS  extra linker flags for MPlayer
#  --extra-libs-mencoder=FLAGS extra linker flags for MEncoder
#  --with-xvmclib=NAME         adapter-specific library name (e.g. XvMCNVIDIA)
#
#  --with-freetype-config=PATH path to freetype-config
#  --with-glib-config=PATH     path to glib*-config
#  --with-gtk-config=PATH      path to gtk*-config
#  --with-sdl-config=PATH      path to sdl*-config
#  --with-dvdnav-config=PATH   path to dvdnav-config
#  --with-dvdread-config=PATH   path to dvdread-config
#
#This configure script is NOT autoconf-based, even though its output is similar.
#It will try to autodetect all configuration options. If you --enable an option
#it will be forcefully turned on, skipping autodetection. This can break
#compilation, so you need to know what you are doing.
# vim:sts=4:ts=8:
