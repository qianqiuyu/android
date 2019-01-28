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
				/* ����netprint�ķ����߳� */
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

	/*O_NOCTTY������Unix����������Ϊ�������նˡ����Ƶĳ��򣬲�˵�������־�Ļ����κ����붼��Ӱ����ĳ���
	*O_NDELAY������Unix������򲻹���DCD�ź���״̬���������˿��Ƿ����У���˵�������־�Ļ����ó���ͻ���DCD�ź���Ϊ�͵�ƽʱֹͣ��
	*/
	fd = open("/dev/ttySAC1", O_RDWR | O_NOCTTY);//O_NOCTTY:��ʾ�򿪵���һ���ն��豸�����򲻻��Ϊ�ö˿ڵĿ����նˡ�
	if (fd < 0)
		printf("can't open dev\n");
	else
		printf("can open dev\n");
	/*�����Ƿ�Ϊ�ն��豸 */   
  	if(0 == isatty(STDIN_FILENO))
             printf("standard input is not a terminal device/n");

	set_uart(fd,115200,0,8,1,'N');

	/* ��uart_read����ʹ�����в����κ�fd*/
	FD_ZERO(&uart_read);  
	/* ��fd����uart_read���� */
    	FD_SET(fd,&uart_read);
	/*������*/
	time.tv_sec = 0;  
    	time.tv_usec = 0; 

	pthread_create(&g_tRecvTreadID, NULL, RecvTread, NULL);
	
	write(fd, val, strlen(val));
	while(1)
	{
		/* ƽʱ���� */
		pthread_mutex_lock(&g_tSendMutex);
		pthread_cond_wait(&g_tSendConVar, &g_tSendMutex);	
		pthread_mutex_unlock(&g_tSendMutex);
		printf("data val is %s\n", rcv_buf);//��ӡ�ַ���
		//read(pipes[0],buf,sizeof(buf)/sizeof(char));//�����ַ����������䴢����char s[]��
 		//printf("data val is %s\n",buf);//��ӡ�ַ���
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
	
	/*�õ���fdָ��������ز�����������*/
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
		
	/*�޸Ŀ���ģʽ����֤���򲻻�ռ�ô���*/  
    	opt.c_cflag |= CLOCAL;  
    	/*�޸Ŀ���ģʽ��ʹ���ܹ��Ӵ����ж�ȡ�������� */ 
    	opt.c_cflag |= CREAD;

	/*�������������� */ 
    	switch(flow_ctrl)  
    	{  
       	case 0 ://��ʹ��������  
              	opt.c_cflag &= ~CRTSCTS;  
              	break;     
       	case 1 ://ʹ��Ӳ��������  
              	opt.c_cflag |= CRTSCTS;  
              	break;  
       	case 2 ://ʹ�����������  
              	opt.c_cflag |= IXON | IXOFF | IXANY;  
              	break;  
		default:     
                 	printf("Unsupported mode flow\n");  
                 	return -1;
    	}

	/*����������־λ  */
    	opt.c_cflag &= ~CSIZE;

	/*��������λ*/
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

	/*����У��λ */ 
   	 switch (parity)  
    	{    
       	case 'n':  
       	case 'N': //����żУ��λ��  
                 	opt.c_cflag &= ~PARENB;   
                 	opt.c_iflag &= ~INPCK;      
                 	break;   
       	case 'o':    
       	case 'O'://����Ϊ��У��      
                	 opt.c_cflag |= (PARODD | PARENB);   
                	 opt.c_iflag |= INPCK;               
                 	break;   
       	case 'e':   
       	case 'E'://����ΪżУ��    
               	 opt.c_cflag |= PARENB;         
                	 opt.c_cflag &= ~PARODD;         
                	 opt.c_iflag |= INPCK;        
                	 break;  
      	 	case 's':  
       	case 'S': //����Ϊ�ո�   
                 	opt.c_cflag &= ~PARENB;  
                 	opt.c_cflag &= ~CSTOPB;  
                 	break;   
        	default:    
                 	printf("Unsupported parity\n");      
                 	return -1;   
   	 }

	 /* ����ֹͣλ */  
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

	/*����ʹ���������ʱû���յ��س�����Ҳ�ܷ���*/
	//opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	opt.c_oflag &= ~OPOST;//ԭʼ�������
	/*��������,����0x0a��0x0d,�س��ͻ��в���ͬһ���ַ�*/
    	opt.c_oflag &= ~(ONLCR | OCRNL); 

	opt.c_iflag &= ~(ICRNL | INLCR);
	/*ʹ��ASCII��׼��XON��XOFF�ַ��ܴ���*/
    	opt.c_iflag &= ~(IXON | IXOFF | IXANY);    //����Ҫ�������ַ�

	/*�����������������������ݣ����ǲ��ٶ�ȡ*/
	tcflush(fd, TCIFLUSH);
	
	/*��������ݿ��ã���read��෵����Ҫ����ֽ�������������ݿ��ã���read��������0*/
    	opt.c_cc[VTIME] = 0;        //���ó�ʱ
    	opt.c_cc[VMIN] = 0;        //Update the Opt and do it now

	/*
	*ʹ������
	*TCSANOW������ִ�ж����ȴ����ݷ��ͻ��߽������
	*TCSADRAIN���ȴ��������ݴ�����ɺ�ִ��
	*TCSAFLUSH����������buffers  �ı�ʱִ��
	*/
	if (tcsetattr(fd,TCSANOW,&opt) != 0)
	{
		printf("uart set error!\n");    
              return -1; 
	}
	
}


