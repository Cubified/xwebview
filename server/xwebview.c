/*
 * xwebserver.c: X server viewer for the browser
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>

#include <lz4frame.h>

#include "wsServer/include/ws.h"
#include "fsserver.h"
#include "libspool.h"

//////////////////////////////
// PREPROCESSOR
//
#define LEN (img->width*img->height*4)
#define MAX_CONNECTIONS 8

//////////////////////////////
// ENUMS AND TYPEDEFS
//
enum {
  MOUSEDOWN,
  MOUSEUP,
  KEYDOWN,
  KEYUP
};

//////////////////////////////
// GLOBAL VARIABLES
//
Display *dpy;
XRectangle *sharedarea;
pool *connpool;
pid_t main_pid,
      loop_pid,
      http_pid;

//////////////////////////////
// PIXEL DATA HANDLER
//
void send_frame(){
  char prep[64];
  uint8_t *comp;
  size_t sze_max,
         sze_header,
         sze_body,
         sze_end,
         sze_total;
  int ind, fd;
  LZ4F_compressionContext_t ctx;
  XImage *img;
  XRectangle cpyarea;

  if(pool_empty(connpool)) return;

  memcpy(&cpyarea, sharedarea, sizeof(XRectangle));
  img = XGetImage(
    dpy,
    DefaultRootWindow(dpy),
    sharedarea->x, sharedarea->y,
    sharedarea->width, sharedarea->height,
    AllPlanes,
    ZPixmap
  );
  sze_max = LZ4F_compressBound(LEN, NULL);
  comp = malloc(sze_max);

  if(LZ4F_isError(LZ4F_createCompressionContext(&ctx, LZ4F_VERSION))){
    printf("[wsserver] Failed to creation compression context.\n");
    goto shutdown;
  }

  sze_header = LZ4F_compressBegin(ctx, comp, sze_max, NULL);
  if(sze_header <= 0){
    printf("[wsserver] Failed to start compression.\n");
    goto shutdown;
  }

  sze_body = LZ4F_compressUpdate(ctx, comp+sze_header, sze_max, img->data, LEN, NULL);
  if(sze_body <= 0){
    printf("[wsserver] Failed to compress.\n");
    goto shutdown;
  }

  sze_end = LZ4F_compressEnd(ctx, comp+sze_header+sze_body, sze_max, NULL);
  sze_total = sze_header+sze_body+sze_end;

  pool_foreach_nodecl(connpool){
    fd = (int)pool_get(ind, connpool);
    sprintf(prep, "GETREADY:{\"x\":%i,\"y\":%i,\"w\":%i,\"h\":%i,\"length\":%li}", cpyarea.x, cpyarea.y, cpyarea.width, cpyarea.height, sze_total);
    ws_sendframe_txt(fd, prep, 0);
    ws_sendframe_bin(fd, comp, sze_total, 0);
  }

shutdown:;
  XDestroyImage(img);
  free(comp);
  LZ4F_freeCompressionContext(ctx);
}

//////////////////////////////
// SIGNAL HANDLING
//
void sighandler(int sig){
  if(sig == SIGUSR1){
    send_frame();
    return;
  }

  if(http_pid > 0 &&
     loop_pid > 0){
    printf("[xwebview] Shutting down.\n");
    kill(loop_pid, sig);
    kill(http_pid, SIGKILL); /* Mild hack */
    pool_free(connpool);
    XCloseDisplay(dpy);
    munmap(sharedarea, sizeof(XRectangle));
  }
  exit(0);
}

//////////////////////////////
// WEBSOCKET FUNCTIONS
//
void onopen(int fd){
  char res[64];
  int i;
  pid_t pid;
  XRRScreenResources *scr_res;
  XRRCrtcInfo *crtc;

  printf("[wsserver] New connection established.\n");

  scr_res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
  for(i=0;i<scr_res->ncrtc;i++){
    crtc = XRRGetCrtcInfo(dpy, scr_res, scr_res->crtcs[i]);
    if(crtc->width > 0){
      sprintf(res, "MONITOR:{\"x\":%i,\"y\":%i,\"w\":%i,\"h\":%i}", crtc->x, crtc->y, crtc->width, crtc->height);
      ws_sendframe_txt(fd, res, 0);
    }
    XRRFreeCrtcInfo(crtc);
  }
  XRRFreeScreenResources(scr_res);

  pool_push((void*)fd, connpool);

  sharedarea->x = 0;
  sharedarea->y = 0;
  sharedarea->width = WidthOfScreen(DefaultScreenOfDisplay(dpy));
  sharedarea->height = HeightOfScreen(DefaultScreenOfDisplay(dpy));
  kill(main_pid, SIGUSR1);
}

