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
#include <X11/Xft/Xft.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#ifndef CPT_BIN
#define CPT_BIN "CedarXPlayerTesr-1.4.1"
#endif
#ifndef LD_LINUX
#define LD_LINUX "/lib/ld-linux-armhf.so.3"
#endif
#ifndef CPT_PATH 
#define CPT_PATH "./"
#endif
Display *dis;
Window win;
GC gc;
XEvent cev;
struct termios oldt, newt;
__disp_layer_info_t layer, layer0;
int disp, width, height, winwidth, winheight, src_width=0, src_height=0, colorkey=1, args[4]={0,100,(int)&layer0,0}, fs=0, keepaspect=1, handle=102, ppause=0, showbuttons=1, pipefd, status=0, seek, hidetimer=20, bsize=50, pstatus=-1, linewrap=0;
long long ltime=-1, duration=0;
uint32_t bgcolor=0x000102;
pid_t pid=0;
XftFont *font;
XftColor xftcolor;
XftDraw *xftdraw;
XRenderColor xrcolor;
struct srt 
{
	long t1, t2;
	char *lines[5];
};
struct srt *subtitles=0;
int subcount=0;
void loadsrt(FILE * srtfile)
{
	char buffer[255];
	//iconv_t utf8_to_utf16 = iconv_open("UTF-16LE", "UTF-8");
	int i, h1, m1 ,s1, ms1, h2, m2, s2, ms2, srtlength=0, ret;
	do
	{
		ret=fscanf(srtfile,"%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n", &i, &h1, &m1, &s1, &ms1, &h2, &m2, &s2, &ms2);
		if(ret!=9)
		{
			if(fgetc(srtfile)!=EOF)ret=9;
			continue;
		}
		if(i>srtlength)
		{
			srtlength+=4;
			subtitles=(struct srt*)realloc(subtitles, srtlength*sizeof(struct srt));
		}
		subtitles[i-1].t1=h1*3600000+m1*60000+s1*1000+ms1;
		subtitles[i-1].t2=h2*3600000+m2*60000+s2*1000+ms2;
		int j=0;
		for(fgets(buffer, 255,srtfile);buffer[0]&&buffer[0]!='\r'&&buffer[0]!='\n';j++,fgets(buffer, 255, srtfile)) asprintf(&subtitles[i-1].lines[j], "%s", buffer);
		if(i==1)linewrap=strlen(buffer);
	}
	while(ret==9);
	subcount=i;
}
char ** getsublines()
{
	int i;
	for(i=0;i<subcount;i++)
	{
		if(subtitles[i].t1<ltime&&subtitles[i].t2>ltime)return subtitles[i].lines;
	}
	return 0;
}
void toggle_fullscreen()
{
	fs=!fs;
	cev.xclient.data.l[0]  = fs;
	XSendEvent( dis, DefaultRootWindow(dis), 0, SubstructureRedirectMask | SubstructureNotifyMask, &cev );
	XFlush(dis);
}
#define marign 10
#define BRECT(bw); 	XDrawLine(dis, win, gc, marign + bw, winheight - marign - bsize, marign + bw, winheight - marign);\
					XDrawLine(dis, win, gc, marign + bw, winheight - marign - bsize, marign + bw + bsize, winheight - marign - bsize);\
					XDrawLine(dis, win, gc, marign + bsize + bw, winheight - marign, marign + bw, winheight - marign);\
					XDrawLine(dis, win, gc, marign + bsize + bw, winheight - marign - bsize, marign + bsize + bw, winheight - marign);
#define TRIANGLE(a, b,c,bw); 	XDrawLine(dis, win, gc, marign + bw + b, winheight - marign -bsize / 2 - a, marign + bw + b, winheight - marign - bsize / 2 + a);\
								XDrawLine(dis, win, gc, marign + bw + b, winheight - marign -bsize / 2 - a, marign + bw + c, winheight - marign - bsize / 2);\
								XDrawLine(dis, win, gc, marign + bw + b, winheight - marign -bsize / 2 + a, marign + bw + c, winheight - marign - bsize / 2);
