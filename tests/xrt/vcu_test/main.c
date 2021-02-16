#include "dummy_plugin_dec.h"
#define XCLBIN_PATH "/usr/local/lib/aws.xclbin"

int main()
{
	vcu_dec_test(XCLBIN_PATH, 1, 0);
	return 0;
}
