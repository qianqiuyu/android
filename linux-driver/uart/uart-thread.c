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
				/* 唤醒netprint的发送线程 */
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

	/*O_NOCTTY：告诉Unix这个程序不想成为“控制终端”控制的程序，不说明这个标志的话，任何输入都会影响你的程序�
	*O_NDELAY：告诉Unix这个程序不关心DCD信号线状态，即其他端口是否运行，不说明这个标志的话，该程序就会在DCD信号线为低电平时停止。
	*/
	fd = open("/dev/ttySAC1", O_RDWR | O_NOCTTY);//O_NOCTTY:表示打开的是一个终端设备，程序不会成为该端口的控制终端。
	if (fd < 0)
		printf("can't open dev\n");
	else
		printf("can open dev\n");
	/*测试是否为终端设备 */   
  	if(0 == isatty(STDIN_FILENO))
             printf("standard input is not a terminal device/n");

	set_uart(fd,115200,0,8,1,'N');

	/* 将uart_read清零使集合中不含任何fd*/
	FD_ZERO(&uart_read);  
	/* 将fd加入uart_read集合 */
    	FD_SET(fd,&uart_read);
	/*不阻塞*/
	time.tv_sec = 0;  
    	time.tv_usec = 0; 

	pthread_create(&g_tRecvTreadID, NULL, RecvTread, NULL);
	
	write(fd, val, strlen(val));
	while(1)
	{
		/* 平时休眠 */
		pthread_mutex_lock(&g_tSendMutex);
		pthread_cond_wait(&g_tSendConVar, &g_tSendMutex);	
		pthread_mutex_unlock(&g_tSendMutex);
		printf("data val is %s\n", rcv_buf);//打印字符串
		//read(pipes[0],buf,sizeof(buf)/sizeof(char));//读出字符串，并将其储存在char s[]中
 		//printf("data val is %s\n",buf);//打印字符串
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
	
	/*得到与fd指向对象的相关参数，并保存*/
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
		
	/*修改控制模式，保证程序不会占用串口*/  
    	opt.c_cflag |= CLOCAL;  
    	/*修改控制模式，使得能够从串口中读取输入数据 */ 
    	opt.c_cflag |= CREAD;

	/*设置数据流控制 */ 
    	switch(flow_ctrl)  
    	{  
       	case 0 ://不使用流控制  
              	opt.c_cflag &= ~CRTSCTS;  
              	break;     
       	case 1 ://使用硬件流控制  
              	opt.c_cflag |= CRTSCTS;  
              	break;  
       	case 2 ://使用软件流控制  
              	opt.c_cflag |= IXON | IXOFF | IXANY;  
              	break;  
		default:     
                 	printf("Unsupported mode flow\n");  
                 	return -1;
    	}

	/*屏蔽其他标志位  */
    	opt.c_cflag &= ~CSIZE;

	/*设置数据位*/
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

	/*设置校验位 */ 
   	 switch (parity)  
    	{    
       	case 'n':  
       	case 'N': //无奇偶校验位。  
                 	opt.c_cflag &= ~PARENB;   
                 	opt.c_iflag &= ~INPCK;      
                 	break;   
       	case 'o':    
       	case 'O'://设置为奇校验      
                	 opt.c_cflag |= (PARODD | PARENB);   
                	 opt.c_iflag |= INPCK;               
                 	break;   
       	case 'e':   
       	case 'E'://设置为偶校验    
               	 opt.c_cflag |= PARENB;         
                	 opt.c_cflag &= ~PARODD;         
                	 opt.c_iflag |= INPCK;        
                	 break;  
      	 	case 's':  
       	case 'S': //设置为空格   
                 	opt.c_cflag &= ~PARENB;  
                 	opt.c_cflag &= ~CSTOPB;  
                 	break;   
        	default:    
                 	printf("Unsupported parity\n");      
                 	return -1;   
   	 }

	 /* 设置停止位 */  
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

	/*设置使得输入输出时没接收到回车或换行也能发送*/
	//opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	opt.c_oflag &= ~OPOST;//原始数据输出
	/*下面两句,区分0x0a和0x0d,回车和换行不是同一个字符*/
    	opt.c_oflag &= ~(ONLCR | OCRNL); 

	opt.c_iflag &= ~(ICRNL | INLCR);
	/*使得ASCII标准的XON和XOFF字符能传输*/
    	opt.c_iflag &= ~(IXON | IXOFF | IXANY);    //不需要这两个字符

	/*如果发生数据溢出，接收数据，但是不再读取*/
	tcflush(fd, TCIFLUSH);
	
	/*如果有数据可用，则read最多返回所要求的字节数。如果无数据可用，则read立即返回0*/
    	opt.c_cc[VTIME] = 0;        //设置超时
    	opt.c_cc[VMIN] = 0;        //Update the Opt and do it now

	/*
	*使能配置
	*TCSANOW：立即执行而不等待数据发送或者接受完成
	*TCSADRAIN：等待所有数据传递完成后执行
	*TCSAFLUSH：输入和输出buffers  改变时执行
	*/
	if (tcsetattr(fd,TCSANOW,&opt) != 0)
	{
		printf("uart set error!\n");    
              return -1; 
	}
	
}