void draw_buttons()
{
	if(showbuttons)
	{
		XSetLineAttributes(dis, gc, 4, LineSolid, CapRound, JoinRound);
		XSetForeground(dis, gc, 0x000000);
		if(pstatus!=status)
		{
			XClearArea(dis, win, 0, winheight-marign-bsize, bsize+marign, bsize+marign*2,0);
			BRECT(0);
			if(ppause)
			{
				TRIANGLE(bsize / 4, bsize / 4, bsize * 3 / 4, 0);
			}
			else
			{
				XDrawLine(dis, win, gc, marign + bsize / 3, winheight-marign-bsize / 4, marign + bsize / 3, winheight - marign - bsize * 3 / 4);
				XDrawLine(dis, win, gc, marign + bsize * 2 / 3, winheight-marign-bsize / 4, marign + bsize * 2 / 3, winheight-marign-bsize * 3 / 4); 
			}
			BRECT(bsize+marign);
			TRIANGLE(bsize / 6, bsize * 3 / 4, bsize / 4, bsize + marign);
			XDrawLine(dis, win, gc, bsize + marign * 2 + bsize / 4, winheight - marign - bsize / 3, bsize + marign * 2 + bsize / 4, winheight - marign - bsize * 2 / 3);
			BRECT((bsize + marign) * 2);
			TRIANGLE(bsize / 6, bsize / 2, bsize / 4, (bsize + marign) * 2);
			TRIANGLE(bsize / 6, bsize * 3 / 4, bsize / 2, (bsize + marign) * 2);
			BRECT((bsize + marign) * 3);
			TRIANGLE(bsize / 6, bsize / 4, bsize / 2, (bsize + marign) * 3);
			TRIANGLE(bsize / 6, bsize / 2, bsize * 3 / 4, (bsize + marign) * 3);
			BRECT((bsize+marign) * 4);
			TRIANGLE(bsize / 6, bsize / 6, bsize / 3, (bsize + marign) * 4);
			TRIANGLE(bsize / 6, bsize / 3, bsize / 2, (bsize + marign) * 4);
			TRIANGLE(bsize / 6, bsize / 2, bsize * 5 / 6, (bsize + marign) * 4);
			BRECT((bsize+marign) * 5);
			TRIANGLE(bsize / 6, bsize / 4, bsize * 3 / 4, (bsize + marign) * 5);
			XDrawLine(dis, win, gc, bsize * 5 + marign * 6 + bsize * 3 / 4, winheight - marign - bsize / 3, bsize * 5 + marign * 6 + bsize * 3 / 4, winheight - marign - bsize * 2 / 3);
		}
		if(duration>0)
		{
			XClearArea(dis, win, 0, winheight-marign-bsize-marign*3-marign/2, winwidth, marign*3,0);
			XDrawLine(dis, win, gc, 0, winheight-bsize-marign*3, winwidth*ltime/duration,  winheight-bsize-marign*3);
			XDrawLine(dis, win, gc, winwidth*ltime/duration+marign*2, winheight-bsize-marign*3, winwidth,  winheight-bsize-marign*3);
			XDrawLine(dis, win, gc, winwidth*ltime/duration, winheight-bsize-marign*2, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*2);
			XDrawLine(dis, win, gc, winwidth*ltime/duration, winheight-bsize-marign*4, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*4);
			XDrawLine(dis, win, gc, winwidth*ltime/duration, winheight-bsize-marign*2, winwidth*ltime/duration, winheight-bsize-marign*4);
			XDrawLine(dis, win, gc, marign*2+winwidth*ltime/duration, winheight-bsize-marign*4, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*2);
		}
		XSetLineAttributes(dis, gc, 2, LineSolid, CapRound, JoinRound);
		if(pstatus!=status)
		{
			if(status==5)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT(0);
			if(ppause)
			{
				TRIANGLE(bsize / 4, bsize / 4, bsize * 3 / 4, 0);
			}
			else
			{
				XDrawLine(dis, win, gc, marign + bsize / 3, winheight-marign-bsize / 4, marign + bsize / 3, winheight - marign - bsize * 3 / 4);
				XDrawLine(dis, win, gc, marign + bsize * 2 / 3, winheight-marign-bsize / 4, marign + bsize * 2 / 3, winheight-marign-bsize * 3 / 4); 
			}
			if(status==6)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT(bsize+marign);
			TRIANGLE(bsize / 6, bsize * 3 / 4, bsize / 4, bsize + marign);
			XDrawLine(dis, win, gc, bsize + marign * 2 + bsize / 4, winheight - marign - bsize / 3, bsize + marign * 2 + bsize / 4, winheight - marign - bsize * 2 / 3);
			if(status==3)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT((bsize + marign) * 2);
			TRIANGLE(bsize / 6, bsize / 2, bsize / 4, (bsize + marign) * 2);
			TRIANGLE(bsize / 6, bsize * 3 / 4, bsize / 2, (bsize + marign) * 2);
			if(status==2)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT((bsize + marign) * 3);
			TRIANGLE(bsize / 6, bsize / 4, bsize / 2, (bsize + marign) * 3);
			TRIANGLE(bsize / 6, bsize / 2, bsize * 3 / 4, (bsize + marign) * 3);
			if(status==4)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT((bsize+marign) * 4);
			TRIANGLE(bsize / 6, bsize / 6, bsize / 3, (bsize + marign) * 4);
			TRIANGLE(bsize / 6, bsize / 3, bsize / 2, (bsize + marign) * 4);
			TRIANGLE(bsize / 6, bsize / 2, bsize * 5 / 6, (bsize + marign) * 4);
			if(status==7)XSetForeground(dis, gc, 0xFFFF00);
			else XSetForeground(dis, gc, 0xFFFFFF);
			BRECT((bsize+marign) * 5);
			TRIANGLE(bsize / 6, bsize / 4, bsize * 3 / 4, (bsize + marign) * 5);
			XDrawLine(dis, win, gc, bsize * 5 + marign * 6 + bsize * 3 / 4, winheight - marign - bsize / 3, bsize * 5 + marign * 6 + bsize * 3 / 4, winheight - marign - bsize * 2 / 3);
			pstatus=status;
		}
		XSetForeground(dis, gc, 0xFFFFFF);
		if(duration>0)
		{
			char timestr[9];
			sprintf(timestr, "%02d:%02d:%02d",(int)ltime/3600000%100, (int)(ltime/60000)%60, (int)(ltime/1000)%60);
			XDrawString(dis, win, gc, 0, winheight-bsize-marign*3-marign/2, timestr, 8);
			sprintf(timestr, "%02d:%02d:%02d",(int)duration/3600000%100, (int)(duration/60000)%60, (int)(duration/1000)%60);
			XDrawString(dis, win, gc, winwidth-50, winheight-bsize-marign*3-marign/2, timestr, 8);
			if(status==8)XSetForeground(dis, gc, 0xFFFF00);
			XDrawLine(dis, win, gc, 0, winheight-bsize-marign*3, winwidth*ltime/duration,  winheight-bsize-marign*3);
			XDrawLine(dis, win, gc, winwidth*ltime/duration+marign*2, winheight-bsize-marign*3, winwidth,  winheight-bsize-marign*3);
			if(status==1)XSetForeground(dis, gc, 0xFFFF00);
			XDrawLine(dis, win, gc, winwidth * ltime / duration, winheight-bsize-marign*2, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*2);
			XDrawLine(dis, win, gc, winwidth * ltime / duration, winheight-bsize-marign*4, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*4);
			XDrawLine(dis, win, gc, winwidth * ltime / duration, winheight-bsize-marign*2, winwidth*ltime/duration, winheight-bsize-marign*4);
			XDrawLine(dis, win, gc, marign * 2 + winwidth * ltime / duration, winheight-bsize-marign*4, marign*2 + winwidth*ltime/duration, winheight-bsize-marign*2);
		}
	}
	else pstatus=-1,XClearWindow(dis, win);
	if(subcount>0)
	{
		int i, lastheight=0;
		XGlyphInfo extents;
		XClearArea(dis, win, 0, winheight-bsize-marign*5-50, winwidth, 50, 0);
		char ** lines=getsublines();
		for(i=0;i<4;i++)if(lines&&lines[i])
		{
			XftTextExtentsUtf8 (dis, font, lines[i], strlen(lines[i])-linewrap, &extents);
			XftDrawStringUtf8(xftdraw, &xftcolor,font, winwidth/2-extents.width/2, winheight-marign*4-bsize-50+lastheight, lines[i], strlen(lines[i])-linewrap);
			lastheight+=extents.height;
		}
	}
	XFlush(dis);
}
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
	"	--srt <path>\n"
	"		Load srt file. Experimental, may crash. Only 8-bit encoding.\n"
	"\n",
	binary);
}

