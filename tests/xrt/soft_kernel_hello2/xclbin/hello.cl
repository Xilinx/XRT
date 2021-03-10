//------------------------------------------------------------------------------
//
// kernel:  hello  
//
// Purpose: Copy "Hello World" into a global array to be read from the host
//
// output: char buf vector, returned to host to be printed
//

__kernel void __attribute__ ((reqd_work_group_size(1, 1, 1)))
    hello(__global char* buf) {
  // Get global ID
    
 int glbId = get_global_id(0);

 
  // Only one work-item should be responsible
  // for copying into the buffer.
   if (glbId == 0) {
     buf[0]  = 'H';
     buf[1]  = 'e';
     buf[2]  = 'l';
     buf[3]  = 'l';
     buf[4]  = 'o';
     buf[5]  = ' ';
     buf[6]  = 'W';
     buf[7]  = 'o';
     buf[8]  = 'r';
     buf[9]  = 'l';
     buf[10] = 'd';
     buf[11] = '\n';
     buf[12] = '\0';
     }

   //return;
}
