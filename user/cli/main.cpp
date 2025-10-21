#include "lib.h"

static struct simtemp_sample current_sample;


int main(int argc, char* argv[])
{
	bool poll_dev = true;
	uint32_t sampling_us = 120000;
	int32_t threshold_mC = 25000;
	uint8_t mode = MODE_NRM;
	
	
	int fd = 0;
	char date[128] = {0};
	struct pollfd pfd;
	
	if(argc>1)
	{	
		if(argumentsVerification(argv,sampling_us,mode,threshold_mC))
		{
			if (setSimParameters(sampling_us,mode,threshold_mC) != 0)
			{
				poll_dev = false;
			}
		}
		else
		{
			poll_dev = false;
		}
	}
	else
	{
		if(showDefaultSimParameters() != 0)
		{
			poll_dev = false;
		}
	}
	if(poll_dev)
	{
		fd = open("/dev/simtemp",O_RDONLY);
		if(fd<0)
		{
			std::cout << "Cannot open device file... "<< errno <<" (" << strerror(errno) << ") " << std::endl;
			return 1;
		}
		pfd.fd = fd;
		pfd.events = POLLIN;
		while(1)
		{
			int ret = poll(&pfd,1,5000);
			if(ret == -1)
			{
				perror("Error during poll");
				break;
			}
			else if(ret == 0)
			{
				std::cout << "Timeout waiting for data." << std::endl;
				continue;
			}
			
			if(pfd.revents & POLLIN)
			{
				if (read(fd,&current_sample,sizeof(current_sample)) == sizeof(current_sample))
				{
					getDate(date,current_sample.timestamp_ns);
					
					std::cout << date << " temp=" << std::fixed << std::setprecision(1) <<(double)current_sample.temp_mC/1000 << "C alert=" << (current_sample.flags & FLAG_THRESHOLD_CROSSED ? "1":"0") << std::endl;
				}
			}
		} 
	}
	return 0;
}