void keyevent(int k)
{
	char c=k;
	switch(k)
	{
		case 0x68:showbuttons=!showbuttons,hidetimer=20;draw_buttons();break; 
		case 0x66:toggle_fullscreen();break;
		case 0xFF51:c='j';break;
		case 0xFF53:c='l';break;
		case 0xFF1B:c='q';break;
		case 0x20: ppause=!ppause; draw_buttons();break;
	}
	fprintf(stderr, "KeyEvent: %c 0x%X\n", c, k);
	write(pipefd, &c, 1);
}

static void * x11thread()
{
	fprintf(stderr, "Creating %dx%d window\n", src_width, src_height);
	dis=XOpenDisplay(0);
	XInitThreads();
			int count;
		char *defstring, **missinglist;
	font = XftFontOpenName(dis, 0, "");
	Atom XWMDeleteMessage = XInternAtom(dis, "WM_DELETE_WINDOW", False);
	XSetWindowAttributes  swa;
	memset(&swa, 0, sizeof(swa));
	swa.event_mask  =  ExposureMask | ButtonMotionMask | Button1MotionMask |
		ButtonPressMask | ButtonReleaseMask| StructureNotifyMask| KeyPressMask;
	win = XCreateWindow(dis, DefaultRootWindow(dis),0, 0, src_width,
		src_height, 0, CopyFromParent, InputOutput, CopyFromParent, 
		CWEventMask | CWOverrideRedirect | CWBorderPixel | CWBackPixel, &swa );
	gc = XCreateGC(dis, win, 0, NULL);
	XSetForeground(dis, gc, 0xFFFFFF);
	XStoreName(dis,win,"CedarXPlayerTest");
	XSetWMProtocols(dis, win, &XWMDeleteMessage, 1);
	XSetWindowBackground(dis, win, bgcolor);
	XMapWindow(dis,win);
	xftdraw = XftDrawCreate(dis, win, DefaultVisual(dis, 0), DefaultColormap(dis, 0));
	xrcolor.red  =65535;
	xrcolor.green=65535;
	xrcolor.blue =65535;
	xrcolor.alpha=65535;
	XftColorAllocValue(dis, DefaultVisual(dis ,0), DefaultColormap(dis, 0), &xrcolor, &xftcolor);
	XFlush(dis);
	XEvent ev;
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
	Time last=0;
	while(1)
	{
		XNextEvent(dis,&ev);
		switch(ev.type)
		{
			case Expose:
				pstatus=-1;
				draw_buttons();
				break;
			case ConfigureNotify:
				hidetimer=20, pstatus=-1;
				XClearWindow(dis, win);
				winwidth=width=ev.xconfigure.width;
				winheight=height=ev.xconfigure.height;
				if(!colorkey&&showbuttons)height-=bsize+marign*5;
				if(keepaspect)
				{
					if((tmp=ev.xconfigure.width*src_height/src_width)<height)height=tmp;
					else width=height*src_width/src_height;
				}
				if(handle!=255)
				{
					layer.scn_win.x=((float)ev.xconfigure.x+(ev.xconfigure.width-width)/2)*(float)layer0.scn_win.width/(float)layer0.src_win.width-layer0.src_win.x+layer0.scn_win.x;
					layer.scn_win.y=((float)ev.xconfigure.y+(ev.xconfigure.height-height-((!colorkey&&showbuttons)?(bsize+marign*4):0))/2)*(float)layer0.scn_win.height/(float)layer0.src_win.height-layer0.src_win.y+layer0.scn_win.y;
					layer.scn_win.width=((int)width)*layer0.scn_win.width/layer0.src_win.width;
					layer.scn_win.height=((int)height)*layer0.scn_win.height/layer0.src_win.height;
					ioctl(disp, DISP_CMD_LAYER_SET_PARA, args);
				}
				if(300+marign*7>winwidth)bsize=(winwidth-marign*7)/6;else bsize=50;
				if(bsize<15)bsize=15;
				draw_buttons();
				break;
			case KeyPress:hidetimer=20;
				keyevent(XKeycodeToKeysym(dis, ev.xkey.keycode, 0));
				break;
			case ClientMessage:
				if(ev.xclient.data.l[0]== XWMDeleteMessage)keyevent('q');break;
			case ButtonPress:
				fprintf(stderr,"ButtonPress: %d\n",ev.xbutton.button);
				if(!showbuttons)
				{
					if(ev.xbutton.y<height-bsize-marign*4)
					{
						switch(ev.xbutton.button)
						{
							case 1:if(ev.xbutton.time-last<200&&ev.xbutton.time-last>0)toggle_fullscreen();
								else last=ev.xbutton.time;
								keyevent(' ');break;
							case 4:keyevent('j');break;
							case 5:keyevent('l');break;
						}
					}
					else showbuttons=1, hidetimer=20;
				}
				else
				{
					if(ev.xbutton.y<height-bsize-marign*4&&handle!=255)showbuttons=0;
					else
					{
						if(ev.xbutton.y<height-bsize-marign*2)
						{
							seek=ev.xbutton.x*duration/winwidth;
							if(ev.xbutton.time-last<200&&ev.xbutton.time-last>0)status=8;
							else last=ev.xbutton.time, status=1;
						}
						else
						{
							if(ev.xbutton.x<bsize+marign)status=5,hidetimer=20,keyevent(' ');
							else if(ev.xbutton.x<(bsize+marign)*2)status=6,hidetimer=20,keyevent('d');
							else if(ev.xbutton.x<(bsize+marign)*3)status=3,hidetimer=20,keyevent('j');
							else if(ev.xbutton.x<(bsize+marign)*4)status=2,hidetimer=20,keyevent('l');
							else if(ev.xbutton.x<(bsize+marign)*5)status=4,hidetimer=20,keyevent('i');
							else if(ev.xbutton.x<(bsize+marign)*6)status=7,hidetimer=20,keyevent('n');
						}
					}
				}
				draw_buttons();
				break;
			case ButtonRelease:
				if(status!=8)status=0;
				draw_buttons();
				break;
			case MotionNotify:
				seek=ev.xmotion.x*duration/winwidth, hidetimer=20;
				break;
		}
	}
	return 0;
}

