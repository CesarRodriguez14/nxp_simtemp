#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <iomanip>

struct simtemp_sample {
	uint64_t timestamp_ns; //monotonic timestamp
	int32_t temp_mC;		//milli-degree Celsius
	uint32_t flags;		//
}__attribute__((packed));

//Temperature generation modes
#define MODE_NRM 0
#define MODE_NSY 1
#define MODE_RMP 2
 
//Events flags
#define FLAG_NEW_SAMPLE (1<<0)
#define FLAG_THRESHOLD_CROSSED (1<<1)

//Temperature modes names
const char * modes[] ={"default","noisy","ramp"};

static struct simtemp_sample current_sample;

using namespace std;

void getDate(char * date,uint64_t ns)
{
	char buffer[64];
	time_t seconds = ns / 1000000000ULL;
	long nanoseconds = ns % 1000000000ULL;
	
	//Convert to calendar time
	struct tm tm_time;
	localtime_r(&seconds,&tm_time);
	strftime(buffer,sizeof(buffer),"%Y-%m-%dT%H:%M:%S", &tm_time);
	sprintf(date,"%s.%03ldZ",buffer,nanoseconds/1000000);
}

int showDefaultSimParameters()
{
	double s_s_ms,s_s_us = 0;
	std::string s_mode;
	std::string s_t_mC;
	
	char buffer[10];
	ssize_t bytes_read = 0;
	int fd_s_us, fd_mode, fd_t_mC = 0;
	
	fd_s_us = open("/sys/kernel/simtemp/sampling_us",O_RDONLY);
	if(fd_s_us == -1)
	{
		perror("open sampling_us");
		return -1;
	}
	bytes_read = read(fd_s_us,buffer,sizeof(buffer)-1);
	close(fd_s_us);
	if(bytes_read < 0)
	{
		perror("read sampling_us");
		return -1;
	}
	buffer[bytes_read]='\0';
	s_s_us = (double)(std::stod(buffer));
	s_s_ms = s_s_us / 1000;
	
	fd_mode = open("/sys/kernel/simtemp/mode",O_RDONLY);
	if(fd_mode == -1)
	{
		perror("open mode");
		return -1;
	}
	bytes_read = read(fd_mode,buffer,sizeof(buffer)-1);
	close(fd_mode);
	if(bytes_read < 0)
	{
		perror("read mode");
		return -1;
	}
	buffer[bytes_read]='\0';
	s_mode = buffer;
	
	fd_t_mC = open("/sys/kernel/simtemp/threshold_mC",O_RDONLY);
	if(fd_t_mC == -1)
	{
		perror("open threshold_mC");
		return -1;
	}
	bytes_read = read(fd_t_mC,buffer,sizeof(buffer)-1);
	close(fd_t_mC);
	if(bytes_read < 0)
	{
		perror("read threshold_mC");
		return -1;
	}
	buffer[bytes_read-1]='\0';
	s_t_mC = buffer;
	
	cout << "Sampling rate: " << s_s_ms << "ms | " << s_s_us << "us"<< endl;
	cout << "Mode: " << s_mode ;
	cout << "Temperature threshold: " << s_t_mC <<" m °C" << endl;
	
	return 0;	
}

int setSimParameters(const uint32_t s_us, const uint8_t mode, const uint32_t t_mC)
{
	char buffer[10];
	ssize_t bytes_written = 0;
	int fd_s_us, fd_mode, fd_t_mC = 0;
	
	fd_s_us = open("/sys/kernel/simtemp/sampling_us",O_WRONLY);
	if(fd_s_us == -1)
	{
		perror("open sampling_us");
		return -1;
	}
	
	std::snprintf(buffer,sizeof(buffer),"%u",s_us);
	
	bytes_written = write(fd_s_us,buffer,strlen(buffer));
	close(fd_s_us);
	
	if(bytes_written == -EINVAL)
	{
		perror("write sampling_us");
		return -1;
	}
	
	fd_mode = open("/sys/kernel/simtemp/mode",O_WRONLY);
	if(fd_mode == -1)
	{
		perror("open mode");
		return -1;
	}
	std::snprintf(buffer,sizeof(buffer),"%d",mode);
	
	bytes_written = write(fd_mode,buffer,strlen(buffer));
	close(fd_mode);
	
	if(bytes_written == -EINVAL)
	{
		perror("write mode");
		return -1;
	}
	
	fd_t_mC = open("/sys/kernel/simtemp/threshold_mC",O_WRONLY);
	if(fd_t_mC == -1)
	{
		perror("open threshold_mC");
		return -1;
	}
	
	std::snprintf(buffer,sizeof(buffer),"%d",t_mC);
	
	bytes_written = write(fd_t_mC,buffer,strlen(buffer));
	close(fd_t_mC);
	
	if(bytes_written == -EINVAL)
	{
		perror("write threshold_mC");
		return -1;
	}
	
	return 0;
}

int checkSamplingRate(string &st, double &db)
{
	int ret = 0;
	try
	{
		db = std::stod(st);
		if(db > 10000 || db < 0.05)
		{
			std::cerr << "sampling_rate_ms out of range: limits: [0.05, 10000] " << endl;
			ret = -1;
		}
	}
	catch(const std::invalid_argument& e)
	{
		std::cerr << "Invalid sampling_rate_ms: "<< e.what() << endl;
		ret = -1;
	}
	catch(const std::out_of_range& e)
	{
		std::cerr << "sampling_rate_ms out of range: limits: [0.05, 10000] " << e.what() << endl;
		ret = -1;
	}
	return ret;
}

