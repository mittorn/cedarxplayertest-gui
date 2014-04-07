#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
static int (*next_ioctl)(int fd, int request, void *data) = NULL;
static int (*next_read)(int fd, void *data, size_t nbyte) = NULL;
int disp_fd, screen=-1;
int fifo=-1;
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
		// next_ioctl = dlsym((void *) -11, /* RTLD_NEXT, */ "ioctl");
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
	//fprintf(stderr, "ioctl %d 0x%x\n", fd, request);
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
