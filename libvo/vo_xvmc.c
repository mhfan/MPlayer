#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "fastmemcpy.h"
#include "osdep/timer.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XvMClib.h>

#include "x11_common.h"
#include "xvmc_render.h"

#include "sub.h"
#include "aspect.h"

#ifdef HAVE_NEW_GUI
#include "../Gui/interface.h"
#endif

//no chanse xinerama to be suported in near future
#undef HAVE_XINERAMA

#undef NDEBUG 
#include <assert.h>


#define UNUSED(x) ((void)(x))


extern int vo_directrendering;
extern int vo_verbose;

static int benchmark;
static int busy_wait;
static int use_queue;

static int image_width,image_height;
static uint32_t  drwX,drwY;

static XvPortID xv_port;

#define AUTO_COLORKEY       0
#define BACKGROUND_COLORKEY 1
#define AUTOPAINT_COLORKEY  2
#define MANUALFILL_COLORKEY 3
static int keycolor_handling;
static unsigned long keycolor;

#define NO_SUBPICTURE      0
#define OVERLAY_SUBPICTURE 1
#define BLEND_SUBPICTURE   2
#define BACKEND_SUBPICTURE 3

static int subpicture_mode;
static int subpicture_alloc;
static XvMCSubpicture subpicture;
static XvImageFormatValues subpicture_info;
static int subpicture_clear_color;//transparent color for the subpicture or color key for overlay

static XvMCSurfaceInfo surface_info;
static XvMCContext ctx;
static XvMCBlockArray data_blocks;
static XvMCMacroBlockArray mv_blocks;

#define MAX_SURFACES 8
static int number_of_surfaces=0;
static XvMCSurface surface_array[MAX_SURFACES];
static xvmc_render_state_t * surface_render;

static xvmc_render_state_t * p_render_surface_to_show=NULL;
static xvmc_render_state_t * p_render_surface_visible=NULL;

//display queue, kinda render ahead
static xvmc_render_state_t * show_queue[MAX_SURFACES];
static int free_element;


static void (*draw_osd_fnc)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);
static void (*clear_osd_fnc)(int x0,int y0, int w,int h);
static void (*init_osd_fnc)(void);

static void   draw_osd_AI44(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);
static void   draw_osd_IA44(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);
static void   clear_osd_subpic(int x0,int y0, int w,int h);
static void   init_osd_yuv_pal(void);


static const struct{
   int id;//id as xvimages or as mplayer RGB|{8,15,16,24,32}
   void (* init_func_ptr)();
   void (* draw_func_ptr)();
   void (* clear_func_ptr)();
   } osd_render[]={
                     {0x34344149,init_osd_yuv_pal,draw_osd_AI44,clear_osd_subpic},
                     {0x34344941,init_osd_yuv_pal,draw_osd_IA44,clear_osd_subpic},
                     {0,NULL,NULL,NULL}
                  };

static void xvmc_free(void);
static int count_free_surfaces();
static xvmc_render_state_t * find_free_surface();

static vo_info_t info = {
  "XVideo Motion Compensation",
  "xvmc",
  "Ivan Kalvachev <iive@users.sf.net>",
  ""
};

LIBVO_EXTERN(xvmc);

//shm stuff from vo_xv
#ifdef HAVE_SHM
/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);
static XShmSegmentInfo Shminfo;
static int Shmem_Flag;
#endif
XvImage * xvimage;


static void allocate_xvimage(int xvimage_width,int xvimage_height,int xv_format)
{
 /*
  * allocate XvImages.  FIXME: no error checking, without
  * mit-shm this will bomb... trzing to fix ::atmos
  */
#ifdef HAVE_SHM
   if ( mLocalDisplay && XShmQueryExtension( mDisplay ) ) Shmem_Flag = 1;
   else
   {
      Shmem_Flag = 0;
      mp_msg(MSGT_VO,MSGL_INFO, "Shared memory not supported\nReverting to normal Xv\n" );
   }
   if ( Shmem_Flag )
   {
      xvimage = (XvImage *) XvShmCreateImage(mDisplay, xv_port, xv_format, 
                             NULL, xvimage_width, xvimage_height, &Shminfo);

      Shminfo.shmid    = shmget(IPC_PRIVATE, xvimage->data_size, IPC_CREAT | 0777);
      Shminfo.shmaddr  = (char *) shmat(Shminfo.shmid, 0, 0);
      Shminfo.readOnly = False;

      xvimage->data = Shminfo.shmaddr;
      XShmAttach(mDisplay, &Shminfo);
      XSync(mDisplay, False);
      shmctl(Shminfo.shmid, IPC_RMID, 0);
   }
   else
#endif
   {
      xvimage = (XvImage *) XvCreateImage(mDisplay, xv_port, xv_format, NULL, xvimage_width, xvimage_height);
      xvimage->data = malloc(xvimage->data_size);
      XSync(mDisplay,False);
   }
// memset(xvimage->data,128,xvimage->data_size);
   return;
}

static void deallocate_xvimage()
{
#ifdef HAVE_SHM
   if ( Shmem_Flag )
   {
      XShmDetach( mDisplay,&Shminfo );
      shmdt( Shminfo.shmaddr );
   }
   else
#endif
   {
      free(xvimage->data);
   }
   XFree(xvimage);

   XFlush( mDisplay );
   XSync(mDisplay, False);
   return;
}
//end of vo_xv shm/xvimage code


static void  init_keycolor(){
Atom xv_atom;
XvAttribute * attributes;
int colorkey;
int rez;
int attrib_count,i;

   keycolor=2110;

   if(keycolor_handling == AUTO_COLORKEY){
   //XV_AUTOPING_COLORKEY doesn't work for XvMC yet(NVidia 43.63)
      attributes = XvQueryPortAttributes(mDisplay, xv_port, &attrib_count);
      if(attributes!=NULL)
         for (i = 0; i < attrib_count; i++)
            if (!strcmp(attributes[i].name, "XV_AUTOPAINT_COLORKEY"))
            {
               xv_atom = XInternAtom(mDisplay, "XV_AUTOPAINT_COLORKEY", False);
               if(xv_atom!=None)
               {
                  rez=XvSetPortAttribute(mDisplay, xv_port, xv_atom, 1);
                  if(rez == Success) 
                     keycolor_handling = AUTOPAINT_COLORKEY;
               }
               break;
            }
         XFree(attributes);
   }

   xv_atom = XInternAtom(mDisplay, "XV_COLORKEY",False);
   if(xv_atom == None) return;
   rez=XvGetPortAttribute(mDisplay,xv_port, xv_atom, &colorkey);
   if(rez == Success){
      keycolor = colorkey;
      if(keycolor_handling == AUTO_COLORKEY){
         keycolor_handling = MANUALFILL_COLORKEY;
      }
   }
}

