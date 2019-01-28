#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/signal.h> 
#include <unistd.h>  

#include <pthread.h>
#include <signal.h>
//arm-linux-gcc -o uart-thread uart-thread.c -lpthread


fd_set uart_read;
struct timeval time;
int len,read_flag;
char rcv_buf[100];
//char buf[50];
//int pipes[2];
int fd;
static pthread_t g_tRecvTreadID;
static pthread_mutex_t g_tSendMutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_tSendConVar = PTHREAD_COND_INITIALIZER;

int set_uart(int fd,int boud,int flow_ctrl,int databits,int stopbits,int parity) ;

static void *RecvTread(void *pVoid)
{
	printf(" thread has create success ! \n");
	while(1)
		{
			read_flag = select(fd+1,&uart_read,NULL,NULL,NULL);//select(fd+1,&uart_read,NULL,NULL,&time);
			if(read_flag)  
       		{  
	   			memset(rcv_buf,0,sizeof(rcv_buf)/sizeof(char));
             	 		len = read(fd,rcv_buf,sizeof(rcv_buf)/sizeof(char));  
          			printf("data len = %d\n val = %s\n",len,rcv_buf);  
				/* »½ĞÑnetprintµÄ·¢ËÍÏß³Ì */
				pthread_mutex_lock(&g_tSendMutex);
				pthread_cond_signal(&g_tSendConVar);
				pthread_mutex_unlock(&g_tSendMutex);
          			//write(pipes[1],rcv_buf,strlen(rcv_buf));
			}
			else
			{
				printf(" receive  data error ! \n");
			}
			usleep(10000);
		}
}

int main(int argc, char **argv)
{
	char val[] = "hello world\n";

	/*O_NOCTTY£º¸æËßUnixÕâ¸ö³ÌĞò²»Ïë³ÉÎª¡°¿ØÖÆÖÕ¶Ë¡±¿ØÖÆµÄ³ÌĞò£¬²»ËµÃ÷Õâ¸ö±êÖ¾µÄ»°£¬ÈÎºÎÊäÈë¶¼»áÓ°ÏìÄãµÄ³ÌĞò¡
	*O_NDELAY£º¸æËßUnixÕâ¸ö³ÌĞò²»¹ØĞÄDCDĞÅºÅÏß×´Ì¬£¬¼´ÆäËû¶Ë¿ÚÊÇ·ñÔËĞĞ£¬²»ËµÃ÷Õâ¸ö±êÖ¾µÄ»°£¬¸Ã³ÌĞò¾Í»áÔÚDCDĞÅºÅÏßÎªµÍµçÆ½Ê±Í£Ö¹¡£
	*/
	fd = open("/dev/ttySAC1", O_RDWR | O_NOCTTY);//O_NOCTTY:±íÊ¾´ò¿ªµÄÊÇÒ»¸öÖÕ¶ËÉè±¸£¬³ÌĞò²»»á³ÉÎª¸Ã¶Ë¿ÚµÄ¿ØÖÆÖÕ¶Ë¡£
	if (fd < 0)
		printf("can't open dev\n");
	else
		printf("can open dev\n");
	/*²âÊÔÊÇ·ñÎªÖÕ¶ËÉè±¸ */   
  	if(0 == isatty(STDIN_FILENO))
             printf("standard input is not a terminal device/n");

	set_uart(fd,115200,0,8,1,'N');

	/* ½«uart_readÇåÁãÊ¹¼¯ºÏÖĞ²»º¬ÈÎºÎfd*/
	FD_ZERO(&uart_read);  
	/* ½«fd¼ÓÈëuart_read¼¯ºÏ */
    	FD_SET(fd,&uart_read);
	/*²»×èÈû*/
	time.tv_sec = 0;  
    	time.tv_usec = 0; 

	pthread_create(&g_tRecvTreadID, NULL, RecvTread, NULL);
	
	write(fd, val, strlen(val));
	while(1)
	{
		/* Æ½Ê±ĞİÃß */
		pthread_mutex_lock(&g_tSendMutex);
		pthread_cond_wait(&g_tSendConVar, &g_tSendMutex);	
		pthread_mutex_unlock(&g_tSendMutex);
		printf("data val is %s\n", rcv_buf);//´òÓ¡×Ö·û´®
		//read(pipes[0],buf,sizeof(buf)/sizeof(char));//¶Á³ö×Ö·û´®£¬²¢½«Æä´¢´æÔÚchar s[]ÖĞ
 		//printf("data val is %s\n",buf);//´òÓ¡×Ö·û´®
	}
	close(fd);
	return 0;
}

