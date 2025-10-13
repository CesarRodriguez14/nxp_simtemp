#ifndef _SIMTEMP_H_
#define _SIMTEMP_H_

#include <linux/types.h>

#define E_NO_ERR		10
#define E_EV_S_US		11
#define E_OR_S_US		12
#define E_EV_TH			13
#define E_EV_MD			14
#define E_MN_ALLOC		15
#define E_DV_ADD		16
#define E_CL_CREATE		17
#define E_DV_CREATE		18
#define E_KO_CREATE		19
#define E_SYS_CREATE	20

const char * sim_errors[] = { "NO_ERROR",
	"EINVAL_sampling_us",
	"OUTOFRANGE_sampling_us",
	"EINVAL_threshold_mC",
	"EINVAL_mode",
	"MAJORNUM_alloc",
	"ADD_device",
	"CREATE_class",
	"CREATE_device",
	"CREATE_kobject",
	"CREATE_kobject"
};
	
	
	
	

struct simtemp_sample {
	__u64 timestamp_ns; //monotonic timestamp
	__s32 temp_mC;		//milli-degree Celsius
	__u32 flags;		//
}__attribute__((packed));

struct simtemp_flags {
	__u64 counter;		//number of samples since insmod
	__u64 alert;		//number of alerts since insmod
	__u8 l_error;		//last error
}__attribute__((packed));


#endif