int checkMode(string &st, uint8_t &md)
{
	char flag = st[0];
	int ret = 0;
	
	if(st.length() == 1)
	{
		switch(flag)
		{
			case 'd':
				md = MODE_NRM;
				break;
			case 'n':
				md = MODE_NSY;
				break;
			case 'r':
				md = MODE_RMP;
				break;
			default:
				std::cerr << "Invalid mode: " << st << endl;
				ret = -1;
		}
	}
	else
	{
		std::cerr << "Invalid mode: " << st << endl;
		ret = -1;
	}
	return ret;
}

int checkThreshold(string &st, int32_t &my_int)
{
	int ret = 0;
	try
	{
		my_int = static_cast<int32_t>(std::stol(st));
		if(my_int > 100000 || my_int < -50000)
		{
			std::cerr << "threshold_mC out of range: limits: [-50000, 100000] " << endl;
			ret = -1;
		}
	}
	catch(const std::invalid_argument& e)
	{
		std::cerr << "Invalid threshold_mC: "<< e.what() << endl;
		ret = -1;
	}
	catch(const std::out_of_range& e)
	{
		std::cerr << "threshold_mC out of range: limits: [-50000, 100000] " << e.what() << endl;
		ret = -1;
	}
	return ret;
}


void help_menu()
{
	cout << "nxp_simtemp CLI help menu" <<endl;
	cout << "**********************************************************" <<endl;
	cout << "*                                                        *" <<endl;
	cout << "* Simulation of a temperature sensor in a 20°C mean room *" <<endl;
	cout << "*                                                        *" <<endl;
	cout << "**********************************************************" << endl;
	cout << "Correct usage: nxp_simtemp_cli [options]"<<endl;
	cout << "Options:"<<endl;
	cout << "-s\t\tsampling_rate_ms (Default 100) limits: [0.05, 10000]"<<endl;
	cout << "-m\t\tmode [d,n,r] (default,noisy,ramp)"<<endl;
	cout << "-t\t\ttemp_threshold_mC (Default 20000) limits: [-50000, 100000]"<<endl;
	cout << "-h/--help\tThis help menu"<<endl;
	cout << "Example usage: nxp_simtemp_cli -s200 -mr -t20000"<<endl;
}

int main(int argc, char* argv[])
{
	char flag = 0;
	int i=1;
	bool valid_arguments = true;
	bool poll_dev = true;
	uint32_t sampling_us = 100000;
	double sampling_ms = 100;
	int32_t threshold_mC = 20000;
	uint8_t mode = MODE_NRM;
	string arg;
	string arg_value;
	
	int fd = 0;
	char date[128] = {0};
	struct pollfd pfd;
	
	if(argc>1)
	{
		while(argv[i])
		{
			arg = argv[i];
			if(arg[0]!='-' || arg.length() < 3)
			{
				if(arg == "-h")
				{
					help_menu();
				}
				else
				{
					cout <<arg<<" : Invalid argument 1"<<endl;
				}
				valid_arguments = false;
			}
			else
			{
				flag = arg[1];
				arg_value = arg.substr(2);
				if(arg == "--help")
				{
					help_menu();
					valid_arguments = false;
				}
				else
				{
					switch(flag)
					{
						case 's':
							if(checkSamplingRate(arg_value,sampling_ms) == -1)
							{
								valid_arguments = false;
							}
							else
							{
								sampling_us = (uint32_t)(sampling_ms * 1000);
							}
							break;
						case 'm':
							if(checkMode(arg_value,mode) == -1)
							{
								valid_arguments = false;
							}
							break;
						case 't':
							if(checkThreshold(arg_value,threshold_mC) == -1)
							{
								valid_arguments = false;
							}
							break;
						default:
							cout <<arg<<" : Invalid argument 2"<<endl;
							valid_arguments = false;
					}
				}
			}
			i++;
		}
		
		poll_dev = valid_arguments;
		
		if(poll_dev)
		{
			cout<<"All arguments are valid"<<endl;
			cout << "Sampling rate set: " << sampling_ms << "ms | " << sampling_us << "us"<< endl;
			cout << "Mode set: " << modes[mode] << endl;
			cout << "Temperature threshold set: " << threshold_mC <<" m °C"<<endl;
			if (setSimParameters(sampling_us,mode,threshold_mC) != 0)
			{
				poll_dev = false;
			}
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
			cout << "Cannot open device file... "<< errno <<" (" << strerror(errno) << ") " << endl;
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
				cout << "Timeout waiting for data." << endl;
				continue;
			}
			
			if(pfd.revents & POLLIN)
			{
				if (read(fd,&current_sample,sizeof(current_sample)) == sizeof(current_sample))
				{
					getDate(date,current_sample.timestamp_ns);
					
					cout << date << " temp=" << std::fixed << std::setprecision(1) <<(double)current_sample.temp_mC/1000 << "C alert=" << (current_sample.flags & FLAG_THRESHOLD_CROSSED ? "1":"0") << endl;
				}
			}
		} 
	}
	return 0;
}
