#####################################################################

# CS:APP Malloc Lab

# Handout files for students

#

# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.

# May not be used, modified, or copied without permission.

#

######################################################################

- 해당 실습은 [HarshTrivedi/malloc](https://github.com/HarshTrivedi/malloc)의 리포지터리를 바탕으로 구현했습니다.

---

Main Files:

---

mm.{c,h}
Your solution malloc package. mm.c is the file that you
will be handing in, and is the only file you should modify.

mdriver.c
The malloc driver that tests your mm.c file

short{1,2}-bal.rep
Two tiny tracefiles to help you get started.

Makefile
Builds the driver

---

Other support files for the driver

---

config.h Configures the malloc lab driver
fsecs.{c,h} Wrapper function for the different timer packages
clock.{c,h} Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h} Timer functions based on cycle counters
ftimer.{c,h} Timer functions based on interval timers and gettimeofday()
memlib.{c,h} Models the heap and sbrk function

---

Building and running the driver

---

To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

    unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

    unix> mdriver -h

---

# malloc-lab

> **COMP 221 Project 5: Malloc Dynamic Memory Allocator :** An implementation of malloc in C using explicit free list, as according to the lab assignment of CS-APP book , reaching 88 % efficiency.

---

### DESCRIPTION

The solution adopted is of using an explicit free list to maintain the list of pointers to free blocks. This enhances speed wise performace of allocator as compared to implicit free list, since allocator does not need to traverse the allocated blocks while finding appropiate size of free memory. This improved throughput to great extent. To improve memory allocation (util) , some changes have been made in reallocation strategy. Finally to improve performance a little more, some changes have been made in find fit function so as to prevent very long traversals sometimes.

### DESIGN

In all there are 3 main features that has been added to allocator (changed from default allocator given)

- Implemented explicit free list to maintain the list of pointers to free blocks.

- DataStructure used of free list is Doubly Linked List.
- Programmer accesses only that memory which is allocated. Ie (the allocated blocks) So free blocks can be effieciently to store other important information. Since minimum block size given is 4 words , no free block can be less than 4 words. With this fact, we can easiliy store 2 pointers in free blocks (which requires only 2 words). First pointer will point to the free block (in heap) previous to the current free block (in which I am storing pointers) and the second will point to the free block next to the current free block.

#### 1.

Any free block in heap will be of following format.

```
--------------------------------------------------------------------------
| Prev Free Block ptr (1 word) | Next Free Block ptr(1 word) |  ...      |
--------------------------------------------------------------------------
```

Hence if I have a pointer bp to a free block then i can access next and previous free blocks using macros:

```
	#define GET_NEXT_PTR(bp)  (*(char **)(bp + WSIZE))
	#define GET_PREV_PTR(bp)  (*(char **)(bp))
```

And to set previous and next pointer we have used:

```
    #define SET_NEXT_PTR(bp, qp) (GET_NEXT_PTR(bp) = qp)
	#define SET_PREV_PTR(bp, qp) (GET_PREV_PTR(bp) = qp)
```

Further two functions :

```
    void insert_in_free_list(void *bp)
	remove_from_free_list(void *bp)
```

have been implemented using above macros so as to maintain free list.

#### 2.

Changed realloc function so as to improve the dropping performance in realloc-bal.rep and realloc2-bal.rep

The default realloc function was inefficient due to the reason that it would always call malloc again so find memory of new size and copy all contents to it, even when it is not necessary.

To fix this, we have added a condition in realloc.

Eg, for scenario where realloc asks for newsize of space and current size of block is oldsize and newsize is greater than oldsize. Ie (block needs to expand) then, it should first check if the next block is free or not. In case next block is free and sum total of both block (this block and next block) is greater than the oldsize then we can only change the size of current block, fix header, footer and we are done. No need to copy contents anywhere. Contents of old block are still intact and safe.

For example,

```
--------------------------------------------------------------------
| Current Block (oldsize =8 words)| Next Block (8 word) (free)     |
--------------------------------------------------------------------
```

