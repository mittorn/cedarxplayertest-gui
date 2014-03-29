#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sunxi_disp_ioctl.h>
#include <X11/Xlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

Display *dis;
Window win;
__disp_layer_info_t layer, layer0;
int disp, width, height, args[4]={0,100,(int)&layer0,0}, fs=0;
FILE * cpt;
void keyevent(int k)
{
	char c=k;
	switch(k)
	{
		case 0x66:XFlush(dis);fs=!fs;break;
		case 0xFF51:c='j';break;
		case 0xFF53:c='l';break;
		case 0xFF1B:c='q';break;
	}
	fprintf(cpt,"%c",c);
	fprintf(stderr, "KeyEvent: %c 0x%X\n", c, k);
	fflush(cpt);
}
static void * eventthread()
{
	dis=XOpenDisplay(0);
	Atom XWMDeleteMessage = XInternAtom(dis, "WM_DELETE_WINDOW", False);
	XSetWindowAttributes  swa;
	swa.event_mask  =  ExposureMask | ButtonMotionMask | Button1MotionMask |
		ButtonPressMask | ButtonReleaseMask| StructureNotifyMask| KeyPressMask;
	Window win  =  XCreateWindow (dis, DefaultRootWindow(dis),0, 0, width ,
		height, 0, CopyFromParent, InputOutput, CopyFromParent, 
		CWEventMask | CWOverrideRedirect | CWBorderPixel | CWBackPixel, &swa );
	XStoreName(dis,win,"CedarXPlayerTest");
	XSetWMProtocols(dis, win, &XWMDeleteMessage, 1);
	XSetWindowBackground(dis, win, 0x000102);
	XMapWindow(dis,win);
	XFlush(dis);
	XEvent ev, cev;
	memset(&cev,0,sizeof(XEvent));
	cev.type = ClientMessage;
	cev.xclient.type = ClientMessage;
	cev.xclient.send_event = 1;
	cev.xclient.window = win;
	cev.xclient.message_type = XInternAtom( dis, "_NET_WM_STATE", 0);
	cev.xclient.format = 32;
	cev.xclient.data.l[0] = 0;
	cev.xclient.data.l[1] = XInternAtom( dis, "_NET_WM_STATE_FULLSCREEN", 0);
	int tmp;
	while(1)
	{
		XNextEvent(dis,&ev);
		switch(ev.type)
		{
			case ConfigureNotify:
				width=ev.xconfigure.width;
				height=ev.xconfigure.height;
				if((tmp=ev.xconfigure.width*layer.src_win.height/layer.src_win.width)<ev.xconfigure.height)height=tmp;
				else width=ev.xconfigure.height*layer.src_win.width/layer.src_win.height;
				layer.scn_win.x=(float)ev.xconfigure.x*(float)layer0.scn_win.width/(float)layer0.src_win.width-layer0.src_win.x+layer0.scn_win.x+(ev.xconfigure.width-width)/2;
				layer.scn_win.y=(float)ev.xconfigure.y*(float)layer0.scn_win.height/(float)layer0.src_win.height-layer0.src_win.y+layer0.scn_win.y+(ev.xconfigure.height-height)/2;
				layer.scn_win.width=((int)width)*layer0.scn_win.width/layer0.src_win.width;
				layer.scn_win.height=((int)height)*layer0.scn_win.height/layer0.src_win.height;
				ioctl(disp, DISP_CMD_LAYER_SET_PARA, args);
				break;
			case KeyPress:
				keyevent(XKeycodeToKeysym(dis, ev.xkey.keycode, 0));
				break;
			case ClientMessage:
				if(ev.xclient.data.l[0]== XWMDeleteMessage)keyevent('q');break;
			case ButtonPress:
				fprintf(stderr,"ButtonPress: %d\n",ev.xbutton.button);
				switch(ev.xbutton.button)
				{           
					case 1:keyevent(' ');break;
					case 4:keyevent('j');break;
					case 5:keyevent('l');break;
				}
		}
		cev.xclient.data.l[0]  = fs;
		XSendEvent( dis, DefaultRootWindow(dis), 0, SubstructureRedirectMask | SubstructureNotifyMask, &cev );
	}
	return 0;
}

static void *inputthread()
{
	while(1)
	{
		keyevent(getchar());
	}
	return 0;
}
int main(int argc, char **argv)
{
	pthread_t thread_id;
	int flag=1;
	disp=open("/dev/disp",0);
	char *s;
	ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
	if(layer0.mode!=DISP_LAYER_WORK_MODE_SCALER)layer0.src_win.width=layer0.src_win.height=layer0.scn_win.width=layer0.scn_win.height=1;
	if(asprintf(&s,"/lib/ld-linux-armhf.so.3 --library-path . ./CedarXPlayerTest-1.4.1 %s", argv[1]) < 0)return 1;
	cpt=popen(s,"w");
	while(flag)
	{
		usleep(50000);
		args[1]=102;
		args[2]=(int)&layer;
		if(flag)ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
		layer.scn_win.width=layer.src_win.width;layer.scn_win.height=layer.src_win.height;
		if(layer.scn_win.width<4096&&layer.src_win.width<4096&&layer.scn_win.height<4096&&layer.src_win.height<4096&&layer.scn_win.width>0&&layer.src_win.width>0&&layer.scn_win.height>0&&layer.src_win.height>0&&flag)
		{
			flag=0;
			layer.ck_enable=1;
			layer.pipe=1;
			width=layer.src_win.width,height=layer.src_win.height;
			__disp_colorkey_t ck;
			ck.ck_max.red = ck.ck_min.red = 0;
			ck.ck_max.green = ck.ck_min.green = 1;
			ck.ck_max.blue = ck.ck_min.blue = 2;
			ck.red_match_rule = 2;
			ck.green_match_rule = 2;
			ck.blue_match_rule = 2;
			args[1] = (int)&ck;
			ioctl(disp, DISP_CMD_SET_COLORKEY, args);
			args[1]=102;
			ioctl(disp, DISP_CMD_LAYER_BOTTOM, args);
			pthread_create(&thread_id, 0, &eventthread, 0);
			pthread_create(&thread_id, 0, &inputthread, 0);
		}
	}
	wait(0);
	return 0;
}
