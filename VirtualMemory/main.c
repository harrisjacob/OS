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
const char *program, *algorithm;
char *physmem;
struct disk *disk;

int pageFaults, diskReads, diskWrites;


void init_frame_table() {
	int i;
	for(i = 0; i < nframes; i++) {
		frame_table[i] = -1;

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
			printf("Page %d was in frame %d with PROT_READ and PROT_WRITE. It has been written back to disk.\n", frame_table[policy_index], frame);
		}
		else { //Page to be replaced only has read permissions, thus does not need to be written back
			page_table_set_entry(pt, frame_table[policy_index], frame, 0);
			printf("Page %d was in frame %d with PROT_READ. No writeback occured.\n", frame_table[policy_index], frame);
		}

		disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
		page_table_set_entry(pt, page, frame, PROT_READ);
		diskReads++;
		printf("Page %d is now in frame %d with PROT_READ.\n", page, frame);

		frame_table[policy_index] = page;

	}

}


void fifo_policy(struct page_table *pt, int page){
	replace_page(pt, page);
	policy_index++;
	policy_index %= nframes;
}

void random_policy(struct page_table *pt, int page) {
	policy_index = rand() % nframes;
	replace_page(pt, page);
}


void page_fault_handler(struct page_table *pt, int page){
	
	pageFaults++;
	
	printf("Page fault on page %d.\n", page);

	int frame, bits;

	page_table_get_entry(pt, page, &frame, &bits);

	if (bits == 0) { //Has neither read or write. Must employ policy

		printf("Page %d has neither PROT_READ or PROT_WRITE.\n", page);

		if ((frame = frame_table_full()) < 0) { //If table is full

			printf("Frame table had no open frames. Employing selected replacement policy.\n");

			if (!strcmp(algorithm, "rand")) {				//random replacement
				random_policy(pt, page);
			}

			else if (!strcmp(algorithm, "fifo")) { 			//first in first out replacement
				fifo_policy(pt, page);
			}
			//custom algorithm
			else if (!strcmp(algorithm, "custom")) {
				//CUSTOM ALGORITHM FUNCTION GOES HERE
			}
		
		} else {
			
			page_table_set_entry(pt, page, frame, PROT_READ);
			frame_table[frame] = page;

			printf("Frame table had an open frame. Page %d is at frame %d with PROT_READ.\n", page, frame);

		}

	}else{ //Has read permission. Add write permission
		
		page_table_set_entry(pt, page, frame, (PROT_READ|PROT_WRITE));
		printf("Page %d in frame %d was given PROT_WRITE.\n", page, frame);
		
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

	frame_table = malloc(sizeof(int)*npages);
	init_frame_table();




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

	return 0;
}