int set_uart(int fd,int boud,int flow_ctrl,int databits,int stopbits,int parity)  
{ 
	struct termios opt;
	int i;
	int baud_rates[] = { B9600, B19200, B57600, B115200, B38400 };
	int speed_rates[] = {9600, 19200, 57600, 115200, 38400};
	
	/*µÃµ½ÓëfdÖ¸Ïò¶ÔÏóµÄÏà¹Ø²ÎÊı£¬²¢±£´æ*/
	if (tcgetattr(fd, &opt) != 0)
	{
		printf("fail get fd data \n");
		return -1;
	}
	for ( i= 0;  i < sizeof(speed_rates) / sizeof(int);  i++)  
	{  
		if (boud == speed_rates[i])  
		{               
			cfsetispeed(&opt, baud_rates[i]);   
			cfsetospeed(&opt, baud_rates[i]);    
		}  
	} 
		
	/*ĞŞ¸Ä¿ØÖÆÄ£Ê½£¬±£Ö¤³ÌĞò²»»áÕ¼ÓÃ´®¿Ú*/  
    	opt.c_cflag |= CLOCAL;  
    	/*ĞŞ¸Ä¿ØÖÆÄ£Ê½£¬Ê¹µÃÄÜ¹»´Ó´®¿ÚÖĞ¶ÁÈ¡ÊäÈëÊı¾İ */ 
    	opt.c_cflag |= CREAD;

	/*ÉèÖÃÊı¾İÁ÷¿ØÖÆ */ 
    	switch(flow_ctrl)  
    	{  
       	case 0 ://²»Ê¹ÓÃÁ÷¿ØÖÆ  
              	opt.c_cflag &= ~CRTSCTS;  
              	break;     
       	case 1 ://Ê¹ÓÃÓ²¼şÁ÷¿ØÖÆ  
              	opt.c_cflag |= CRTSCTS;  
              	break;  
       	case 2 ://Ê¹ÓÃÈí¼şÁ÷¿ØÖÆ  
              	opt.c_cflag |= IXON | IXOFF | IXANY;  
              	break;  
		default:     
                 	printf("Unsupported mode flow\n");  
                 	return -1;
    	}

	/*ÆÁ±ÎÆäËû±êÖ¾Î»  */
    	opt.c_cflag &= ~CSIZE;

	/*ÉèÖÃÊı¾İÎ»*/
	switch (databits)  
    	{    
       	case 5    :  
                     opt.c_cflag |= CS5;  
                     break;  
       	case 6    :  
                     opt.c_cflag |= CS6;  
                     break;  
       	case 7    :      
                 	opt.c_cflag |= CS7;  
                 	break;  
       	case 8:      
                 	opt.c_cflag |= CS8;  
                 	break;    
       	default:     
                 	printf("Unsupported data size\n");  
                 	return -1;   
    	} 

	/*ÉèÖÃĞ£ÑéÎ» */ 
   	 switch (parity)  
    	{    
       	case 'n':  
       	case 'N': //ÎŞÆæÅ¼Ğ£ÑéÎ»¡£  
                 	opt.c_cflag &= ~PARENB;   
                 	opt.c_iflag &= ~INPCK;      
                 	break;   
       	case 'o':    
       	case 'O'://ÉèÖÃÎªÆæĞ£Ñé      
                	 opt.c_cflag |= (PARODD | PARENB);   
                	 opt.c_iflag |= INPCK;               
                 	break;   
       	case 'e':   
       	case 'E'://ÉèÖÃÎªÅ¼Ğ£Ñé    
               	 opt.c_cflag |= PARENB;         
                	 opt.c_cflag &= ~PARODD;         
                	 opt.c_iflag |= INPCK;        
                	 break;  
      	 	case 's':  
       	case 'S': //ÉèÖÃÎª¿Õ¸ñ   
                 	opt.c_cflag &= ~PARENB;  
                 	opt.c_cflag &= ~CSTOPB;  
                 	break;   
        	default:    
                 	printf("Unsupported parity\n");      
                 	return -1;   
   	 }

	 /* ÉèÖÃÍ£Ö¹Î» */  
    	switch (stopbits)  
    	{    
       	case 1:     
                 	opt.c_cflag &= ~CSTOPB;
			break;   
       	case 2:     
                 	opt.c_cflag |= CSTOPB; 
			break;  
       	default:     
                     printf("Unsupported stop bits\n");   
                       return -1;  
    	}

	/*ÉèÖÃÊ¹µÃÊäÈëÊä³öÊ±Ã»½ÓÊÕµ½»Ø³µ»ò»»ĞĞÒ²ÄÜ·¢ËÍ*/
	//opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	opt.c_oflag &= ~OPOST;//Ô­Ê¼Êı¾İÊä³ö
	/*ÏÂÃæÁ½¾ä,Çø·Ö0x0aºÍ0x0d,»Ø³µºÍ»»ĞĞ²»ÊÇÍ¬Ò»¸ö×Ö·û*/
    	opt.c_oflag &= ~(ONLCR | OCRNL); 

	opt.c_iflag &= ~(ICRNL | INLCR);
	/*Ê¹µÃASCII±ê×¼µÄXONºÍXOFF×Ö·ûÄÜ´«Êä*/
    	opt.c_iflag &= ~(IXON | IXOFF | IXANY);    //²»ĞèÒªÕâÁ½¸ö×Ö·û

	/*Èç¹û·¢ÉúÊı¾İÒç³ö£¬½ÓÊÕÊı¾İ£¬µ«ÊÇ²»ÔÙ¶ÁÈ¡*/
	tcflush(fd, TCIFLUSH);
	
	/*Èç¹ûÓĞÊı¾İ¿ÉÓÃ£¬Ôòread×î¶à·µ»ØËùÒªÇóµÄ×Ö½ÚÊı¡£Èç¹ûÎŞÊı¾İ¿ÉÓÃ£¬ÔòreadÁ¢¼´·µ»Ø0*/
    	opt.c_cc[VTIME] = 0;        //ÉèÖÃ³¬Ê±
    	opt.c_cc[VMIN] = 0;        //Update the Opt and do it now

	/*
	*Ê¹ÄÜÅäÖÃ
	*TCSANOW£ºÁ¢¼´Ö´ĞĞ¶ø²»µÈ´ıÊı¾İ·¢ËÍ»òÕß½ÓÊÜÍê³É
	*TCSADRAIN£ºµÈ´ıËùÓĞÊı¾İ´«µİÍê³ÉºóÖ´ĞĞ
	*TCSAFLUSH£ºÊäÈëºÍÊä³öbuffers  ¸Ä±äÊ±Ö´ĞĞ
	*/
	if (tcsetattr(fd,TCSANOW,&opt) != 0)
	{
		printf("uart set error!\n");    
              return -1; 
	}
	
}


