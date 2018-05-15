/*
You need to run setup first to configure the gpio pins for the button. The files that get set up are temporary
so setup needs to be run everytime the system is booted.
You can compile both files with the linux command 'gcc -o <destination file name> <source .c file> -lpthread'.
This program will take a picture with a USB camera when the button is pushed and save it into the folder
/LASR/Pictures.  You can change the resolution with the format.fmt.pix.(width/height) lines below.  
This uses V4l2 for the camera capture and native linux gpio commands.


*/

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

int imageNumber = 0;
int bufferLength = 0;

void *savePicture(void *picData){
	//this is a thread that will save the picture data that was passed to it.
	//printf("Saving picture...\n");
	
	//setup the file name
	char buffer[30];
	snprintf(buffer, sizeof(char) * 30, "/LASR/Pictures/Image%i.jpg",imageNumber);
	int jpgfile;
	if((jpgfile = open(buffer, O_WRONLY | O_CREAT, 0660)) < 0){
		perror("open");
		exit(1);
	}
	write(jpgfile, picData, bufferLength);
	close(jpgfile);
	
	printf("Finished saving!");
	pthread_exit(NULL);
}

int main(void){
	
	//*********** thread setup **************
	pthread_t tid;
	
	//*********** camera setup **************
	int cameraFd;
	//open the file descriptor
	if((cameraFd = open("/dev/video2", O_RDWR)) < 0){
		perror("open");
		exit(1);
	}
	
	//query the capabilities of the camera
	struct v4l2_capability cap;
	if(ioctl(cameraFd, VIDIOC_QUERYCAP, &cap) < 0){
		perror("VIDIOC_QUERYCAP");
		exit(1);
	}
	if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
		fprintf(stderr, "capture not supported");
		exit(1);
	}
	if(!(cap.capabilities & V4L2_CAP_STREAMING)){
		fprintf(stderr, "streaming not supported");
		exit(1);
	}
	
	//set the fps... if set higher than the camera can go it will 
	//default to the highest it can
	struct v4l2_streamparm setfps;
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator = 60;
	
	if(ioctl(cameraFd, VIDIOC_S_PARM, &setfps)){
		perror("VIDIOC_S_PARM");
		exit(1);
	} 
	
	//set the format for the capture
	struct v4l2_format format;
	format.type = V4L2_CAP_VIDEO_CAPTURE;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	format.fmt.pix.width = 1280;
	format.fmt.pix.height = 720;
	//format.fmt.pix.height = 1024;
	
	if(ioctl(cameraFd, VIDIOC_S_FMT, &format) < 0){
		perror("VIDIOC_S_FMT");
		exit(1);
	}
	
	//request memory buffers
	struct v4l2_requestbuffers bufrequest;
	bufrequest.type = V4L2_CAP_VIDEO_CAPTURE;
	bufrequest.memory = V4L2_MEMORY_MMAP;
	bufrequest.count = 1;
	
	if(ioctl(cameraFd, VIDIOC_REQBUFS, &bufrequest) < 0){
		perror("VIDIOC_REQBUFS");
		exit(1);
	}
	
	//initialize memory buffers
	struct v4l2_buffer bufferinfo;
	memset(&bufferinfo, 0, sizeof(bufferinfo));
	bufferinfo.type = V4L2_CAP_VIDEO_CAPTURE;
	bufferinfo.memory =  V4L2_MEMORY_MMAP;
	bufferinfo.index = 0;
	
	if(ioctl(cameraFd, VIDIOC_QUERYBUF, &bufferinfo) < 0){
		perror("VIDIOC_QUERYBUF");
		exit(1);
	}
	
	//this gets a pointer to memory.  This is where we will store the picture data.
	void* buffer_start = mmap(
	NULL,
	bufferinfo.length,
	PROT_READ | PROT_WRITE,
	MAP_SHARED,
	cameraFd,
	bufferinfo.m.offset);
	
	if(buffer_start == MAP_FAILED){
		perror("mmap");
		exit(1);
	}
	
	memset(buffer_start, 0, bufferinfo.length);
	
	//*********** button setup **********
	struct pollfd pollfds;
    int nread, result;
    pollfds.fd = open("/sys/class/gpio/gpio20/value", O_RDONLY);
    int timeout = 10000;           /* Timeout in msec. */
    char bufffer[128];

    if( pollfds.fd < 0 ){
        printf(" failed to open gpio \n");
        exit (1);
    }

    pollfds.events = POLLPRI;
	
	//*********** start video **********
	int type = bufferinfo.type;
	if(ioctl(cameraFd, VIDIOC_STREAMON, &type)<0){
		perror("VIDIOC_STREAMON");
		exit(1);
	}
	printf("Stream on\n");
	
	//*********** setup some timers **********
	struct timeval tval_before, tval_after, tval_result, tval_lastPress, tval_gap, tval_interval;
	gettimeofday(&tval_lastPress,NULL);
	tval_interval.tv_sec = 0;
	tval_interval.tv_usec = 50000;
	
	//*********** start button detection **********
	printf("Waiting...\n");
	while (1)
    {
            result = poll (&pollfds, 1, timeout);
            switch (result)
            {
                  case 0 :
					//the poll timed out and will restart itself.
                    printf ("Waiting...\n");
                    break;
                  case -1:
					//an error occurred somewhere in the polling process.
                    printf ("poll error \n");
                    exit (1);

                  default:
					gettimeofday(&tval_before, NULL);
					timersub(&tval_before, &tval_lastPress, &tval_gap);
					gettimeofday(&tval_lastPress, NULL);
					if (pollfds.revents & POLLPRI)
					{
						nread = read (pollfds.fd, bufffer, 8);
						if (nread == 0) {
							//not the value we were looking for so seek back to the beginning of the file
							lseek(pollfds.fd, 0, SEEK_SET);
						} 
						else {
							bufffer[nread] = 0;
							if(*bufffer == '0')
							{
								if(timercmp(&tval_gap, &tval_interval, >))
								{
									//button was pressed.
									//attempt to debounce the button.
									//Q a frame and DQ that same frame
									//if you set the bufferinfo.index before you DQ the frame it slows down capture time.
									//the driver will set the index on its own when a frame is captured and only need to
									//set the index before you Q the next frame to prevent Qing the same frame twice.
									
									//printf("Capturing frame...\n");
									if(ioctl(cameraFd, VIDIOC_QBUF, &bufferinfo) < 0){
										perror("VIDIOC_QBUF");
										exit(1);
									}
									
									if(ioctl(cameraFd, VIDIOC_DQBUF, &bufferinfo) < 0){
										perror("VIDIOC_DQBUF");
										exit(1);
									}
									
									gettimeofday(&tval_after, NULL);
									timersub(&tval_after, &tval_before, &tval_result);
									printf("Button press to DQ: %ld.%06ld\n",tval_result.tv_sec, tval_result.tv_usec);
									
									bufferLength = bufferinfo.length;
									//start a thread so we can save the image
									pthread_create(&tid, NULL, savePicture, buffer_start);
									imageNumber++;
									bufferinfo.index = 0;
								}
							}
							//seek back to the beginning of the file so we can poll it again
							//poll will spam if you do not do this.
							lseek(pollfds.fd, 0, SEEK_SET);
						}
					}
              }
     }
	 //closes all the things
     close(pollfds.fd);
	 
	 //*********** stop video **********
	 if(ioctl(cameraFd, VIDIOC_STREAMOFF, &type) < 0){
		perror("VIDIOC_STREAMOFF");
		exit(1);
	}
	printf("stream off\n");
	
}