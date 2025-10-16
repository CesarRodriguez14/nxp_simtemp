#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/math64.h>
#include "gaussian_random.h"

#define CLT_N 12


__s32 gaussian_s32_clt(__s32 mean, __s32 stddev)
{
	u64 sum = 0;
	int i = 0;
	s64 centered;
	__s32 val = 0;
	
	for(i=0; i < CLT_N;i++)
	{
		sum += prandom_u32();
	}
	
	
	centered = (s64)sum - ((s64)CLT_N*(UINT_MAX/2));

	centered = (centered << 16)/((UINT_MAX/2)*CLT_N);

	centered = (centered * stddev) >> 16;

	val = mean + (s32)centered;
	
	return val;
}

EXPORT_SYMBOL(gaussian_s32_clt);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("LASEC TECHNNOLOGIES");
MODULE_DESCRIPTION("Gaussian RNG Central Limit Theorem");