//from vo_xmga
static void mDrawColorKey(uint32_t x,uint32_t  y, uint32_t w, uint32_t h)
{
   if( (keycolor_handling != AUTOPAINT_COLORKEY) && 
       (keycolor_handling != MANUALFILL_COLORKEY) ) 
      return;

   XSetBackground( mDisplay,vo_gc,0 );
   XClearWindow( mDisplay,vo_window );

   if(keycolor_handling == MANUALFILL_COLORKEY){
      XSetForeground( mDisplay,vo_gc,keycolor );
      XFillRectangle( mDisplay,vo_window,vo_gc,x,y,w,h);
   }
   XFlush( mDisplay );
}


static int xvmc_check_surface_format(uint32_t format, XvMCSurfaceInfo * surf_info){
   if ( format == IMGFMT_XVMC_IDCT_MPEG2 ){ 
      if( surf_info->mc_type != (XVMC_IDCT|XVMC_MPEG_2) ) return -1;
      if( surf_info->chroma_format != XVMC_CHROMA_FORMAT_420 ) return -1;
      return 0;
   }      
   if ( format == IMGFMT_XVMC_MOCO_MPEG2 ){ 
      if(surf_info->mc_type != XVMC_MPEG_2) return -1;
      if(surf_info->chroma_format != XVMC_CHROMA_FORMAT_420) return -1;
      return 0;
   }      
return -1;//fail
}

//print all info needed to add new format
static void print_xvimage_format_values(XvImageFormatValues *xifv){
int i;
   printf("Format_ID = 0x%X\n",xifv->id);
   
   printf("  type = ");
   if(xifv->type == XvRGB) printf("RGB\n");
   else if(xifv->type == XvYUV) printf("YUV\n");
   else printf("Unknown\n");

   printf("  byte_order = ");
   if(xifv->byte_order == LSBFirst) printf("LSB First\n");
   else if(xifv->type == MSBFirst) printf("MSB First\n");
   else printf("Unknown\n");//yes Linux support other types too

   printf("  guid = ");
   for(i=0;i<16;i++)
      printf("%02X ",(unsigned char)xifv->guid[i]);
   printf("\n");

   printf("  bits_per_pixel = %d\n",xifv->bits_per_pixel);

   printf("  format = ");
   if(xifv->format == XvPacked) printf("XvPacked\n");
   else if(xifv->format == XvPlanar) printf("XvPlanar\n");
   else printf("Unknown\n");

   printf("  num_planes = %d\n",xifv->num_planes);

   if(xifv->type == XvRGB){
      printf("  red_mask = %0X\n",  xifv->red_mask);
      printf("  green_mask = %0X\n",xifv->green_mask);
      printf("  blue_mask = %0X\n", xifv->blue_mask);
   }
   if(xifv->type == XvYUV){
      printf("  y_sample_bits = %d\n  u_sample_bits = %d\n  v_sample_bits = %d\n",
             xifv->y_sample_bits,xifv->u_sample_bits,xifv->v_sample_bits);
      printf("  horz_y_period = %d\n  horz_u_period = %d\n  horz_v_period = %d\n",
            xifv->horz_y_period,xifv->horz_u_period,xifv->horz_v_period);
      printf("  vert_y_period = %d\n  vert_u_period = %d\n  vert_v_period = %d\n",
            xifv->vert_y_period,xifv->vert_u_period,xifv->vert_v_period);

      printf("  component_order = ");
      for(i=0;i<32;i++)
         if(xifv->component_order[i]>=32) 
            printf("%c",xifv->component_order[i]);
      printf("\n");

      printf("  scanline = ");
      if(xifv->scanline_order == XvTopToBottom) printf("XvTopToBottom\n");
      else if(xifv->scanline_order == XvBottomToTop) printf("XvBottomToTop\n");
      else printf("Unknown\n");
   }
   printf("\n");
}

// WARNING This function may changes xv_port and surface_info!
static int xvmc_find_surface_by_format(int format,int width,int height,
                                  XvMCSurfaceInfo * surf_info,int query){
int rez;
XvAdaptorInfo * ai;
int num_adaptors,i;
unsigned long p;
int s,mc_surf_num;
XvMCSurfaceInfo * mc_surf_list;

   rez = XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay),&num_adaptors,&ai);
   if( rez != Success ) return -1;
   if( verbose > 2 ) printf("vo_xvmc: Querying %d adaptors\n",num_adaptors);
   for(i=0; i<num_adaptors; i++)
   {
      if( verbose > 2) printf("vo_xvmc: Quering adaptor #%d\n",i);
      if( ai[i].type == 0 ) continue;// we need at least dummy type!
//probing ports
      for(p=ai[i].base_id; p<ai[i].base_id+ai[i].num_ports; p++)
      {
         if( verbose > 2) printf("vo_xvmc: probing port #%ld\n",p);
	 mc_surf_list = XvMCListSurfaceTypes(mDisplay,p,&mc_surf_num);
	 if( mc_surf_list == NULL || mc_surf_num == 0){
	    if( verbose > 2) printf("vo_xvmc: No XvMC supported. \n");
	    continue;
	 }
	 if( verbose > 2) printf("vo_xvmc: XvMC list have %d surfaces\n",mc_surf_num);
//we have XvMC list!
         for(s=0; s<mc_surf_num; s++)
         {
            if( width > mc_surf_list[s].max_width ) continue;
            if( height > mc_surf_list[s].max_height ) continue;
            if( xvmc_check_surface_format(format,&mc_surf_list[s])<0 ) continue;
//we have match!

            if(!query){
               rez = XvGrabPort(mDisplay,p,CurrentTime);
	       if(rez != Success){
	          if (verbose > 2) printf("vo_xvmc: Fail to grab port %ld\n",p);
	          continue;
	       }
	       printf("vo_xvmc: Port %ld grabed\n",p);
	       xv_port = p;
	    }
	    goto surface_found;
	 }//for mc surf
	 XFree(mc_surf_list);//if mc_surf_num==0 is list==NULL ?
      }//for ports
   }//for adaptors

   if(!query) printf("vo_xvmc: Could not find free matching surface. Sorry.\n");
   return 0;

// somebody know cleaner way to escape from 3 internal loops?
surface_found:

   memcpy(surf_info,&mc_surf_list[s],sizeof(XvMCSurfaceInfo));
   if( verbose > 2 || !query) 
      printf("vo_xvmc: Found matching surface with id=%X on %ld port at %d adapter\n",
             mc_surf_list[s].surface_type_id,p,i);
   return mc_surf_list[s].surface_type_id;
}