void onclose(int fd){
  int ind = pool_find((void*)fd, connpool);
  if(ind > -1){
    pool_pop(ind, connpool);
  }
  printf("[wsserver] Connection closed.\n");
}

void onmessage(int fd, const unsigned char *msg, size_t size, int type){
  char cmd[512];
  int t, b, x, y;
  char str[16];
  char k;
  sscanf(msg, "%i:%i:%i:%i:%s", &t, &b, &x, &y, str);
  k = str[0];

  switch(t){
    case MOUSEDOWN:
      sprintf(cmd, "xdotool mousemove %i %i mousedown %i", x, y, b);
      break;
    case MOUSEUP:
      sprintf(cmd, "xdotool mousemove %i %i mouseup %i", x, y, b);
      break;
    case KEYDOWN:
      if((k >= '!' && k <= '@') || (k >= '[' && k <= '`') || k >= '{'){
        sprintf(cmd, "xdotool keydown 0x%.4x", k);
      } else if(k == ' '){
        sprintf(cmd, "xdotool keydown space", k);
      } else if(strlen(str) > 1){
        sprintf(cmd, "xdotool keydown %s", str);
      } else {
        sprintf(cmd, "xdotool keydown %c", k);
      }
      break;
    case KEYUP:
      if((k >= '!' && k <= '@') || (k >= '[' && k <= '`') || k >= '{'){
        sprintf(cmd, "xdotool keyup 0x%.4x", k);
      } else if(k == ' '){
        sprintf(cmd, "xdotool keyup space", k);
      } else if(strlen(str) > 1){
        sprintf(cmd, "xdotool keyup %s", str);
      } else {
        sprintf(cmd, "xdotool keyup %c", k);
      }
      break;
  }
#ifdef IS_DEBUG
  printf("[wsserver] exec: %s\n", cmd);
#endif
  system(cmd);
}

//////////////////////////////
// X DAMAGE EVENTS
//
void damage_loop(){
  int i, ind, count;
  int dmg_evt, dmg_err;
  Display *dpy_frk;
  Damage root_dmg;
  XEvent evt;
  XserverRegion region;
  XRectangle *area;

  dpy_frk = XOpenDisplay(NULL);
  if(dpy_frk == NULL){
    printf("\x1b[31mFailed to open display.\x1b[0m\n");
    exit(1);
  }

  XDamageQueryExtension(dpy_frk, &dmg_evt, &dmg_err);
  root_dmg = XDamageCreate(
    dpy_frk,
    DefaultRootWindow(dpy_frk),
    XDamageReportNonEmpty
  );

  while(1){
    XNextEvent(dpy_frk, &evt);
    if(evt.type == dmg_evt+XDamageNotify){
      region = XFixesCreateRegion(dpy_frk, NULL, 0);
      XDamageSubtract(
        dpy_frk,
        root_dmg,
        None,
        region
      );
      area = XFixesFetchRegion(dpy_frk, region, &count);
      if(area){
        for(i=0;i<count;i++){
#ifdef IS_DEBUG
          printf("[wsserver] area: (%i, %i) %ix%i\n", area[i].x, area[i].y, area[i].width, area[i].height);
#endif
          memcpy(sharedarea, &area[i], sizeof(XRectangle));
          kill(main_pid, SIGUSR1);
        }
      }
      XFixesDestroyRegion(dpy_frk, region);
      XFree(area);
    }
  }
}

//////////////////////////////
// MAIN
//
int main(int argc, char **argv){
  struct ws_events evs;
  evs.onopen    = &onopen;
  evs.onclose   = &onclose;
  evs.onmessage = &onmessage;
 
  signal(SIGINT,  sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGQUIT, sighandler);
  signal(SIGUSR1, sighandler);

  printf("[xwebview] Starting up.\n");

  XInitThreads();

  dpy = XOpenDisplay(NULL);
  if(dpy == NULL){
    printf("\x1b[31mFailed to open display.\x1b[0m\n[xwebview] Shutting down.\n");
    return 1;
  }
  sharedarea = mmap(NULL, sizeof(XRectangle), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);

  connpool = pool_init(MAX_CONNECTIONS);

  main_pid = getpid();
  if((http_pid=fork()) == 0){
    fsserver();
  } else if((loop_pid=fork()) == 0){
    damage_loop();
  } else {
    printf("[wsserver] ");
    ws_socket(&evs, 8080);
  }

  /* Unreachable -- see sighandler() for cleanup */

  return 0;
}
