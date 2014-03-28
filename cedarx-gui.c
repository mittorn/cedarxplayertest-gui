#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sunxi_disp_ioctl.h>
#include <X11/Xlib.h>
__disp_layer_info_t layer;
int disp, width, height, args[4]={0,102,&layer,0};;
FILE * pipe;
void keyevent(char k)
{
	fprintf(pipe,"%c",k);
	fflush(pipe);
}
static void * eventthread()
{
	Display *dis=XOpenDisplay(0);
	XSetWindowAttributes  swa;
	swa.event_mask  =  ExposureMask | ButtonMotionMask | Button1MotionMask |
		ButtonPressMask | ButtonReleaseMask| StructureNotifyMask| KeyPressMask;
	Window win  =  XCreateWindow (dis, DefaultRootWindow(dis),0, 0, width ,
		height, 0, CopyFromParent, InputOutput, CopyFromParent, 
		CWEventMask, &swa );
	XMapWindow(dis,win);
	XFlush(dis);
	XEvent ev;
	while(1)
	{
		XNextEvent(dis,&ev);
		switch(ev.type)
		{
			case ConfigureNotify:
				layer.scn_win.x=ev.xconfigure.x;
				layer.scn_win.y=ev.xconfigure.y;
				layer.scn_win.width=ev.xconfigure.width;
				layer.scn_win.height=ev.xconfigure.height;
				ioctl(disp,DISP_CMD_LAYER_SET_PARA,args);
				break;
			case KeyPress:
				keyevent(tolower(XKeycodeToKeysym(dis, ev.xkey.keycode,0)));
				break;
			//default: //printf("0x%X\n",ev.xkey.keycode);
		}
	}
}
static void *inputthread()
{
	while(1)
	{
		keyevent(getchar());
	}
}
int main(int argc, char **argv)
{
	pthread_t thread_id;
	__disp_fb_t fb;
	int flag=1;
	disp=open("/dev/disp",0);
	char *s;
	asprintf(&s,"/lib/ld-linux-armhf.so.3 --library-path . ./CedarXPlayerTest-1.4.1 %s", argv[1]);
	pipe=popen(s,"w");
	while(flag)
	{
		usleep(100000);
		if(flag)ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
		//printf("\nlayer %d %d %d %d\n", hdl,ret, layer.scn_win.width, layer.scn_win.height);
		layer.scn_win.width=layer.src_win.width;layer.scn_win.height=layer.src_win.height;
		if(layer.scn_win.width&&layer.src_win.width&&flag)
		{
			flag=0;
			width=layer.src_win.width,height=layer.src_win.height;
			ioctl(disp,DISP_CMD_LAYER_SET_PARA,args);
			pthread_create(&thread_id, 0, &eventthread, 0);
			pthread_create(&thread_id, 0, &inputthread, 0);
		}
	}
	wait(0);
}