Now if current block is requested by programmer to extend to 16 words then no need to call malloc again and copy contents.
Just combine two blocks while taking care of extra space remaining:

```
-----------------------------------------------------------------------
| Current Block                     (newsize =16 words)		          |
-----------------------------------------------------------------------
```

#### 3.

Preventing dropping performance due to repetitive same sized malloc calls.

I have a test file called binary-bal.rep in which same malloc requests are made many many of times in a row. Implementing first fit algorithm using explicit list reduces time- efficiency. To prevent from this condition, we can use the following scheme:

If it is realized that many times (say > 30 times) same malloc size request has been made then instead of traversing the full free list , when I know it is going to take long, I can just extend the heap by the requested amount. However care has to be taken so as to prevent many extend_heap calls to make program run out of memory.

##### Extra points about the program:

Headers and Footer have been kept as such in the program. It has the following structure:

```
      31                     3  2  1  0
      -----------------------------------
     | s  s  s  s  ... s  s  s  0  0  a/f
      -----------------------------------
```

- Coalescing scheme is immediate coalescing using boundary tag coalescing.
- Searching through free list executes first fit algorithm.

> ./mdriver -v short1-bal.rep
> Perf index = 46 (util) + 40 (thru) = 86/100

| trace         | valid  | util      | ops      | secs             | Kops   |
| ------------- | ------ | --------- | -------- | ---------------- | ------ |
| trace         | valid  | util      | ops      | secs             | Kops   |
| :------------ | :----: | :-------: | -----:   | ---------------: | -----: |
| 0             | yes    | 90%       | 5694     | 0.000232         | 24735  |
| 1             | yes    | 82%       | 5848     | 0.000171         | 34179  |
| 2             | yes    | 92%       | 6648     | 0.000343         | 20012  |
| 3             | yes    | 92%       | 5380     | 0.000304         | 17981  |
| 4             | yes    | 99%       | 14400    | 0.000220         | 64257  |
| 5             | yes    | 88%       | 4800     | 0.000554         | 8638   |
| 6             | yes    | 85%       | 4800     | 0.000575         | 8490   |
| 7             | yes    | 55%       | 12000    | 0.000364         | 34149  |
| 8             | yes    | 51%       | 24000    | 0.000468         | 50794  |
| 9             | yes    | 93%       | 14401    | 0.000263         | 53298  |
| 10            | yes    | 21%       | 14401    | 0.000270         | 53377  |
| Total         |        | 0.003755  | 0.003755 | 0.003755         | 30032  |

> summary: /mdriver -v short2-bal.rep
>
> - Perf index = 46 (util) + 40 (thru) = 86/100

| trace         | valid  | util      | ops    | secs             | Kops   |
| ------------- | ------ | --------- | ------ | ---------------- | ------ |
| trace         | valid  | util      | ops    | secs             | Kops   |
| :------------ | :----: | :-------: | -----: | ---------------: | -----: |
| 0             | yes    | 90%       | 5694   | 0.000232         | 24585  |
| 1             | yes    | 82%       | 5848   | 0.000171         | 34099  |
| 2             | yes    | 92%       | 6648   | 0.000343         | 19410  |
| 3             | yes    | 92%       | 5380   | 0.000304         | 17715  |
| 4             | yes    | 99%       | 14400  | 0.000220         | 65484  |
| 5             | yes    | 88%       | 4800   | 0.000554         | 8664   |
| 6             | yes    | 85%       | 4800   | 0.000575         | 8343   |
| 7             | yes    | 55%       | 12000  | 0.000364         | 32958  |
| 8             | yes    | 51%       | 24000  | 0.000468         | 51238  |
| 9             | yes    | 93%       | 14401  | 0.000263         | 54757  |
| 10            | yes    | 21%       | 14401  | 0.000261         | 55261  |
| Total         |        | 77%       | 112372 | 0.003755         | 29929  |
