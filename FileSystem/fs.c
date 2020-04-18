#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int inode_blocks;

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

void updateBitmap() {

	printf("Attepting to build a free block bitmap with %d blocks\n", disk_size());
	int free_bitmap[disk_size()];

	inode_blocks = disk_size() / 10;
	inode_blocks += (disk_size() % 10 == 0) ? 0 : 1;

	union fs_block block;
	union fs_block ind_block;

	int i, j, k, m;

	for (i = 0; i < disk_size(); i++) {
		
		disk_read(i, block.data);
		if (!i) {//superblock
			free_bitmap[i] = (block.super.magic == FS_MAGIC) ? 1 : 0;
		}
		else if (i <= inode_blocks) {//inode blocks

			//Check each inode
			for (j = 0; j < INODES_PER_BLOCK; j++) {
				if (block.inode[j].isvalid) {

					//The entire block is valid
					free_bitmap[i] = 1;

					//Check the address of the direct poointers
					for (k = 0; k < POINTERS_PER_INODE; k++) {
						free_bitmap[block.inode[j].direct[k]] = (block.inode[j].direct[k]) ? 1 : 0;
					}

					//Check the indirect pointer
					if (block.inode[j].indirect) {
						//Follow the pointer to the indirect block and search it
						disk_read(block.inode[j].indirect, ind_block.data);
						for (m = 0; m < POINTERS_PER_BLOCK; m++) {
							free_bitmap[m] = (ind_block.pointers[m]) ? 1 : 0;
						}
					}
				}
				else {
					free_bitmap[i] = 0;
				}
			}
		}
	}


}


int fs_format()
{
	updateBitmap();
	printf("Bitmap has been updated\n");

	return 0;
}

void dispInode(struct fs_inode *myInode, int inodeBlock, int offset) {

	int i, dcount=0;
	printf("inode %d:\n", (inodeBlock - 1)*INODES_PER_BLOCK + offset);
	printf("    size: %d bytes\n", myInode->size);
	
	for (i = 0; i < POINTERS_PER_INODE; i++) {

		if (myInode->direct[i]) {
			dcount += 1;
			if (dcount == 1) printf("    direct blocks:");
			printf(" %d", myInode->direct[i]);
		}

		if (i == POINTERS_PER_INODE - 1  && dcount) printf("\n");

	}

	if (myInode->indirect) {
		printf("    indirect block: %d\n", myInode->indirect);
		printf("    indirect data blocks:");
		union fs_block indirect_block;
		disk_read(myInode->indirect, indirect_block.data);
		
		int j;
		for (j = 0; j < POINTERS_PER_BLOCK; j++) {
			if (indirect_block.pointers[j]) printf(" %d", indirect_block.pointers[j]);
		}
		printf("\n");

	}

}

void fs_debug()
{

	inode_blocks = disk_size() / 10;
	inode_blocks += (disk_size() % 10 == 0) ? 0 : 1;

	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	int i, j;
	for (i = 1; i < inode_blocks; i++) {
		disk_read(i, block.data);
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			if (block.inode[j].isvalid) dispInode(&(block.inode[j]), i, j);
		}

	}



}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}