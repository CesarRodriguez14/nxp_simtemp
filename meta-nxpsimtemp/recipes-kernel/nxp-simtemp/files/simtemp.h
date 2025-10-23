#ifndef _SIMTEMP_H_
#define _SIMTEMP_H_

#include <linux/types.h>

struct simtemp_sample {
	__u64 timestamp_ns; //monotonic timestamp
	__s32 temp_mC;		//milli-degree Celsius
	__u32 flags;		//
}__attribute__((packed));

#endif
