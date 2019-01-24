#include "svm.h"

__kernel __attribute__ ((reqd_work_group_size(1,1,1)))
void link_sum(__global Node * node, __global long * output)
{
    long sum = 0;
    int  i = 0;
    while (node != 0) {
        sum += node->val;
        output[i] = sum;
        i++;
        node = node->next;
    }
}
