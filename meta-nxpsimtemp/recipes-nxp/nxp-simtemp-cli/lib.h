#ifndef _LIB_H_
#define _LIB_H_

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
extern const char *modes[];

void getDate(char * date,uint64_t ns);

int showDefaultSimParameters();

int setSimParameters(const uint32_t s_us, const uint8_t mode, const uint32_t t_mC);

int checkSamplingRate(std::string &st, double &db);

int checkMode(std::string &st, uint8_t &md);

int checkThreshold(std::string &st, int32_t &my_int);

void help_menu();

bool argumentsVerification(char* argv[], uint32_t &sampling_us, uint8_t &mode, int32_t &threshold_mC);

#endif