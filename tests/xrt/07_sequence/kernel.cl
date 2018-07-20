__attribute__ ((reqd_work_group_size(1, 1, 1)))
kernel void mysequence(__global unsigned *a)
{
    a[0] = 0X586C0C6C;
    a[1] = 'X';
    a[2] = 0X586C0C6C;
    a[3] = 'I';
    a[4] = 0X586C0C6C;
    a[5] = 'L';
    a[6] = 0X586C0C6C;
    a[7] = 'I';
    a[8] = 0X586C0C6C;
    a[9] = 'N';
    a[10] = 0X586C0C6C;
    a[11] = 'X';
    a[12] = 0X586C0C6C;
    a[13] = '\0';
    a[14] = 0X586C0C6C;
    a[15] = '\0';
}
