#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/* 
 * hello_test /dev/hello0
 */

void print_usage(char *file)
{
	printf("%s <dev>\n", file);
}

int main(int argc, char **argv)
{
	int fd;
	char val;
	/*if (argc != 2)
	{
		print_usage(argv[0]);
		return 0;
	}*/
	fd = open("/sys/class/pwm/pwmchip0/export", O_RDWR);val=0;
	write(fd, &val, 1);return 0;
	
	fd = open("/dev/pwm", O_RDWR);
	if (fd < 0)
		printf("can't open /dev/pwm\n");
	else
		printf("can open /dev/pwm\n");

	if (argc == 2)
	{
		if(!strcmp("r", argv[1]))
		{
			read(fd, &val, 1);
			printf("gpio is %d\n", val);
			return 0;
		}
		val = *argv[1] - '0';
		write(fd, &val, 1);
	}	

	return 0;
}


