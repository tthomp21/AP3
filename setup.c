#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <time.h>
#include <pthread.h>

int main(){
	
	int fd;

	//Enable gpio20
	fd = open("/sys/class/gpio/export", O_WRONLY);
	write(fd, "20", 2);
	close(fd);

	//Set gpio20 as input
	fd = open("/sys/class/gpio/gpio20/direction", O_WRONLY);
	write(fd, "in", 2);
	close(fd);

	//Set gpio20 interrupt
	fd = open("/sys/class/gpio/gpio20/edge", O_WRONLY);
	write(fd, "both", 4);
	close(fd); 
	
	struct stat st = {0};
	if(stat("/LASR/Pictures", &st) == -1){
		mkdir("/LASR/Pictures", 0700);
	}

	return(0);
}