static uint32_t xvmc_draw_image(mp_image_t *mpi){
xvmc_render_state_t * rndr;

   assert(mpi!=NULL);
   assert(mpi->flags &MP_IMGFLAG_DIRECT);
//   assert(mpi->flags &MP_IMGFLAGS_DRAWBACK);

   rndr = (xvmc_render_state_t*)mpi->priv;//there is copy in plane[2]
   assert( rndr != NULL );
   assert( rndr->magic == MP_XVMC_RENDER_MAGIC );
   if( verbose > 3 ) 
       printf("vo_xvmc: draw_image(show rndr=%p)\n",rndr);
// the surface have passed vf system without been skiped, it will be displayed
   rndr->state |= MP_XVMC_STATE_DISPLAY_PENDING;
   p_render_surface_to_show = rndr;
   return VO_TRUE;
}

static uint32_t preinit(const char *arg){
int xv_version,xv_release,xv_request_base,xv_event_base,xv_error_base;
int mc_eventBase,mc_errorBase;
int mc_ver,mc_rev;

   //Obtain display handler
   if (!vo_init()) return -1;//vo_xv

   //XvMC is subdivision of XVideo
   if (Success != XvQueryExtension(mDisplay,&xv_version,&xv_release,&xv_request_base,
			 &xv_event_base,&xv_error_base) ){
      mp_msg(MSGT_VO,MSGL_ERR,"Sorry, Xv(MC) not supported by this X11 version/driver\n");
      mp_msg(MSGT_VO,MSGL_ERR,"********** Try with  -vo x11  or  -vo sdl  ***********\n");
      return -1;
   }
   printf("vo_xvmc: X-Video extension %d.%d\n",xv_version,xv_release);

   if( True != XvMCQueryExtension(mDisplay,&mc_eventBase,&mc_errorBase) ){	
      printf("vo_xvmc: No X-Video MotionCompensation Extension on %s\n",
	      XDisplayName(NULL));
      return -1;
   }

   if(Success == XvMCQueryVersion(mDisplay, &mc_ver, &mc_rev) ){
      printf("vo_xvmc: X-Video MotionCompensation Extension version %i.%i\n",
		mc_ver,mc_rev);
   }
   else{
      printf("vo_xvmc: Error querying version info!\n");
      return -1;
   }
   surface_render = NULL;
   xv_port = 0;
   number_of_surfaces = 0;
   keycolor_handling = MANUALFILL_COLORKEY;//fixme
   subpicture_alloc = 0;
   
   benchmark = 0; //disable PutImageto allow faster display than screen refresh
   busy_wait = 1;
   use_queue = 0;
   if(arg)
      while(*arg){
         if(strncmp(arg,"benchmark",9) == 0){
            arg+=9;
            if(*arg == ':') arg++;
            benchmark = 1;//disable PutImageto allow faster display than screen refresh
            continue;
         }
         if(strncmp(arg,"wait",4) == 0){
            arg+=4;
            if(*arg == ':') arg++;
            busy_wait = 1;
            continue;
         }
         if(strncmp(arg,"sleep",5) == 0){
            arg+=5;
            if(*arg == ':') arg++;
            busy_wait = 0;
            continue;
         }
         if(strncmp(arg,"queue",5) == 0){
            arg+=5;
            if(*arg == ':') arg++;
            use_queue = 1;
            continue;
         }
         break;
      }

   return 0;
}

