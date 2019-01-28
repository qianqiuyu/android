#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include<sys/signal.h> 

struct termios opt;//termio
fd_set fs_read;
struct timeval time;
int len,fs_sel;
char rcv_buf[100];
int rec_flag=0;


struct sigaction saio;
int wait_flag = 0;  
int STOP = 0;
int res;

void  signal_handler_IO (int status)  
{  
  printf ("received SIGIO signale.\n");  
  wait_flag = 0;  
}

int main(int argc, char **argv)
{
	int fd;
	char val[] = "hello world\n";
	
	fd = open("/dev/ttySAC1", O_RDWR);
	if (fd < 0)
		printf("can't open dev\n");
	else
		printf("can open dev\n");

	if (tcgetattr(fd, &opt) != 0)
	{
		printf("fail get data \n");
	}
    	cfsetispeed(&opt, B115200);
    	cfsetospeed(&opt, B115200);
		if (tcsetattr(fd,TCSANOW,&opt) != 0)    
           {  
               printf("com set error!\n");    
              return (0);   
           }


		
	//修改控制模式，保证程序不会占用串口  
    /*opt.c_cflag |= CLOCAL;  
    //修改控制模式，使得能够从串口中读取输入数据  
    opt.c_cflag |= CREAD;
	opt.c_cflag &= ~CRTSCTS;
	opt.c_cflag |= CS8;
	//无奇偶校验位。  
                 opt.c_cflag &= ~PARENB;   
                 opt.c_iflag &= ~INPCK;*/

	
	write(fd, val, strlen(val));




saio.sa_handler = signal_handler_IO;  
  sigemptyset (&saio.sa_mask);  
  saio.sa_flags = 0;  
  saio.sa_restorer = NULL;  
  sigaction (SIGIO, &saio, NULL);

  //allow the process to receive SIGIO  
  fcntl (fd, F_SETOWN, getpid ());  
  //make the file descriptor asynchronous  
  fcntl (fd, F_SETFL, FASYNC); 

  while(STOP == 0)
  {
    usleep(100000);
    //* after receving SIGIO ,wait_flag = FALSE,input is availabe and can be read/
    if(wait_flag == 0)
    {
          memset(rcv_buf,0,15);
      res = read(fd,rcv_buf,15);
       printf("nread=%d,%s\n",res,rcv_buf);
 //     if(res == 1)
 //       STOP = TRUE; //*stop loop if only a CR was input /
       wait_flag = 1; /*wait for new input*/
    }
  }
  while(1);

	FD_ZERO(&fs_read);  
    	FD_SET(fd,&fs_read);
	time.tv_sec = 50;  
    	time.tv_usec = 0;  
while(1)
{
    //使用select实现串口的多路通信  
    fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);
	if(fs_sel)  
       {  
	   	rec_flag++;
		if(rec_flag==1)
		{
	   	memset(rcv_buf,0,15);
              len = read(fd,rcv_buf,15);  
          	printf("I am right!(version1.2) len = %d\n val = %s\n",len,rcv_buf);  
             }// return len;  
             else if(rec_flag==2)
             {
			 	len = read(fd,rcv_buf,15);
			 	rec_flag=0;
             }
       }  
    else  
       {  
          printf("Sorry,I am wrong!\n");  
       }
usleep(10000);
}
	return  0;
}