static void *inputthread()
{
	int c, esc=0;
	do
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
	while(c>0);
	return 0;
}


int errorhandler(Display *dpy, XErrorEvent * error)
{
	fprintf(stderr, "\nX11 ERROR\n");
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
	int flag=1, count=0, argi=1, raw=1, showoutput=0;
	char *cpt_path=CPT_PATH, *cpt_bin=CPT_BIN, *cpt_preload, *ld_linux=LD_LINUX;
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
			showoutput=1;
		}
		else
		if(!strcasecmp(argv[argi], "--bgcolor"))
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
		if(!strcasecmp(argv[argi], "--screen"))
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
		if(!strcasecmp(argv[argi], "--layer-handle")) // works only when failed to  create fifo
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
		if(!strcasecmp(argv[argi], "--srt"))
		{
			argi++;
			loadsrt(fopen(argv[argi],"r"));
		}
		else
		if(!strcasecmp(argv[argi], "--help"))
		{
			usage(argv[0]);
			return 0;
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
	char *env;
	if((env=getenv("CPT_PATH")))cpt_path=env;
	if((env=getenv("LD_LINUX")))ld_linux=env;
	if((env=getenv("CPT_BIN")))cpt_bin=env;
	env=getenv("LD_PRELOAD");
	if(!env)env="";
	asprintf(&cpt_bin, "%s%s", cpt_path, cpt_bin);
	asprintf(&cpt_preload,(*env)?"%s:%s%s":"%s%s%s",env, cpt_path, "preload.so");
	disp=open("/dev/disp",0);
	setenv("LD_PRELOAD", cpt_preload, 1);
	char *scnstr=getenv("CPT_SCREEN");
	if(scnstr)args[0]=atoi(scnstr);
	else
	{
		asprintf(&scnstr,"%d",args[0]);
		setenv("CPT_SCREEN", scnstr, 1);
	}
	char *s;
	ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
	if(layer0.mode!=DISP_LAYER_WORK_MODE_SCALER)layer0.src_win.width=layer0.src_win.height=layer0.scn_win.width=layer0.scn_win.height=1;
	int pipefd1[2], pipefd2[2];
	pipe(pipefd1);
	pipe(pipefd2);
	pid=fork();
	if(!pid)
	{
		if(!showoutput)close(STDOUT_FILENO);
		close(pipefd1[1]);
		close(pipefd2[0]);
		dup2(pipefd1[0], 0);
		dup2(pipefd2[1], 100);
		execl(ld_linux, ld_linux, "--library-path", cpt_path, cpt_bin, argv[argc-1], (char*)0);
		exit(1);
	}
	close(pipefd1[0]);
	close(pipefd2[1]);
	pipefd=pipefd1[1];
	if(raw)
	{
		tcgetattr( STDIN_FILENO, &oldt );
		newt = oldt;
		newt.c_lflag &= ~( ICANON | ECHO );
		tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	}
	read(pipefd2[0], &handle, 1);
	printf("handle is %d\n", handle);
	if(handle!=255)
	{
		read(pipefd2[0],&args[1],4);
		if(args[1]!=1)exit(1);
		args[1]=handle;
		args[2]=(int)&layer;
		ioctl(disp,DISP_CMD_LAYER_GET_PARA,args);
		layer.scn_win.width=layer.src_win.width;layer.scn_win.height=layer.src_win.height;
		{
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
		}
	}
	else src_width=300+marign*7, src_height=marign*5+50;
	//if(layer.scn_win.width<4096&&layer.src_win.width<4096&&layer.scn_win.height<4096&&layer.src_win.height<4096&&layer.scn_win.width>32&&layer.src_win.width>32&&layer.scn_win.height>32&&layer.src_win.height>32)
	pthread_create(&thread_id, 0, &x11thread, 0);
	pthread_create(&thread_id, 0, &inputthread, 0);
	XInitThreads();
	long long tmptime;
	while(read(pipefd2[0], &tmptime, 8)>0)
	{
		//fprintf(stderr, "duration %lld ltime %lld\n", duration, ltime);
		
		if(tmptime==-1) read(pipefd2[0], &duration, 8);
		if(tmptime>0)ltime=tmptime;
		if(duration>0)
		{
			if(status==0&&handle!=255)hidetimer--;
			else hidetimer=20;
			if(status==1||status==8)
			{
				if(seek>ltime&&handle!=255&&(duration-seek)*2<seek-ltime)keyevent('n');
				else if(seek<ltime&&handle!=255&&ltime-seek>seek)ltime=0, keyevent('d');
				else if((seek>ltime&&seek-ltime>=60000))keyevent('i');
				else if((seek<ltime&&ltime-seek>=60000))keyevent('k');
				else if((seek>ltime&&seek-ltime>=5000))keyevent('l');
				else if((seek<ltime&&seek-ltime<=-5000))keyevent('j');
				else if(status==8)status=0;
			}
			else if(status==2)keyevent('l');
			else if(status==3)keyevent('j');
			else if(status==4)keyevent('i');
			if(!hidetimer)showbuttons=0;
			draw_buttons();
		}
		else if(duration==0)keyevent('k');
	}
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return 0;
}
