#include "lib.h"

//Temperature modes names
const char * modes[] ={"default","noisy","ramp"};

//Function to obtain the date in the desired format
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

//Function that shows parameters set when the driver is loaded
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
	
	std::cout << "Sampling rate: " << s_s_ms << "ms | " << s_s_us << "us"<< std::endl;
	std::cout << "Mode: " << s_mode ;
	std::cout << "Temperature threshold: " << s_t_mC <<" m °C" << std::endl;
	
	return 0;	
}

//Funtion to set simulation parameters
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

//Function that checks sampling rate is within the limits
int checkSamplingRate(std::string &st, double &db)
{
	int ret = 0;
	try
	{
		db = std::stod(st);
		if(db > 10000 || db < 0.05)
		{
			std::cerr << "sampling_rate_ms out of range: limits: [0.05, 10000] " << std::endl;
			ret = -1;
		}
	}
	catch(const std::invalid_argument& e)
	{
		std::cerr << "Invalid sampling_rate_ms: "<< e.what() << std::endl;
		ret = -1;
	}
	catch(const std::out_of_range& e)
	{
		std::cerr << "sampling_rate_ms out of range: limits: [0.05, 10000] " << e.what() << std::endl;
		ret = -1;
	}
	return ret;
}

//Function that checks mode is valid
int checkMode(std::string &st, uint8_t &md)
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
				std::cerr << "Invalid mode: " << st << std::endl;
				ret = -1;
		}
	}
	else
	{
		std::cerr << "Invalid mode: " << st << std::endl;
		ret = -1;
	}
	return ret;
}

//Function that checks threshold is within the limits
int checkThreshold(std::string &st, int32_t &my_int)
{
	int ret = 0;
	try
	{
		my_int = static_cast<int32_t>(std::stol(st));
		if(my_int > 100000 || my_int < -50000)
		{
			std::cerr << "threshold_mC out of range: limits: [-50000, 100000] " << std::endl;
			ret = -1;
		}
	}
	catch(const std::invalid_argument& e)
	{
		std::cerr << "Invalid threshold_mC: "<< e.what() << std::endl;
		ret = -1;
	}
	catch(const std::out_of_range& e)
	{
		std::cerr << "threshold_mC out of range: limits: [-50000, 100000] " << e.what() << std::endl;
		ret = -1;
	}
	return ret;
}

//Usage menu
void help_menu()
{
	std::cout << "nxp_simtemp CLI help menu" <<std::endl;
	std::cout << "**********************************************************" <<std::endl;
	std::cout << "*                                                        *" <<std::endl;
	std::cout << "* Simulation of a temperature sensor in a 20°C mean room *" <<std::endl;
	std::cout << "*                                                        *" <<std::endl;
	std::cout << "**********************************************************" << std::endl;
	std::cout << "Correct usage: nxp_simtemp_cli [options]"<<std::endl;
	std::cout << "Options:"<<std::endl;
	std::cout << "-s\t\tsampling_rate_ms limits: [0.05, 10000]"<<std::endl;
	std::cout << "-m\t\tmode [d,n,r] (default, noisy, ramp)"<<std::endl;
	std::cout << "-t\t\ttemp_threshold_mC limits: [-50000, 100000]"<<std::endl;
	std::cout << "-h/--help\tThis help menu"<<std::endl;
	std::cout << "Example usage: nxp_simtemp_cli -s200 -mr -t20000"<<std::endl;
	std::cout << "If no options are provided, default parameters will be applied."<<std::endl;
}

//Funtion that validates and set simulation parameters
bool argumentsVerification(char* argv[], uint32_t &sampling_us, uint8_t &mode, int32_t &threshold_mC)
{
	char flag = 0;
	std::string arg;
	std::string arg_value;
	bool valid_arguments = true;
	int i=1;
	double sampling_ms = (double)(sampling_us / 1000);

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
				std::cout <<arg<<" : Invalid argument 1"<<std::endl;
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
                        std::cout <<arg<<" : Invalid argument 2"<<std::endl;
						valid_arguments = false;
				}
			}
		}
		i++;
	}

	if(valid_arguments)
	{
		std::cout<<"All arguments are valid"<< std::endl;
		std::cout << "Sampling rate set: " << sampling_ms << "ms | " << sampling_us << "us" << std::endl;
		std::cout << "Mode set: " << modes[mode] << std::endl;
		std::cout << "Temperature threshold set: " << threshold_mC <<" m °C" << std::endl;
	}

	return valid_arguments;
}
