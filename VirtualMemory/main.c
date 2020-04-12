/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int npages, nframes, policy_index = 0;
int* frame_table;
int* lru_table;
const char *program, *algorithm;
char *physmem;
struct disk *disk;

int pageFaults, diskReads, diskWrites;



int match_counter;
int* page_history;
int page_history_count = 0;
int cycle_thresh = 2;
int detected_cycles;
int rand_switch = 0;



//Helper Functions
void init_frame_table() {
	int i;
	for(i = 0; i < nframes; i++) {
		frame_table[i] = -1;
	}
}

void init_lru_table() {
	int i;
	for (i = 0; i < nframes; i++) {
		lru_table[i] = 0;
	}
}

void init_page_history(){
	int i;
	for (i = 0; i < npages; i++){
		page_history[i] = -1;
	}
}

int frame_table_full() {		//Function returns the first open location in the page table. If none are found, -1 is returned
	int i;
	for (i = 0; i < nframes; i++) {
		if (frame_table[i] == -1) return i;
	}

	return -1;
}

void replace_page(struct page_table *pt, int page) {
	
	int frame, bits;
	page_table_get_entry(pt, frame_table[policy_index], &frame, &bits);

	if (policy_index == frame) {
		if (bits == (PROT_READ|PROT_WRITE)) { // Page to be replaced has write permissions
			disk_write(disk, frame_table[policy_index], &physmem[frame*PAGE_SIZE]);
			page_table_set_entry(pt, frame_table[policy_index], frame, 0);
			diskWrites++;
		}
		else { //Page to be replaced only has read permissions, thus does not need to be written back
			page_table_set_entry(pt, frame_table[policy_index], frame, 0);
		}

		disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
		page_table_set_entry(pt, page, frame, PROT_READ);
		diskReads++;
		lru_table[policy_index] = 1;

		frame_table[policy_index] = page;

	}

}


//Replacement Policies
void fifo_policy(struct page_table *pt, int page){
	replace_page(pt, page);
	policy_index++;
	policy_index %= nframes;
}

void random_policy(struct page_table *pt, int page) {
	policy_index = rand() % nframes;
	replace_page(pt, page);
}

void custom_policy(struct page_table *pt, int page) {
	//Policy implements LRU with a clock style structure, switching to random if a cycle_threshold is exceeded in the page history
	if (rand_switch == 0) {

		if (page_history_count >= npages) {

			if (page == page_history[0]) {
				//Current page occured 1 cycle ago
				match_counter++;
			} else {
				//Pattern broken
				match_counter = 0;
				detected_cycles = 0;
			}

			if (match_counter == npages) {
				//All pages of the cycle match
				match_counter = 0;
				detected_cycles++;
			}

			if (detected_cycles == cycle_thresh) {
				rand_switch = 1;
			}

			//Shift the page history left
			int j;
			for (j = 0; j < npages - 1; j++) {
				page_history[j] = page_history[j + 1];
			}
			page_history[npages-1] = page;
		}else{
			//Finish populating page history
			page_history[page_history_count] = page;
			page_history_count++;

		}


		while (lru_table[policy_index] != 0) {
			lru_table[policy_index] = 0;
			policy_index++;
			policy_index %= nframes;
		}

		replace_page(pt, page);

		policy_index += 2;
		policy_index %= nframes;

	} else {
		random_policy(pt, page);
	}
}


void page_fault_handler(struct page_table *pt, int page){
	
	pageFaults++;
	
	int frame, bits;

	page_table_get_entry(pt, page, &frame, &bits);

	if (bits == 0) { //Has neither read or write. Must employ policy

		if ((frame = frame_table_full()) < 0) { //If table is full

			if (!strcmp(algorithm, "rand")) {				//random replacement
				random_policy(pt, page);
			}

			else if (!strcmp(algorithm, "fifo")) { 			//first in first out replacement
				fifo_policy(pt, page);
			}
			else if (!strcmp(algorithm, "custom")) {
				custom_policy(pt, page);
			}
		
		} else {
			
			disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
			diskReads++;
			page_table_set_entry(pt, page, frame, PROT_READ);
			frame_table[frame] = page;
			lru_table[frame] = 1;
			page_history[page_history_count] = page;
			page_history_count++;

		}

	}else{ //Has read permission. Add write permission
		
		page_table_set_entry(pt, page, frame, (PROT_READ|PROT_WRITE));
	}


}


int main(int argc, char *argv[])
{
	if (argc != 5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <alpha|beta|gamma|delta>\n");
		return 1;
	}

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	algorithm = argv[3];
	program = argv[4];

	//make sure user enters right algorithm
	if(!(!strcmp(algorithm, "rand")||!strcmp(algorithm, "fifo")||!strcmp(algorithm, "custom"))) {
		fprintf(stderr, "unknown algorithm: %s. Must be fifo, rand or custom\n", algorithm);
		return 1;
	}

	//make sure user enters right program
	if(!(!strcmp(program, "alpha")||!strcmp(program, "beta")||!strcmp(program, "delta")||!strcmp(program,"gamma"))) {
		fprintf(stderr, "unknown program: %s. Must be alpha, beta, delta or gamma\n", program);
		return 1;
	}




	frame_table = malloc(sizeof(int)*npages);
	init_frame_table();
	lru_table = malloc(sizeof(int)*npages);
	init_lru_table();
	page_history = malloc(sizeof(int)*npages);




	disk = disk_open("myvirtualdisk", npages);
	if (!disk) {
		fprintf(stderr, "couldn't create virtual disk: %s\n", strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create(npages, nframes, page_fault_handler);
	if (!pt) {
		fprintf(stderr, "couldn't create page table: %s\n", strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	physmem = page_table_get_physmem(pt);

	if (!strcmp(program, "alpha")) {
		alpha_program(virtmem, npages*PAGE_SIZE);

	}
	else if (!strcmp(program, "beta")) {
		beta_program(virtmem, npages*PAGE_SIZE);

	}
	else if (!strcmp(program, "gamma")) {
		gamma_program(virtmem, npages*PAGE_SIZE);

	}
	else if (!strcmp(program, "delta")) {
		delta_program(virtmem, npages*PAGE_SIZE);

	}
	else {
		fprintf(stderr, "unknown program: %s\n", argv[3]);
		return 1;
	}

	printf("\nStats for program execution:\n");
	printf("Page Faults: %d\n", pageFaults);
	printf("Disk Reads: %d\n", diskReads);
	printf("Disk Writes: %d\n\n", diskWrites);


	page_table_delete(pt);
	disk_close(disk);
	free(frame_table);
	free(lru_table);

	free(page_history);
	return 0;
}
