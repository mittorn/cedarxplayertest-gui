#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sunxi_disp_ioctl.h>
#include <X11/Xlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#ifndef CEDARXPLAYERTEST_COMMAND
#define CEDARXPLAYERTEST_COMMAND "/lib/ld-linux-armhf.so.3 --library-path . ./CedarXPlayerTest-1.4.1"
#endif
#ifndef FIFO_PATH
#define FIFO_PATH "./cedarxplayertest"
#endif
#ifndef PRELOAD_PATH
#define PRELOAD_PATH "./preload.so"
#endif
Display *dis;
Window win;
struct termios oldt, newt;
__disp_layer_info_t layer, layer0;
int disp, width, height, src_width=0, src_height=0, args[4]={0,100,(int)&layer0,0}, fs=0, keepaspect=1, handle=102;
uint32_t bgcolor=0x000102;
FILE * cpt;
char *fifo_path=FIFO_PATH;
void usage(char *binary)
{
	fprintf(stderr,"CedarXPlayerTest GUI\n"
	"	%s [options] <file>\n"
	"Availiable options:\n"
	"	--res <width>x<height>\n"
	"		Set window size and video aspect ratio to <width>x<height>.\n"
	"	--no-aspect\n"
	"		Don't keep video aspect ratio\n"
	"	--no-colorkey\n"
	"		Do not enable colorkey. Video will overlap all windows.\n"
	"	--no-rawinput\n"
	"		Do not put console to raw mode.\n"
	"	--crop [x<x>][y<y>][w<width>][h<height>]\n"
	"		Crop video to the given size.\n"
	"		Example: x10y10w630h470\n"
	"	--show-output\n"
	"		Do not redirect player output to /dev/null.\n"
	"	--bgcolor RRGGBB\n"
	"		Set window background and colorkey color.\n"
	"	--screen <n>\n"
	"		Specify disp screen number. Uses ioctl wraper to change it.\n"
	"	--fifo-path <path>\n"
	"		Override path to fifo file. default is " FIFO_PATH ".\n"
	"		Used to get layer handle from preloaded lib.\n"
	"\n",
	binary);
}

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

static void * x11thread()
{
	fprintf(stderr, "Creating %dx%d window\n", src_width, src_height);
	dis=XOpenDisplay(0);
	Atom XWMDeleteMessage = XInternAtom(dis, "WM_DELETE_WINDOW", False);
	XSetWindowAttributes  swa;
	memset(&swa, 0, sizeof(swa));
	swa.event_mask  =  ExposureMask | ButtonMotionMask | Button1MotionMask |
		ButtonPressMask | ButtonReleaseMask| StructureNotifyMask| KeyPressMask;
	Window win  =  XCreateWindow (dis, DefaultRootWindow(dis),0, 0, src_width,
		src_height, 0, CopyFromParent, InputOutput, CopyFromParent, 
		CWEventMask | CWOverrideRedirect | CWBorderPixel | CWBackPixel, &swa );
	XStoreName(dis,win,"CedarXPlayerTest");
	XSetWMProtocols(dis, win, &XWMDeleteMessage, 1);
	XSetWindowBackground(dis, win, bgcolor);
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
				if(keepaspect)
				{
					if((tmp=ev.xconfigure.width*src_height/src_width)<ev.xconfigure.height)height=tmp;
					else width=ev.xconfigure.height*src_width/src_height;
				}
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
	int c, esc=0;
	while(1)
	{
		c=getchar();
		switch(c)
		{
			case 0x1B:if(esc)esc=0,keyevent('q');else esc=1;break;
			case 0x5B:if(esc)esc=2;break;
			case 0x44:if(esc==2)esc=0, keyevent('j');break;
			case 0x43:if(esc==2)esc=0, keyevent('l');break;
			default:if(esc)esc=0;else keyevent(c);
		}
	}
	return 0;
}


int errorhandler(Display *dpy, XErrorEvent * error)
{
	fprintf(stderr, "\nX11 ERROR\n");
	fprintf(cpt,"q\nq\nq\n");
	fflush(cpt);
	pclose(cpt);
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	exit(2);
}

int (*XSetErrorHandler(int
(*handler)(Display *, XErrorEvent *)))();

