#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
static int (*next_ioctl)(int fd, int request, void *data) = NULL;
int disp_fd, screen=-1, duration, skip_printf=0;
long long ltime=0, tmp;
int  pthread_attr_setschedpolicy()
{
	return 0;
}

int ioctl(int fd, int request, void *data)
{
	char *msg;

	if (next_ioctl == NULL) {
		fprintf(stderr, "ioctl : wrapping ioctl\n");
		fflush(stderr);
		next_ioctl = dlsym(RTLD_NEXT, "ioctl");
		fprintf(stderr, "next_ioctl = %p\n", next_ioctl);
		fflush(stderr);
		if ((msg = dlerror()) != NULL) {
			fprintf(stderr, "ioctl: dlopen failed : %s\n", msg);
			fflush(stderr);
			exit(1);
		} else
			fprintf(stderr, "ioctl: wrapping done\n");
		fflush(stderr);
	}
	if(request==0x40)disp_fd=fd;
	if(fd==disp_fd)
	{
		if(screen==-1)
		{
			char *env=getenv("CPT_SCREEN");
			if(env)screen=atoi(env);else screen=0;

		}
		((int*)data)[0]=screen;
	}
	int ret=next_ioctl(fd, request, data);
	if(fd==disp_fd&&request==0x40)write(100,&ret,1);
	return ret;
}
 int printf (const char * format, ...)
 {
	/*char *msg;
	if (next_ioctl == NULL) {
		fprintf(stderr, "ioctl : wrapping ioctl\n");
		fflush(stderr);
		next_ioctl = dlsym(RTLD_NEXT, "ioctl");
		fprintf(stderr, "next_ioctl = %p\n", next_ioctl);
		fflush(stderr);
		if ((msg = dlerror()) != NULL) {
			fprintf(stderr, "ioctl: dlopen failed : %s\n", msg);
			fflush(stderr);
			exit(1);
		} else
			fprintf(stderr, "ioctl: wrapping done\n");
		fflush(stderr);
	}*/
	if(format==0x23bc8c)
	{
		tmp=*(&format+2);
		//fprintf(stderr, "tmp %lld ltime %lld\n", tmp, ltime);
		if(((tmp-ltime>500)||(ltime-tmp>500))&&tmp)
		{
			ltime=*(&format+2);
			write(100,&tmp,8);
		}
		if(!tmp)ltime=0;
	}
	if(format==0x23a9f0)
	{
		fprintf(stderr, "Duration is %d\n", *(&format+2));
		tmp=-1;
		write(100,&tmp, 8);
		tmp=(int)*(&format+2);
		write(100,&tmp, 8);
	}
	if(format==0x23aa80)
	{
		tmp=(int)*(&format+1);
		write(100, &tmp , 8);
		//fprintf(stderr, "%s %d", format, *(&format+2));
	}
	//fprintf(stdout, "0x%x %s \n", format, format);
	if(!skip_printf)if(fprintf(stdout, format, *(&format+1), *(&format+2), *(&format+3), *(&format+4), *(&format+5), *(&format+6), *(&format+7), *(&format+8))<0)skip_printf=1;
 }
