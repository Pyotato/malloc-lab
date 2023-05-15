#include <unistd.h>
/*allocator is in mm.c, 
users can complie and link into their applications*/
void mem_init(void);       //init allocator, return successful?  0 : -1        
void mem_deinit(void);
void *mem_sbrk(int incr);
void mem_reset_brk(void); 
void *mem_heap_lo(void);
void *mem_heap_hi(void);
size_t mem_heapsize(void);
size_t mem_pagesize(void);

