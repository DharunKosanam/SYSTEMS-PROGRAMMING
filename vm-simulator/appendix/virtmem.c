/*
 * Some starter code for CSC 360, Spring 2025, Assignment #4
 *
 * Prepared by: 
 * Michael Zastre (University of Victoria) -- 2024
 * 
 * Modified for ease-of-use and marking by 
 * Konrad Jasman (University of Victoria) -- 2025
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <unistd.h>
 
 /*
  * Some compile-time constants.
  */
 
 #define REPLACE_NONE 0
 #define REPLACE_FIFO 1
 #define REPLACE_LRU  2
 #define REPLACE_CLOCK 3
 #define REPLACE_OPTIMAL 4
 
 #define TRUE 1
 #define FALSE 0
 #define PROGRESS_BAR_WIDTH 60
 #define MAX_LINE_LEN 100
 
 /*
  * Global variables set via command-line arguments.
  */
 int size_of_frame = 0;  /* power of 2 */
 int size_of_memory = 0; /* number of frames */
 int page_replacement_scheme = REPLACE_NONE;
 
 /*
  * Some function prototypes to keep the compiler happy.
  */
 int setup(void);
 int teardown(void);
 int output_report(void);
 long resolve_address(long, int);
 void error_resolve_address(long, int);
 
 /* Page replacement helper functions */
 int select_victim_fifo(void);
 int select_victim_lru(void);
 int select_victim_clock(void);
 
 /*
  * Variables used to keep track of the number of memory-system events
  * that are simulated.
  */
 int page_faults = 0;
 int mem_refs    = 0;
 int swap_outs   = 0;
 int swap_ins    = 0;
 
 /*
  * Global variables for replacement algorithms.
  * For LRU timestamp, FIFO index, and CLOCK hand pointer.
  */
 unsigned long current_time = 0;
 int fifo_index = 0;
 int clock_hand = 0;
 
 /*
  * Page-table information. You are permitted to modify this in order to
  * implement schemes such as CLOCK. However, you are not required
  * to do so.
  */
 struct page_table_entry {
     long page_num;
     int dirty;
     int free;
     unsigned long timestamp;   /* For LRU: stores the last access time. */
     int reference;             /* For CLOCK: reference/use bit. */
 };
 struct page_table_entry *page_table = NULL;
 
 /*
  * Function to convert a logical address into its corresponding 
  * physical address. The value returned by this function is the
  * physical address (or -1 if no physical address can exist for
  * the logical address given the current page-allocation state).
  */
 long resolve_address(long logical, int memwrite){
     int i;
     long page, frame;
     long offset;
     long mask = 0;
     long effective;
 
     /* Get the page and offset */
     page = (logical >> size_of_frame);
     for (i = 0; i < size_of_frame; i++){
         mask = (mask << 1) | 1;
     }
     offset = logical & mask;
 
     /* Find page in the (inverted) page table. */
     frame = -1;
     for (i = 0; i < size_of_memory; i++){
         if (!page_table[i].free && page_table[i].page_num == page){
             frame = i;
             break;
         }
     }
 
     /* If frame is not -1, then we can successfully resolve the
      * address and return the result. Update LRU and CLOCK info.
      */
     if (frame != -1){
         current_time++;
         page_table[i].timestamp = current_time; // update timestamp for LRU
         page_table[i].reference = 1;            // set reference bit for CLOCK
         if (memwrite)
             page_table[i].dirty = 1;            // mark as dirty if write
         effective = (frame << size_of_frame) | offset;
         return effective;
     }
 
     /* Page fault: increment counter */
     page_faults++;
 
     /* Look for a free frame */
     int free_frame = -1;
     for (i = 0; i < size_of_memory; i++){
         if (page_table[i].free){
             free_frame = i;
             break;
         }
     }
 
     /* If a free frame is available, patch up the page table entry
      * and compute the effective address.
      */
     if (free_frame != -1){
         page_table[free_frame].page_num = page;
         page_table[free_frame].free = FALSE; /* Corrected: use free_frame */
         page_table[free_frame].dirty = (memwrite ? 1 : 0);
         current_time++;
         page_table[free_frame].timestamp = current_time;
         page_table[free_frame].reference = 1;
         swap_ins++;
         effective = (free_frame << size_of_frame) | offset;
         return effective;
     } else {
         /* No free frame: use the selected replacement algorithm */
         int victim_frame = -1;
         switch(page_replacement_scheme) {
             case REPLACE_FIFO:
                 victim_frame = select_victim_fifo();
                 break;
             case REPLACE_LRU:
                 victim_frame = select_victim_lru();
                 break;
             case REPLACE_CLOCK:
                 victim_frame = select_victim_clock();
                 break;
             case REPLACE_OPTIMAL:
                 /* For bonus: implement optimal replacement; fall back to FIFO */
                 victim_frame = select_victim_fifo();
                 break;
             default:
                 return -1;
         }
         /* If victim is dirty, simulate a swap-out */
         if (page_table[victim_frame].dirty)
             swap_outs++;
         /* Replace victim frame with new page */
         page_table[victim_frame].page_num = page;
         page_table[victim_frame].dirty = (memwrite ? 1 : 0);
         current_time++;
         page_table[victim_frame].timestamp = current_time;
         page_table[victim_frame].reference = 1;
         swap_ins++;
         effective = (victim_frame << size_of_frame) | offset;
         return effective;
     }
 }
 
 
 /*
  * Super-simple progress bar.
  */
 void display_progress(int percent){
     int to_date = PROGRESS_BAR_WIDTH * percent / 100;
     static int last_to_date = 0;
     int i;
 
     if (last_to_date < to_date){
         last_to_date = to_date;
     } else {
         return;
     }
 
     printf("Progress [");
     for (i = 0; i < to_date; i++){
         printf(".");
     }
     for (; i < PROGRESS_BAR_WIDTH; i++){
         printf(" ");
     }
     printf("] %3d%%", percent);
     printf("\r");
     fflush(stdout);
 }
 
 
 /*
  * Setup the simulator by allocating and initializing the page table.
  */
 int setup(){
     int i;
 
     page_table = (struct page_table_entry *)malloc(
          sizeof(struct page_table_entry) * size_of_memory
     );
     if (page_table == NULL){
         fprintf(stderr, "Simulator error: cannot allocate memory for page table.\n");
         exit(1);
     }
 
     for (i = 0; i < size_of_memory; i++){
         page_table[i].free = TRUE;
         page_table[i].dirty = 0;
         page_table[i].timestamp = 0;
         page_table[i].reference = 0;
     }
 
     /* Initialize global replacement pointers */
     current_time = 0;
     fifo_index = 0;
     clock_hand = 0;
     return -1;
 }
 
 
 /*
  * Teardown routine to free allocated resources.
  */
 int teardown(){
     if (page_table != NULL){
         free(page_table);
     }
     return -1;
 }
 
 
 /*
  * Print an error message when address resolution fails.
  */
 void error_resolve_address(long a, int l){
     fprintf(stderr, "\n");
     fprintf(stderr, "Simulator error: cannot resolve address 0x%lx at line %d\n", a, l);
     exit(1);
 }
 
 
 /*
  * Output a simulation report.
  */
 int output_report(){
     printf("\n");
     printf("Memory references: %d\n", mem_refs);
     printf("Page faults: %d\n", page_faults);
     printf("Swap ins: %d\n", swap_ins);
     printf("Swap outs: %d\n", swap_outs);
     return -1;
 }
 
 
 /*
  * FIFO page replacement:
  * Uses a global fifo_index to select the next victim frame in a cyclic manner.
  */
 int select_victim_fifo(void){
     int victim = fifo_index;
     fifo_index = (fifo_index + 1) % size_of_memory;
     return victim;
 }
 
 
 /*
  * LRU page replacement:
  * Iterates through the page table to find the frame with the oldest timestamp.
  */
 int select_victim_lru(void){
     int victim = -1;
     unsigned long oldest = ~0UL;  /* maximum unsigned long */
     int i;
     for (i = 0; i < size_of_memory; i++){
         if (page_table[i].timestamp < oldest){
             oldest = page_table[i].timestamp;
             victim = i;
         }
     }
     return victim;
 }
 
 
 /*
  * CLOCK page replacement:
  * Implements a simple clock algorithm using a circular pointer.
  */
 int select_victim_clock(void){
     while (1){
         if (page_table[clock_hand].reference == 0){
             int victim = clock_hand;
             clock_hand = (clock_hand + 1) % size_of_memory;
             return victim;
         } else {
             /* Give the page a second chance */
             page_table[clock_hand].reference = 0;
             clock_hand = (clock_hand + 1) % size_of_memory;
         }
     }
 }
 
 
 /*
  * Main program entry point.
  */
 int main(int argc, char **argv){
     int i;
     char *s;
     FILE *infile = NULL;
     char *infile_name = NULL;
     struct stat infile_stat;
     int line_num = 0;
     int infile_size = 0;
     char buffer[MAX_LINE_LEN];
     long addr;
     char addr_type;
     int is_write;
     int show_progress = FALSE;
 
     /* Process the command-line parameters. Note that REPLACE_OPTIMAL is not required. */
     for (i = 1; i < argc; i++){
         if (strncmp(argv[i], "--replace=", 9) == 0){
             s = strstr(argv[i], "=") + 1;
             if (strcmp(s, "fifo") == 0){
                 page_replacement_scheme = REPLACE_FIFO;
             } else if (strcmp(s, "lru") == 0){
                 page_replacement_scheme = REPLACE_LRU;
             } else if (strcmp(s, "clock") == 0){
                 page_replacement_scheme = REPLACE_CLOCK;
             } else if (strcmp(s, "optimal") == 0){
                 page_replacement_scheme = REPLACE_OPTIMAL;
             } else {
                 page_replacement_scheme = REPLACE_NONE;
             }
         } else if (strncmp(argv[i], "--file=", 7) == 0){
             infile_name = strstr(argv[i], "=") + 1;
         } else if (strncmp(argv[i], "--framesize=", 12) == 0){
             s = strstr(argv[i], "=") + 1;
             size_of_frame = atoi(s);
         } else if (strncmp(argv[i], "--numframes=", 12) == 0){
             s = strstr(argv[i], "=") + 1;
             size_of_memory = atoi(s);
         } else if (strcmp(argv[i], "--progress") == 0){
             show_progress = TRUE;
         }
     }
 
     if (infile_name == NULL){
         infile = stdin;
     } else if (stat(infile_name, &infile_stat) == 0){
         infile_size = (int)(infile_stat.st_size);
         infile = fopen(infile_name, "r");
     }
 
     if (page_replacement_scheme == REPLACE_NONE ||
         size_of_frame <= 0 ||
         size_of_memory <= 0 ||
         infile == NULL)
     {
         fprintf(stderr, "usage: %s --framesize=<m> --numframes=<n> --replace={fifo|lru|clock|optimal} [--file=<filename>]\n", argv[0]);
         exit(1);
     }
 
     setup();
 
     while (fgets(buffer, MAX_LINE_LEN-1, infile)){
         line_num++;
         if (strstr(buffer, ":")){
             sscanf(buffer, "%c: %lx", &addr_type, &addr);
             if (addr_type == 'W'){
                 is_write = TRUE;
             } else {
                 is_write = FALSE;
             }
             if (resolve_address(addr, is_write) == -1){
                 error_resolve_address(addr, line_num);
             }
             mem_refs++;
         }
         if (show_progress){
             display_progress(ftell(infile) * 100 / infile_size);
         }
     }
 
     teardown();
     output_report();
     fclose(infile);
     exit(0);
 }
 