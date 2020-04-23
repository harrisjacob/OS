#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DISK_BLOCK_SIZE	   4096
#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int inode_blocks;
int fs_mounted = 0;
int *free_bitmap;


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
							free_bitmap[ind_block.pointers[m]] = (ind_block.pointers[m]) ? 1 : 0;
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
	//Invalidated inodes, reset inode bitmap, rewrite superblock
	if (fs_mounted) return 0;
	updateBitmap();
	printf("Bitmap has been updated\n");

	return 1;
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
	for (i = 1; i <= inode_blocks; i++) {
		disk_read(i, block.data);
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			if (block.inode[j].isvalid) dispInode(&(block.inode[j]), i, j);
		}

	}



}

int fs_mount()
{
	int i;
	int inodeForLoop;
	int dBlocks;
	int iBlocks;
	int successful = 0;
	union fs_block block;
	disk_read(0,block.data);

	//allocate space for bitmap
	int *free_bitmap = calloc(block.super.nblocks,sizeof(int));

	union fs_block inodeBlock;
	struct fs_inode inode;
	
	//parse FS
	for(i=1; i<block.super.ninodeblocks; i++){

		//disk read
		disk_read(i,inodeBlock.data);

		for(inodeForLoop=0;inodeForLoop<INODES_PER_BLOCK;inodeForLoop++) {

			inode = inodeBlock.inode[inodeForLoop];

			//make sure the inode is valid
			if(inode.isvalid) {
				free_bitmap[i] = 1;
				
				for(dBlocks=0; dBlocks*DISK_BLOCK_SIZE<inode.size && dBlocks < POINTERS_PER_BLOCK; dBlocks++) {
					free_bitmap[inode.direct[dBlocks]] = 1;
				}

				if(inode.size > POINTERS_PER_BLOCK*DISK_BLOCK_SIZE) {
		
					free_bitmap[inode.indirect] = 1;

					union fs_block blockTemp;
					disk_read(inode.indirect,blockTemp.data);

					for(iBlocks=0; iBlocks<(double)inode.size/DISK_BLOCK_SIZE - POINTERS_PER_BLOCK; iBlocks++) {
						free_bitmap[blockTemp.pointers[iBlocks]] = 1;
						successful = 1;
					}
				}
			}
		}
	}
	
	//not really sure about this guys, it says return 1 on success and 0 on failure
	//but not sure when to return if it's successful
	//
	return successful;
}

int fs_create()
{
	int inodeIndex;
	int inodeBlockIndex;

	//check if there is a mounted disk
    if(free_bitmap == NULL){
        printf("There is no mounted disk\n");
        return 0;
    }

    union fs_block block;
    disk_read(0, block.data);
    
    for(inodeBlockIndex = 1; inodeBlockIndex < block.super.nblocks; inodeBlockIndex++){

        //read and start checking for open spaces for open spaces
        disk_read(inodeBlockIndex, block.data);
        struct fs_inode inode;

        for(inodeIndex = 0; inodeIndex < POINTERS_PER_BLOCK; inodeIndex++){
            if(inodeIndex == 0 && inodeBlockIndex == 1) { 
				inodeIndex = 1;
			}            

            inode = block.inode[inodeIndex];            

            if(inode.isvalid == 0){                

                //we can fill the space if the inode is invalid
				//create a new inode of zero length
                inode.isvalid = 1;
                inode.size = 0;
                memset(inode.direct, 0, sizeof(inode.direct));
                inode.indirect = 0;                

                free_bitmap[inodeBlockIndex] = 1;

                block.inode[inodeIndex] = inode;

				//write to disk
                disk_write(inodeBlockIndex, block.data);

				//return the positive inode number
                return inodeIndex + (inodeBlockIndex-1)*INODES_PER_BLOCK;
            }
        }
    }

	//return 0 on failure
    printf("Could not create inode, inode blocks are full");
	return 0;
}

int fs_delete( int inumber )
{

	return 0;
}

int fs_getsize( int inumber )
{
	union fs_block block;
	disk_read(0,block.data);

	//find inode block
	int inodeBlockIndex = (inumber+INODES_PER_BLOCK-1)/INODES_PER_BLOCK;

	//check number is within the limit
	if(inodeBlockIndex > block.super.ninodeblocks){
		printf("Inode number %d is outside the limit\n",inumber);
		return 0;
	}
	
	disk_read(inodeBlockIndex,block.data);
	struct fs_inode inode = block.inode[inumber%INODES_PER_BLOCK];

	//return the logical size of the given inode
	if(inode.isvalid) {
		return inode.size;
	}

	//on failure return -1
	printf("inode at inumber %d is invalid",inumber); 
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