static uint32_t config(uint32_t width, uint32_t height,
		       uint32_t d_width, uint32_t d_height,
		       uint32_t flags, char *title, uint32_t format){
int i,mode_id,rez;
int numblocks,blocks_per_macroblock;//bpmb we have 6,8,12

//from vo_xv
char *hello = (title == NULL) ? "XvMC render" : title;
XSizeHints hint;
XVisualInfo vinfo;
XGCValues xgcv;
XSetWindowAttributes xswa;
XWindowAttributes attribs;
unsigned long xswamask;
int depth;
#ifdef HAVE_XF86VM
int vm=0;
unsigned int modeline_width, modeline_height;
static uint32_t vm_width;
static uint32_t vm_height;
#endif
//end of vo_xv

   if( !IMGFMT_IS_XVMC(format) )
   {
      assert(0);//should never happen, abort on debug or
      return 1;//return error on relese
   }

// Find free port that supports MC, by querying adaptors
   if( xv_port != 0 || number_of_surfaces != 0 ){
      xvmc_free();
   };
   numblocks=((width+15)/16)*((height+15)/16);
// Find Supported Surface Type
   mode_id = xvmc_find_surface_by_format(format,width,height,&surface_info,0);//false=1 to grab port, not query

   rez = XvMCCreateContext(mDisplay, xv_port,mode_id,width,height,XVMC_DIRECT,&ctx);
   if( rez != Success ) return -1;
   if( ctx.flags & XVMC_DIRECT ){
      printf("vo_xvmc: Allocated Direct Context\n");
   }else{
      printf("vo_xvmc: Allocated Indirect Context!\n");
   }


   blocks_per_macroblock = 6;
   if(surface_info.chroma_format == XVMC_CHROMA_FORMAT_422)
      blocks_per_macroblock = 8;
   if(surface_info.chroma_format == XVMC_CHROMA_FORMAT_444)
      blocks_per_macroblock = 12;

   rez = XvMCCreateBlocks(mDisplay,&ctx,numblocks*blocks_per_macroblock,&data_blocks);
   if( rez != Success ){
      XvMCDestroyContext(mDisplay,&ctx);
      return -1;
   }
   printf("vo_xvmc: data_blocks allocated\n");

   rez = XvMCCreateMacroBlocks(mDisplay,&ctx,numblocks,&mv_blocks);
   if( rez != Success ){
      XvMCDestroyBlocks(mDisplay,&data_blocks);
      XvMCDestroyContext(mDisplay,&ctx);
      return -1;
   }
   printf("vo_xvmc: mv_blocks allocated\n");

   if(surface_render==NULL)
      surface_render=malloc(MAX_SURFACES*sizeof(xvmc_render_state_t));//easy mem debug

   for(i=0; i<MAX_SURFACES; i++){
      rez=XvMCCreateSurface(mDisplay,&ctx,&surface_array[i]);
      if( rez != Success )
	 break;
      memset(&surface_render[i],0,sizeof(xvmc_render_state_t));
      surface_render[i].magic = MP_XVMC_RENDER_MAGIC;
      surface_render[i].data_blocks = data_blocks.blocks;
      surface_render[i].mv_blocks = mv_blocks.macro_blocks;
      surface_render[i].total_number_of_mv_blocks = numblocks;
      surface_render[i].total_number_of_data_blocks = numblocks*blocks_per_macroblock;;
      surface_render[i].mc_type = surface_info.mc_type & (~XVMC_IDCT);
      surface_render[i].idct = (surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT;
      surface_render[i].chroma_format = surface_info.chroma_format;
      surface_render[i].unsigned_intra = (surface_info.flags & XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED;
      surface_render[i].p_surface = &surface_array[i];
      if( verbose > 3 )
          printf("vo_xvmc: surface[%d] = %p .rndr=%p\n",i,&surface_array[i], &surface_render[i]);
   }
   number_of_surfaces = i;
   if( number_of_surfaces < 4 ){// +2 I or P and +2 for B (to avoid visible motion drawing)
      printf("vo_xvmc: Unable to allocate at least 4 Surfaces\n");
      uninit();
      return -1;
   }
   printf("vo_xvmc: Motion Compensation context allocated - %d surfaces\n",
          number_of_surfaces);

  //debug
   printf("vo_xvmc: idct=%d unsigned_intra=%d\n",
           (surface_info.mc_type & XVMC_IDCT) == XVMC_IDCT,
	   (surface_info.flags & XVMC_INTRA_UNSIGNED) == XVMC_INTRA_UNSIGNED);

// Find way to display OSD & subtitle
   printf("vo_xvmc: looking for OSD support\n");
   subpicture_mode = NO_SUBPICTURE;
   if(surface_info.flags & XVMC_OVERLAID_SURFACE)
      subpicture_mode = OVERLAY_SUBPICTURE;

   if(surface_info.subpicture_max_width  != 0 && 
      surface_info.subpicture_max_height != 0  ){
      int s,k,num_subpic;

      XvImageFormatValues * xvfmv;
      xvfmv = XvMCListSubpictureTypes(mDisplay, xv_port,
                      surface_info.surface_type_id, &num_subpic);

      if(num_subpic != 0 && xvfmv != NULL){
         if(verbose > 3){//Print All subpicture types for debug
            for(s=0;s<num_subpic;s++)
               printf("    Subpicture id 0x%08X\n",xvfmv[s].id);
         }

         for(s=0;s<num_subpic;s++){
            for(k=0;osd_render[k].draw_func_ptr!=NULL;k++){
               if(xvfmv[s].id == osd_render[k].id)
               {  
                  init_osd_fnc  = osd_render[k].init_func_ptr;
                  draw_osd_fnc  = osd_render[k].draw_func_ptr;
                  clear_osd_fnc = osd_render[k].clear_func_ptr;

                  subpicture_mode = BLEND_SUBPICTURE;
                  subpicture_info = xvfmv[s];
                  print_xvimage_format_values(&subpicture_info);
                  goto found_subpic;
               }
            }
         }
found_subpic:
         XFree(xvfmv); 
      }
      //Blend2 supicture is always possible, blend1 only at backend
      if( (subpicture_mode == BLEND_SUBPICTURE) &&
          (surface_info.flags & XVMC_BACKEND_SUBPICTURE) )
      {
         subpicture_mode = BACKEND_SUBPICTURE;
      }

   }

   switch(subpicture_mode){
      case NO_SUBPICTURE:
         printf("vo_xvmc: No OSD support for this mode\n");
         break;
      case OVERLAY_SUBPICTURE:
         printf("vo_xvmc: OSD support via color key tricks\n");
         printf("vo_xvmc: not yet implemented:(\n");
         break;
      case BLEND_SUBPICTURE:
         printf("vo_xvmc: OSD support by additional frontend rendering\n");
         break;
      case BACKEND_SUBPICTURE:
         printf("vo_xvmc: OSD support by beckend rendering (fast)\n");
         printf("vo_xvmc: Pleace send feedback to configrm that it work,otherwise send bugreport!\n");
         break;
   }

   init_keycolor();// take keycolor value and choose method for handling it

//taken from vo_xv
   panscan_init();

   aspect_save_orig(width,height);
   aspect_save_prescale(d_width,d_height);

   image_height = height;
   image_width = width;

   vo_mouse_autohide = 1;

   vo_dx=( vo_screenwidth - d_width ) / 2; vo_dy=( vo_screenheight - d_height ) / 2;
   geometry(&vo_dx, &vo_dy, &d_width, &d_height, vo_screenwidth, vo_screenheight);
   vo_dwidth=d_width; vo_dheight=d_height;

#ifdef HAVE_XF86VM
   if( flags&0x02 ) vm = 1;
#endif

   aspect_save_screenres(vo_screenwidth,vo_screenheight);

#ifdef HAVE_NEW_GUI
   if(use_gui)
      guiGetEvent( guiSetShVideo,0 ); // let the GUI to setup/resize our window
   else
#endif
   {
      hint.x = vo_dx;
      hint.y = vo_dy;
      aspect(&d_width,&d_height,A_NOZOOM);
      hint.width = d_width;
      hint.height = d_height;
#ifdef HAVE_XF86VM
      if ( vm )
      {
	 if ((d_width==0) && (d_height==0))
	    { vm_width=image_width; vm_height=image_height; }
	 else
	    { vm_width=d_width; vm_height=d_height; }
	 vo_vm_switch(vm_width, vm_height,&modeline_width, &modeline_height);
	 hint.x=(vo_screenwidth-modeline_width)/2;
	 hint.y=(vo_screenheight-modeline_height)/2;
	 hint.width=modeline_width;
	 hint.height=modeline_height;
	 aspect_save_screenres(modeline_width,modeline_height);
      }
      else
#endif
      if ( vo_fs )
      {
#ifdef X11_FULLSCREEN
     /* this code replaces X11_FULLSCREEN hack in mplayer.c
      * aspect() is available through aspect.h for all vos.
      * besides zooming should only be done with -zoom,
      * but I leave the old -fs behaviour so users don't get
      * irritated for now (and send lots o' mails ;) ::atmos
      */

         aspect(&d_width,&d_height,A_ZOOM);
#endif

      }
   vo_dwidth=d_width; vo_dheight=d_height;
   hint.flags = PPosition | PSize /* | PBaseSize */;
   hint.base_width = hint.width; hint.base_height = hint.height;
   XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
   depth=attribs.depth;
   if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
   XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   if (keycolor_handling == BACKGROUND_COLORKEY)
      xswa.background_pixel = keycolor;// 2110;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

   if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : mRootWin;
      if ( WinID ) 
      {
         XUnmapWindow( mDisplay,vo_window );
         XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
	 vo_x11_selectinput_witherr( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | PropertyChangeMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask | ExposureMask );
         XMapWindow( mDisplay,vo_window );
      } else { drwX=vo_dx; drwY=vo_dy; }
   } else 
      if ( vo_window == None ){
         vo_window = XCreateWindow(mDisplay, mRootWin,
              hint.x, hint.y, hint.width, hint.height,
              0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

         vo_x11_classhint( mDisplay,vo_window,"xvmc" );
         vo_hidecursor(mDisplay,vo_window);

         vo_x11_selectinput_witherr(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | PropertyChangeMask | ExposureMask |
	      ((WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask)) );
         XSetStandardProperties(mDisplay, vo_window, hello, hello, None, NULL, 0, &hint);
         XSetWMNormalHints( mDisplay,vo_window,&hint );
	 XMapWindow(mDisplay, vo_window);
	 if ( flags&1 ) vo_x11_fullscreen();
	 else {
#ifdef HAVE_XINERAMA
	    vo_x11_xinerama_move(mDisplay,vo_window);
#endif
	    vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );
	 }
      } else {
	// vo_fs set means we were already at fullscreen
	 vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );
	 if ( !vo_fs ) XMoveResizeWindow( mDisplay,vo_window,hint.x,hint.y,hint.width,hint.height );
	 if ( flags&1 && !vo_fs ) vo_x11_fullscreen(); // handle -fs on non-first file
      }

//    vo_x11_sizehint( hint.x, hint.y, hint.width, hint.height,0 );   

      if ( vo_gc != None ) XFreeGC( mDisplay,vo_gc );
      vo_gc = XCreateGC(mDisplay, vo_window, GCForeground, &xgcv);
      XFlush(mDisplay);
      XSync(mDisplay, False);
#ifdef HAVE_XF86VM
      if ( vm )
      {
      /* Grab the mouse pointer in our window */
         if(vo_grabpointer)
         XGrabPointer(mDisplay, vo_window, True, 0,
                      GrabModeAsync, GrabModeAsync,
                      vo_window, None, CurrentTime );
         XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
      }
#endif
   }

   aspect(&vo_dwidth,&vo_dheight,A_NOZOOM);
   if ( (( flags&1 )&&( WinID <= 0 )) || vo_fs )
   {
      aspect(&vo_dwidth,&vo_dheight,A_ZOOM);
      drwX=( vo_screenwidth - (vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth) ) / 2;
      drwY=( vo_screenheight - (vo_dheight > vo_screenheight?vo_screenheight:vo_dheight) ) / 2;
      vo_dwidth=(vo_dwidth > vo_screenwidth?vo_screenwidth:vo_dwidth);
      vo_dheight=(vo_dheight > vo_screenheight?vo_screenheight:vo_dheight);
      mp_msg(MSGT_VO,MSGL_V, "[xvmc-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
   }

   panscan_calc();

   mp_msg(MSGT_VO,MSGL_V, "[xvmc] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );

   saver_off(mDisplay);  // turning off screen saver
//end vo_xv

   /* store image dimesions for displaying */
   p_render_surface_visible = NULL;
   p_render_surface_to_show = NULL;

   free_element = 0;

   vo_directrendering = 1;//ugly hack, coz xvmc works only with direct rendering
   return 0;		
}

static uint32_t draw_frame(uint8_t *srcp[]){
   UNUSED(srcp);
   assert(0);
}

static void init_osd_yuv_pal(){
   char * palette;
   int rez;
   int i,j;
   int snum,seb;
   int Y,U,V;

   subpicture_clear_color = 0;

   if(subpicture.num_palette_entries > 0){

      snum = subpicture.num_palette_entries;
      seb = subpicture.entry_bytes;
      palette = (char*)malloc(snum*seb);//check fail
      if(palette == NULL) return;
      for(i=0; i<snum; i++){
         // 0-black max-white the other are gradients
         Y = i*(1 << subpicture_info.y_sample_bits)/snum;//snum=2;->(0),(1*(1<<1)/2)
         U = 1 << (subpicture_info.u_sample_bits - 1);
         V = 1 << (subpicture_info.v_sample_bits - 1);
         for(j=0; j<seb; j++)
            switch(subpicture.component_order[j]){
               case 'U': palette[i*seb+j] = U; break;
               case 'V': palette[i*seb+j] = V; break;
               case 'Y': 
               default:
                         palette[i*seb+j] = Y; break;
         }
      }
      rez = XvMCSetSubpicturePalette(mDisplay, &subpicture, palette);
      if(rez!=Success){
         printf("vo_xvmc: set pallete fail\n");
      }
   }
}

static void clear_osd_subpic(int x0, int y0, int w, int h){
int rez;
   rez=XvMCClearSubpicture(mDisplay, &subpicture,
                       x0, y0, w,h,
                       subpicture_clear_color);
   if(rez != Success)
      printf("vo_xvmc: XvMCClearSubpicture failed!\n");
}

static void OSD_init(){
unsigned short osd_height, osd_width;
int rez;

   if(subpicture_alloc){
      if(verbose>3)
         printf("vo_xvmc: destroying subpicture\n");
      XvMCDestroySubpicture(mDisplay,&subpicture);
      deallocate_xvimage();
      subpicture_alloc = 0;
   }

/*   if(surface_info.flags & XVMC_SUBPICTURE_INDEPENDENT_SCALING){
      osd_width = vo_dwidth;
      osd_height = vo_dheight;
   }else*/
   {
      osd_width = image_width;
      osd_height = image_height;
   }

   if(osd_width > surface_info.subpicture_max_width)
      osd_width = surface_info.subpicture_max_width;
   if(osd_height > surface_info.subpicture_max_height)
      osd_height = surface_info.subpicture_max_height;
   if(osd_width == 0 || osd_height == 0) 
      return;//if called before window size is known

   if(verbose > 3)
      printf("vo_xvmc: creating subpicture (%d,%d) format %X\n",
              osd_width,osd_height,subpicture_info.id);

   rez = XvMCCreateSubpicture(mDisplay,&ctx,&subpicture,
                           osd_width,osd_height,subpicture_info.id);
   if(rez != Success){
      subpicture_mode = NO_SUBPICTURE;
      printf("vo_xvmc: Create Subpicture failed, OSD disabled\n");
      return;
   }
   if(verbose > 3){
   int i;
      printf("vo_xvmc: Created Subpicture:\n");
      printf("         xvimage_id=0x%X\n",subpicture.xvimage_id);
      printf("         width=%d\n",subpicture.width);
      printf("         height=%d\n",subpicture.height);
      printf("         num_palette_entries=0x%X\n",subpicture.num_palette_entries);
      printf("         entry_bytes=0x%X\n",subpicture.entry_bytes);

      printf("         component_order=\"");
      for(i=0; i<4; i++)
         if(subpicture.component_order[i] >= 32)
            printf("%c", subpicture.component_order[i]);
      printf("\"\n");
   }
   
   //call init for the surface type
   init_osd_fnc();//init palete,clear color etc ...
   if(verbose > 3)
      printf("vo_xvmc: clearing subpicture\n");
   clear_osd_fnc(0, 0, subpicture.width, subpicture.height);

   allocate_xvimage(subpicture.width, subpicture.height, subpicture_info.id);
   subpicture_alloc = 1;
}

static void draw_osd_IA44(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
int ox,oy;
int rez;

   if(verbose > 3)
      printf("vo_xvmc:composite AI44 subpicture (%d,%d - %d,%d)\n",x0,y0,w,h);

   for(ox=0; ox<w; ox++){
      for(oy=0; oy<h; oy++){
         xvimage->data[oy*xvimage->width+ox] = (src[oy*stride+ox]>>4) | ((0-srca[oy*stride+ox])&0xf0);
      }
   }
   rez = XvMCCompositeSubpicture(mDisplay, &subpicture, xvimage, 0, 0,
                           w,h,x0,y0);
   if(rez != Success){
      printf("vo_xvmc: composite subpicture failed\n");
      assert(0);
   }
}

static void draw_osd_AI44(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride){
int ox,oy;
int rez;
   if( verbose > 3)
      printf("vo_xvmc:composite AI44 subpicture (%d,%d - %d,%d)\n",x0,y0,w,h);

   for(ox=0; ox<w; ox++){
      for(oy=0; oy<h; oy++){
         xvimage->data[oy*xvimage->width+ox] = (src[oy*stride+ox]&0xf0) | (((0-srca[oy*stride+ox])>>4)&0xf);
      }
   }
   rez = XvMCCompositeSubpicture(mDisplay, &subpicture, xvimage, 0, 0,
                           w,h,x0,y0);
   if(rez != Success){
      printf("vo_xvmc: composite subpicture failed\n");
      assert(0);
   }
}

static void draw_osd(void){
xvmc_render_state_t * osd_rndr;
int osd_has_changed;
int have_osd_to_draw;
int rez;

   if(verbose > 3)
      printf("vo_xvmc: draw_osd ,OSD_mode=%d, surface_to_show=%p\n",
             subpicture_mode,p_render_surface_to_show);

   if(subpicture_mode == BLEND_SUBPICTURE || 
      subpicture_mode == BACKEND_SUBPICTURE ){

      if(!subpicture_alloc) //allocate subpicture when dimensions are known
         OSD_init();
      if(!subpicture_alloc) 
         return;//dimensions still unknown.

      osd_has_changed = vo_update_osd(subpicture.width, subpicture.height);
      have_osd_to_draw = vo_osd_check_range_update(0, 0, subpicture.width, 
                                                         subpicture.height); 

      if(!have_osd_to_draw)
         return;//nothing to draw,no subpic, no blend

      if(osd_has_changed){
         //vo_remove_text(subpicture.width, subpicture.height,clear_osd_fnc)
         clear_osd_fnc(0,0,subpicture.width,subpicture.height);
         vo_draw_text(subpicture.width, subpicture.height, draw_osd_fnc);
      }
      XvMCSyncSubpicture(mDisplay,&subpicture);//todo usleeep wait!

      if(subpicture_mode == BLEND_SUBPICTURE){
         osd_rndr = find_free_surface();
         if(osd_rndr == NULL) 
            return;// no free surface to draw OSD in

         rez = XvMCBlendSubpicture2(mDisplay,
                       p_render_surface_to_show->p_surface, osd_rndr->p_surface,
                       &subpicture,
                       0, 0, subpicture.width, subpicture.height,
                       0, 0, image_width, image_height);
         if(rez!=Success){
            printf("vo_xvmc: BlendSubpicture failed rez=%d\n",rez);
            assert(0);
            return;
         }
//       XvMCFlushSurface(mDisplay,osd_rndr->p_surface);//fixme- should I?

         //When replaceing the surface with osd one, save the flags too!
         osd_rndr->picture_structure = p_render_surface_to_show->picture_structure;
         osd_rndr->display_flags = p_render_surface_to_show->display_flags;
//add more if needed    osd_rndr-> = p_render_surface_to_show->;

         p_render_surface_to_show->state &= ~MP_XVMC_STATE_DISPLAY_PENDING;
         p_render_surface_to_show->state |= MP_XVMC_STATE_OSD_SOURCE;
         p_render_surface_to_show->p_osd_target_surface_render = osd_rndr;

         p_render_surface_to_show = osd_rndr;
         p_render_surface_to_show->state = MP_XVMC_STATE_DISPLAY_PENDING;

         if(verbose > 3)
            printf("vo_xvmc:draw_osd: surface_to_show changed to %p\n",osd_rndr);
      }//endof if(BLEND)
      if(subpicture_mode == BACKEND_SUBPICTURE){
         rez = XvMCBlendSubpicture(mDisplay,
                       p_render_surface_to_show->p_surface,
                       &subpicture,
                       0, 0, subpicture.width, subpicture.height,
                       0, 0, image_width, image_height);

      }
   
   }//if(BLEND||BACKEND)
}

static void xvmc_sync_surface(XvMCSurface * srf){
int status,rez;
   rez = XvMCGetSurfaceStatus(mDisplay,srf,&status);
   assert(rez==Success);
   if((status & XVMC_RENDERING) == 0)
      return;//surface is already complete
   if(!busy_wait){
      rez = XvMCFlushSurface(mDisplay, srf);
      assert(rez==Success);

      do{
         usec_sleep(1000);//1ms (may be 20ms on linux)
         XvMCGetSurfaceStatus(mDisplay,srf,&status);
      } while (status & XVMC_RENDERING);
      return;//done
   }       

   XvMCSyncSurface(mDisplay, srf);
}

static void flip_page(void){
int rez;
int clipX,clipY,clipW,clipH;
int i,cfs;

   clipX = drwX-(vo_panscan_x>>1);
   clipY = drwY-(vo_panscan_y>>1); 
   clipW = vo_dwidth+vo_panscan_x;
   clipH = vo_dheight+vo_panscan_y;

   if( verbose > 3 ) 
      printf("vo_xvmc: flip_page  show(rndr=%p)\n\n",p_render_surface_to_show);

   if(p_render_surface_to_show == NULL) return;
   assert( p_render_surface_to_show->magic == MP_XVMC_RENDER_MAGIC );
//fixme   assert( p_render_surface_to_show != p_render_surface_visible);

   if(use_queue){
      // fill the queue until only n free surfaces remain
      // after that start displaying
      cfs = count_free_surfaces();
      show_queue[free_element++] = p_render_surface_to_show;
      if(cfs > 3){//well have 3 free surfaces after add queue
         if(free_element > 1)//a little voodoo magic
            xvmc_sync_surface(show_queue[0]->p_surface);
         return; 
      }
      p_render_surface_to_show=show_queue[0];
      if(verbose > 4)
         printf("vo_xvmc: flip_queue free_element=%d\n",free_element);
      free_element--;
      for(i=0; i<free_element; i++){
         show_queue[i] = show_queue[i+1];
      }
      show_queue[free_element] = NULL;
   }

// make sure the rendering is done
   xvmc_sync_surface(p_render_surface_to_show->p_surface);

//the visible surface won't be displayed anymore, mark it as free
   if(p_render_surface_visible != NULL)
      p_render_surface_visible->state &= ~MP_XVMC_STATE_DISPLAY_PENDING;

//!!fixme   assert(p_render_surface_to_show->state & MP_XVMC_STATE_DISPLAY_PENDING);

   //show it, displaying is always vsynced, so skip it for benchmark
   if(!benchmark){
      rez = XvMCPutSurface(mDisplay, p_render_surface_to_show->p_surface, 
                         vo_window,
                         0, 0, image_width, image_height,
                         clipX, clipY, clipW, clipH,
                         3);//p_render_surface_to_show->display_flags);
      if(rez != Success){
         printf("vo_xvmc: PutSurface failer, critical error!\n");
         assert(0);
      }
   }

   p_render_surface_visible = p_render_surface_to_show;
   p_render_surface_to_show = NULL;
}

static void check_events(void){
int dwidth,dheight;
Window mRoot;
uint32_t drwBorderWidth,drwDepth;

int e=vo_x11_check_events(mDisplay);
   if(e&VO_EVENT_RESIZE)
   {
      e |= VO_EVENT_EXPOSE;

      XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&vo_dwidth,&vo_dheight,
                   &drwBorderWidth,&drwDepth );
      drwX = drwY = 0;
      mp_msg(MSGT_VO,MSGL_V, "[xvmc] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,
              vo_dwidth,vo_dheight );

      aspect(&dwidth,&dheight,A_NOZOOM);
      if ( vo_fs )
      {
         aspect(&dwidth,&dheight,A_ZOOM);
         drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
         drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
         vo_dwidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
         vo_dheight=(dheight > vo_screenheight?vo_screenheight:dheight);
         mp_msg(MSGT_VO,MSGL_V, "[xvmc-fs] dx: %d dy: %d dw: %d dh: %d\n",drwX,drwY,vo_dwidth,vo_dheight );
      }
   }
   if ( e & VO_EVENT_EXPOSE )
   {
      mDrawColorKey(drwX,drwY,vo_dwidth,vo_dheight);
      if(p_render_surface_visible != NULL)
         XvMCPutSurface(mDisplay, p_render_surface_visible->p_surface,vo_window,
                     0, 0, image_width, image_height,
                     drwX,drwY,vo_dwidth,vo_dheight,
                     3);//,p_render_surface_visible->display_flags);!!
   }
}

static void xvmc_free(void){
int i;
   if( subpicture_alloc ){

      XvMCDestroySubpicture(mDisplay,&subpicture);
      deallocate_xvimage();

      subpicture_alloc = 0;

      if(verbose > 3)
         printf("vo_xvmc: subpicture destroyed\n");
   }

   if( number_of_surfaces ){

      XvMCDestroyMacroBlocks(mDisplay,&mv_blocks);
      XvMCDestroyBlocks(mDisplay,&data_blocks);

      for(i=0; i<number_of_surfaces; i++)
      {
         XvMCHideSurface(mDisplay,&surface_array[i]);//it doesn't hurt, I hope
         XvMCDestroySurface(mDisplay,&surface_array[i]);

         if( (surface_render[i].state != 0) && 
             (p_render_surface_visible != &surface_render[i]) )
            printf("vo_xvmc::uninit surface_render[%d].status=%d\n",i,
                    surface_render[i].state); 
      }

      free(surface_render);surface_render=NULL;

      XvMCDestroyContext(mDisplay,&ctx);
      number_of_surfaces = 0;

      if(verbose > 3) printf("vo_xvmc: Context sucessfuly freed\n");
   }


   if( xv_port !=0 ){
      XvUngrabPort(mDisplay,xv_port,CurrentTime);
      xv_port = 0;
      if(verbose > 3) printf("vo_xvmc: xv_port sucessfuly ungrabed\n");
   }
}

static void uninit(void){
   if( verbose > 3 ) printf("vo_xvmc: uninit called\n");
   xvmc_free();
 //from vo_xv
   saver_on(mDisplay);
   vo_vm_close(mDisplay);
   vo_x11_uninit();
}

static uint32_t query_format(uint32_t format){
uint32_t flags;
XvMCSurfaceInfo qsurface_info;
int mode_id;

   if(verbose > 3)
      printf("vo_xvmc: query_format=%X\n",format);

   if(!IMGFMT_IS_XVMC(format)) return 0;// no caps supported
   mode_id = xvmc_find_surface_by_format(format, 16, 16, &qsurface_info, 1);//true=1 - quering

   if( mode_id == 0 ) return 0;

   flags = VFCAP_CSP_SUPPORTED |
           VFCAP_CSP_SUPPORTED_BY_HW |
	   VFCAP_ACCEPT_STRIDE;

   if( (qsurface_info.subpicture_max_width  != 0) &&
       (qsurface_info.subpicture_max_height != 0) )
      flags|=VFCAP_OSD;
   return flags;
}


static uint32_t draw_slice(uint8_t *image[], int stride[],
			   int w, int h, int x, int y){
xvmc_render_state_t * rndr;
int rez;

   if(verbose > 3)
      printf("vo_xvmc: draw_slice y=%d\n",y);

   rndr = (xvmc_render_state_t*)image[2];//this is copy of priv-ate
   assert( rndr != NULL );
   assert( rndr->magic == MP_XVMC_RENDER_MAGIC );

   rez = XvMCRenderSurface(mDisplay,&ctx,rndr->picture_structure,
             		   rndr->p_surface,
                           rndr->p_past_surface,
                           rndr->p_future_surface,
                           rndr->flags,
                           rndr->filled_mv_blocks_num,rndr->start_mv_blocks_num,
                           &mv_blocks,&data_blocks);
#if 1
   if(rez != Success)
   {
   int i;
      printf("vo_xvmc::slice: RenderSirface returned %d\n",rez);

      printf("vo_xvmc::slice: pict=%d,flags=%x,start_blocks=%d,num_blocks=%d\n",
             rndr->picture_structure,rndr->flags,rndr->start_mv_blocks_num,
             rndr->filled_mv_blocks_num);
      printf("vo_xvmc::slice: this_surf=%p, past_surf=%p, future_surf=%p\n",
             rndr->p_surface,rndr->p_past_surface,rndr->p_future_surface);

      for(i=0; i<rndr->filled_mv_blocks_num; i++){
       XvMCMacroBlock* testblock;
         testblock = &mv_blocks.macro_blocks[i];

	 printf("vo_xvmc::slice: mv_block - x=%d,y=%d,mb_type=0x%x,mv_type=0x%x,mv_field_select=%d\n",
	        testblock->x,testblock->y,testblock->macroblock_type,
	        testblock->motion_type,testblock->motion_vertical_field_select);
         printf("vo_xvmc::slice: dct_type=%d,data_index=0x%x,cbp=%d,pad0=%d\n",
	         testblock->dct_type,testblock->index,testblock->coded_block_pattern,
	         testblock->pad0);
         printf("vo_xvmc::slice: PMV[0][0][0/1]=(%d,%d)\n",
	         testblock->PMV[0][0][0],testblock->PMV[0][0][1]);
       }
   }
#endif
   assert(rez==Success);
   if(verbose > 3 ) printf("vo_xvmc: flush surface\n");
   rez = XvMCFlushSurface(mDisplay, rndr->p_surface);
   assert(rez==Success);

//   rndr->start_mv_blocks_num += rndr->filled_mv_blocks_num;
   rndr->start_mv_blocks_num = 0;
   rndr->filled_mv_blocks_num = 0;

   rndr->next_free_data_block_num = 0;

   return VO_TRUE;
}

//XvMCHide hides the surface on next retrace, so
//check if the surface is not still displaying
static void check_osd_source(xvmc_render_state_t * src_rndr){
xvmc_render_state_t * osd_rndr;
int stat;
      //If this is source surface, check does the OSD rendering is compleate
      if(src_rndr->state & MP_XVMC_STATE_OSD_SOURCE){
         if(verbose > 3)
            printf("vo_xvmc: OSD surface=%p quering\n",src_rndr);
         osd_rndr = src_rndr->p_osd_target_surface_render;
         XvMCGetSurfaceStatus(mDisplay, osd_rndr->p_surface, &stat);
         if(!(stat & XVMC_RENDERING))
            src_rndr->state &= ~MP_XVMC_STATE_OSD_SOURCE;
      }
}
static int count_free_surfaces(){
int i,num;
   num=0;
   for(i=0; i<number_of_surfaces; i++){
      check_osd_source(&surface_render[i]);
      if(surface_render[i].state == 0)
        num++;
   }
   return num;
}

static xvmc_render_state_t * find_free_surface(){
int i,t;
int stat;
xvmc_render_state_t * visible_rndr;

   visible_rndr = NULL;
   for(i=0; i<number_of_surfaces; i++){

      check_osd_source(&surface_render[i]);
      if( surface_render[i].state == 0){
         XvMCGetSurfaceStatus(mDisplay, surface_render[i].p_surface,&stat);
         if( (stat & XVMC_DISPLAYING) == 0 ) 
            return &surface_render[i];
         visible_rndr = &surface_render[i];// remember it, use as last resort
      }
   }

   //all surfaces are busy, but there is one that will be free
   //on next monitor retrace, we just have to wait
   if(visible_rndr != NULL){       
      printf("vo_xvmc: waiting retrace\n");
      for(t=0;t<1000;t++){ 
         usec_sleep(1000);//1ms
         XvMCGetSurfaceStatus(mDisplay, visible_rndr->p_surface,&stat);
         if( (stat & XVMC_DISPLAYING) == 0 ) 
            return visible_rndr;
      }
   }
//todo remove when stable
   printf("vo_xvmc: no free surfaces, this should not happen in g1\n");
   for(i=0;i<number_of_surfaces;i++)
      printf("vo_xvmc: surface[%d].state=%d\n",i,surface_render[i].state);
   return NULL;
}

static uint32_t get_image(mp_image_t *mpi){
xvmc_render_state_t * rndr;

   rndr = find_free_surface();

   if(rndr == NULL){
      printf("vo_xvmc: get_image failed\n");
      return VO_FALSE;
   }

assert(rndr->start_mv_blocks_num == 0);
assert(rndr->filled_mv_blocks_num == 0);
assert(rndr->next_free_data_block_num == 0);

   mpi->flags |= MP_IMGFLAG_DIRECT;
//keep strides 0 to avoid field manipulations
   mpi->stride[0] = 0;
   mpi->stride[1] = 0;
   mpi->stride[2] = 0;

// these are shared!! so watch out
// do call RenderSurface before overwriting
   mpi->planes[0] = (char*)data_blocks.blocks;   
   mpi->planes[1] = (char*)mv_blocks.macro_blocks;
   mpi->priv =
   mpi->planes[2] = (char*)rndr;

   rndr->picture_structure = 0;
   rndr->flags = 0;
   rndr->state = 0;
   rndr->start_mv_blocks_num = 0;
   rndr->filled_mv_blocks_num = 0;
   rndr->next_free_data_block_num = 0;

   if( verbose > 3 ) 
      printf("vo_xvmc: get_image: rndr=%p (surface=%p) \n",
             rndr,rndr->p_surface);
return VO_TRUE;   
}

static uint32_t control(uint32_t request, void *data, ... )
{
   switch (request){
      case VOCTRL_QUERY_FORMAT:
         return query_format(*((uint32_t*)data));
      case VOCTRL_DRAW_IMAGE:
         return xvmc_draw_image((mp_image_t *)data);
      case VOCTRL_GET_IMAGE:
	 return get_image((mp_image_t *)data);
      //vo_xv
      case VOCTRL_GUISUPPORT:
         return VO_TRUE;
      case VOCTRL_FULLSCREEN:
         vo_x11_fullscreen();
      case VOCTRL_GET_PANSCAN:
         if ( !vo_config_count || !vo_fs ) return VO_FALSE;
         return VO_TRUE;
      // indended, fallthrough to update panscan on fullscreen/windowed switch
      case VOCTRL_SET_PANSCAN:
         if ( ( vo_fs && ( vo_panscan != vo_panscan_amount ) ) || ( !vo_fs && vo_panscan_amount ) )
         {
            int old_y = vo_panscan_y;
            panscan_calc();

            if(old_y != vo_panscan_y)
            {
               XClearWindow(mDisplay, vo_window);
               XFlush(mDisplay);
            }
         }
         return VO_TRUE;

      case VOCTRL_SET_EQUALIZER:
      {
      va_list ap;
      int value;

         va_start(ap, data);
         value = va_arg(ap, int);
         va_end(ap);

         return(vo_xv_set_eq(xv_port, data, value));
      }

      case VOCTRL_GET_EQUALIZER:
      {
      va_list ap;
      int *value;

         va_start(ap, data);
         value = va_arg(ap, int*);
         va_end(ap);

         return(vo_xv_get_eq(xv_port, data, value));
      }
   }
return VO_NOTIMPL;
}