int main(int argc, char **argv)
{
	XSetErrorHandler(errorhandler);
	pthread_t thread_id;
	__disp_rect_t crop;
	memset(&crop, 0, sizeof(crop));
	char *command=CEDARXPLAYERTEST_COMMAND;
	char *env=getenv("CEDARXPLAYERTEST");
	char *postfix=">/dev/null";
	if(env)command=env;
	int flag=1, count=0, argi=1, colorkey=1, raw=1;
	if(argc==1)
	{
		usage(argv[0]);
		return 1;
	}
	while(argi<argc-1)
	{
		if(!strcasecmp(argv[argi], "--res"))
		{
			argi++;
			if(sscanf(argv[argi],"%dx%d", &src_width, &src_height)!=2)
			{
				fprintf(stderr, "Cannot parse --res %s.\n", argv[argi]);
				usage(argv[0]);
				return 1;
			}
		}
		else
		if(!strcasecmp(argv[argi], "--no-aspect"))
		{
			keepaspect=0;
		}
		else
		if(!strcasecmp(argv[argi], "--no-colorkey"))
		{
			colorkey=0;
		}
		else
		if(!strcasecmp(argv[argi], "--crop"))
		{
			argi++;
			int i=0;
			do
			{
				i++;
				switch(*(argv[argi]+i-1))
				{
					case 'x':crop.x=atoi(argv[argi]+i);break;
					case 'y':crop.y=atoi(argv[argi]+i);break;
					case 'w':crop.width=atoi(argv[argi]+i);break;
					case 'h':crop.height=atoi(argv[argi]+i);break;
					default:if(*(argv[argi]+i-1)<'0'||*(argv[argi]+i-1)>'9')
					{
						fprintf(stderr,"Cannot parse --crop %s\n",argv[argi]);
						usage(argv[0]);
						return 1;
					}
				}
			}
			while(*(argv[argi]+i));
		}
		else
		if(!strcasecmp(argv[argi], "--no-rawinput"))
		{
			raw=0;
		}
		else
		if(!strcasecmp(argv[argi], "--show-output"))
		{
			postfix="";
		}
		else
		if(!strcasecmp(argv[argi],"--bgcolor"))
		{
			argi++;
			if(sscanf(argv[argi],"%x", &bgcolor)!=1)
			{
				fprintf(stderr, "Cannot parse --bgcolor %s\n", argv[argi]);
				usage(argv[0]);
				return 1;
			}
		}
		else
		if(!strcasecmp(argv[argi],"--screen"))
		{
			argi++;
			if(sscanf(argv[argi],"%d", &args[0])!=1)
			{
				fprintf(stderr, "Cannot parse --screen %s\n", argv[argi]);
				usage(argv[0]);
				return 1;
			}
		}
		else
		if(!strcasecmp(argv[argi],"--layer-handle")) // works only when failed to  create fifo
		{
			argi++;
			if(sscanf(argv[argi],"%d", &handle)!=1)
			{
				fprintf(stderr, "Cannot parse --layer-handle %s\n", argv[argi]);
				usage(argv[0]);
				return 1;
			}
		}
		else
		if(!strcasecmp(argv[argi],"--fifo-path"))
		{
			argi++;
			fifo_path=argv[argi];
		}
		else 
		{
			fprintf(stderr, "Cannot parse %s.\n", argv[argi]);
	 		usage(argv[0]);
			return 1;
		}
		argi++;
	}
	if(argi==argc)
	{
		fprintf(stderr, "No filename given\n");
		usage(argv[0]);
		return 1;
	}
	disp=open("/dev/disp",0);
	mkfifo(fifo_path, S_IRWXU);
	setenv("LD_PRELOAD", PRELOAD_PATH, 1);
	char *scnstr;
	asprintf(&scnstr,"%d",args[0]);
	setenv("CEDARXPLAYERTEST_SCREEN", scnstr, 1);
	setenv("CEDARXPLAYERTEST_FIFO", fifo_path, 1);
	if(raw)
	{
		tcgetattr( STDIN_FILENO, &oldt );
		newt = oldt;
		newt.c_lflag &= ~( ICANON | ECHO );
		tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	}
	char *s;
	ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
	if(layer0.mode!=DISP_LAYER_WORK_MODE_SCALER)layer0.src_win.width=layer0.src_win.height=layer0.scn_win.width=layer0.scn_win.height=1;
	if(asprintf(&s,"%s '%s' %s",command, argv[argc-1], postfix) < 0)return 1;
	cpt=popen(s,"w");
	int fifo=open(fifo_path, O_RDONLY);
	if(fifo>0) read(fifo, &handle, 1);
	printf("handle is %d\n", handle);
	while(flag&&(count<60))
	{
		count++;
		usleep(50000);
		args[1]=handle;
		args[2]=(int)&layer;
		ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
		layer.scn_win.width=layer.src_win.width;layer.scn_win.height=layer.src_win.height;
		if(layer.scn_win.width<4096&&layer.src_win.width<4096&&layer.scn_win.height<4096&&layer.src_win.height<4096&&layer.scn_win.width>32&&layer.src_win.width>32&&layer.scn_win.height>32&&layer.src_win.height>32&&flag)
		{
			flag=0;
			layer.ck_enable=colorkey;
			layer.pipe=1;
			if(crop.x)layer.src_win.x=crop.x;
			if(crop.y)layer.src_win.y=crop.y;
			if(crop.width)layer.src_win.width=crop.width;
			if(crop.height)layer.src_win.height=crop.height;
			if(!(src_width && src_height))src_width=layer.src_win.width,src_height=layer.src_win.height;
			if(colorkey)
			{
				__disp_colorkey_t ck;
				ck.ck_max.red = ck.ck_min.red = (uint8_t)(bgcolor>>16);
				ck.ck_max.green = ck.ck_min.green = (uint8_t)(bgcolor>>8);
				ck.ck_max.blue = ck.ck_min.blue = (uint8_t)(bgcolor);
				ck.red_match_rule = 2;
				ck.green_match_rule = 2;
				ck.blue_match_rule = 2;
				args[1] = (int)&ck;
				ioctl(disp, DISP_CMD_SET_COLORKEY, args);
			}
			args[1]=handle;
#if 0 //Show video on second screen. Very unstable and slow.
			ioctl(disp, DISP_CMD_LAYER_RELEASE, args);
			
			ioctl(disp, DISP_CMD_LAYER_CLOSE, args);
			args[0]=1;
			args[1]=DISP_LAYER_WORK_MODE_SCALER;
			layer.mode=DISP_LAYER_WORK_MODE_SCALER;
			args[2]=0;
			args[1] = ioctl(disp, DISP_CMD_LAYER_REQUEST, args);
			args[2]=&layer;
			ioctl(disp, DISP_CMD_LAYER_SET_PARA, args);

			ioctl(disp, DISP_CMD_LAYER_OPEN, args);
#endif
			if(colorkey)ioctl(disp, DISP_CMD_LAYER_BOTTOM, args);
			pthread_create(&thread_id, 0, &x11thread, 0);
			pthread_create(&thread_id, 0, &inputthread, 0);
		}
	}
	wait(0);
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	remove(fifo_path);
	return 0;
}
