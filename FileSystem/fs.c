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

int fs_mounted = 0;
int *free_bitmap;
int in_blocks;

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

int calcInodeBlocks(){
	int inode_blocks = disk_size() / 10;
	inode_blocks += (disk_size() % 10 == 0) ? 0 : 1;
	return inode_blocks;
}

void updateBitmap() {
	
	union fs_block block;
	union fs_block ind_block;

	int i, j, k, m;

	for (i = 0; i < disk_size(); i++) {
		
		disk_read(i, block.data);

		if (!i) {//superblock
			free_bitmap[0] = (block.super.magic == FS_MAGIC) ? 1 : 0;
		}
		else if (i <= in_blocks) {//inode blocks

			//Check each inode
			int foundValid = 0;
			for (j = 0; j < INODES_PER_BLOCK; j++) {

				if (block.inode[j].isvalid) {

					//The entire block is valid
					free_bitmap[i] = 1;
					foundValid = 1;
					//Check the address of the direct poointers
					for (k = 0; k < POINTERS_PER_INODE; k++) {

						if(block.inode[j].direct[k]) free_bitmap[block.inode[j].direct[k]] = 1;
						//free_bitmap[block.inode[j].direct[k]] = (block.inode[j].direct[k]) ? 1 : 0;
					}
					
					//Check the indirect pointer
					if (block.inode[j].indirect) {
						//Follow the pointer to the indirect block and search it
						disk_read(block.inode[j].indirect, ind_block.data);
						for (m = 0; m < POINTERS_PER_BLOCK; m++) {
							if(ind_block.pointers[m]) free_bitmap[ind_block.pointers[m]] = 1;
							//free_bitmap[ind_block.pointers[m]] = (ind_block.pointers[m]) ? 1 : 0;
						}
					}

				}
			}

			if(!foundValid) free_bitmap[i] = 0;
		}
	}

}

void invalidateInodes(int inodeBlocks){
//Invalidate all inodes in the inode blocks
	union fs_block block;

	int i,j;
	for(i=1; i<=inodeBlocks; i++){
		disk_read(i,block.data);
		for(j=0;j<INODES_PER_BLOCK;j++){
			block.inode[j].isvalid = 0;
		}
		disk_write(i,block.data);
	}
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

	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	int i, j;
	for (i = 1; i <= block.super.ninodeblocks; i++) {
		disk_read(i, block.data);
		for (j = 0; j < INODES_PER_BLOCK; j++) {
			if (block.inode[j].isvalid) dispInode(&(block.inode[j]), i, j);
		}

	}

}

int fs_format()
{
	if (fs_mounted) return 0;
	
	union fs_block datablock;
	datablock.super.magic = FS_MAGIC;
	datablock.super.nblocks = disk_size();
	datablock.super.ninodeblocks = calcInodeBlocks();
	datablock.super.ninodes = calcInodeBlocks()*INODES_PER_BLOCK;

	invalidateInodes(calcInodeBlocks());

	disk_write(0, datablock.data);
	return 1;
}


int fs_mount()
{
	
	union fs_block block;
	disk_read(0,block.data);
	if(block.super.magic != FS_MAGIC) return 0;
	
	//allocate space for bitmap
	free_bitmap = calloc(block.super.nblocks,sizeof(int));
	if(!free_bitmap) return 0;
	in_blocks = block.super.ninodeblocks;

	updateBitmap();
	return 1;
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
			//0 cannot be a valid inumber
			if(inodeBlockIndex == 1 && inodeIndex == 0){
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

	//Reject impossible inodes
	if(inumber > in_blocks*INODES_PER_BLOCK - 1 || inumber < 0) return 0;

	//union fs_block* block = (union fs_block*) malloc(sizeof(union fs_block));
	union fs_block block;

	//Translate inumber to iblock
	int iblock = inumber/INODES_PER_BLOCK + 1;

	//Translate inumber to local index
	int localInodeIndex = inumber%INODES_PER_BLOCK;
	
	//Check to see if iblock is valid
	if(!free_bitmap[iblock]) return 0;

	//Read in the iblock
	disk_read(iblock, block.data);

	//Check to see if inumber is valid
	if(!block.inode[localInodeIndex].isvalid) return 0;

	//Iterate through direct pointers and free blocks referenced by valid pointers
	int i;
	for(i=0;i<POINTERS_PER_INODE;i++){
		if(!block.inode[localInodeIndex].direct[i]) continue;
		free_bitmap[block.inode[localInodeIndex].direct[i]] = 0;
	}

	//Check and iterate through indirect pointers
	if(block.inode[localInodeIndex].indirect){
		union fs_block ind_block;
		disk_read(block.inode[localInodeIndex].indirect, ind_block.data);

		//Iterate through pointers of indirect block
		int k;
		for(k=0;k<POINTERS_PER_BLOCK;k++){
			if(!ind_block.pointers[k]) continue;
			free_bitmap[ind_block.pointers[k]] = 0;
		}

	}

	//Reset size
	block.inode[localInodeIndex].size = 0;
	
	//Invalidate Inode
	block.inode[localInodeIndex].isvalid = 0;

	//Check all inodes in inode block for any valid inode
	int j, foundValidInode = 0;
	for(j=0;j<INODES_PER_BLOCK;j++){
		if(block.inode[j].isvalid){
			foundValidInode = 1;
			break;
		}
	}

	//Invalidate inode block if its emptry
	if(!foundValidInode) free_bitmap[iblock] = 0;

	disk_write(iblock, block.data);

	return 1;
}

int fs_getsize( int inumber )
{
	union fs_block block;
	disk_read(0,block.data);

	//find inode block (C rounds down)

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
	printf("inode at inumber %d is invalid\n",inumber); 

	return -1;
}


int fs_read( int inumber, char *data, int length, int offset )
{

	if(!fs_mounted){
		printf("Error: the filesystem has not been mounted\n");
		return 0;
	}
	if(inumber <= 0) {
		printf("Error: enter a valid inode value\n");		
		return 0;
	}

	int i, j, dblock, bytes_read=0;
	//union fs_block* block = (union fs_block*) malloc(sizeof(union fs_block));
	union fs_block block, indirect_block;
	struct fs_inode inode;
	char loop_data[4096]="";
	char total_data[16384]="";
	

	//local inode number
	int i_offset = inumber % INODES_PER_BLOCK ;
	int block_num = inumber/INODES_PER_BLOCK + 1;
	int pointer_offset = offset/4096;

	//go to the inode's block
	disk_read(block_num, block.data);
	
	inode = block.inode[i_offset];
	int isize=inode.size;

	if((!inode.isvalid) || !isize) return 0;

	int bytes_left = ((isize-offset) < length) ? isize-offset : length;

	//go through each direct pointer in the inode
	for(i=pointer_offset; i<POINTERS_PER_INODE; i++){
		dblock = inode.direct[i];
		if(dblock){
			disk_read(dblock, *(&loop_data));
			strcat(*(&total_data), *(&loop_data));
			bytes_read += ((bytes_left-bytes_read) < 4096) ? bytes_left-bytes_read : 4096;
			
			if(bytes_read >= bytes_left){
				strcpy(data, total_data);
				return bytes_read;
			}
		}
	}

	if(inode.indirect){
		disk_read(inode.indirect, indirect_block.data);
		
		//go through all the pointers in the indirect block, starting at offset
		for(j=(pointer_offset<5) ? 0 : pointer_offset-5; j<POINTERS_PER_BLOCK; j++) {
			if (indirect_block.pointers[j]){
				disk_read(indirect_block.pointers[j], *(&loop_data));
				strcat(*(&total_data), *(&loop_data));
				bytes_read += ((bytes_left-bytes_read) < 4096) ? bytes_left-bytes_read : 4096;

				if(bytes_read >= bytes_left){
					strcpy(data, total_data);
					return bytes_read;
				}
			}
		}
	}
	
	return bytes_read;
}

int fs_write( int inumber, const char *data, int length, int offset )
{	

	if(!fs_mounted){
    printf("Error: the filesystem has not been mounted\n");
    return 0;
  }
  if(inumber <= 0) {
    printf("Error: enter a valid inode value\n");   
    return 0;
  }
	//union fs_block* block = (union fs_block*) malloc(sizeof(union fs_block));	
	union fs_block block, indirect_block;
	struct fs_inode inode;
  int i, j, dblock, bytes_written=0;
	//char loop_data[4096]="";
	char total_data[16384]="";
	strcpy(total_data, data);

  //int tot_bytes = length-offset;
  //local inode number
  int i_offset = inumber % INODES_PER_BLOCK;
  int block_num = inumber/INODES_PER_BLOCK + 1;
	int pointer_offset = offset/4096;
	//printf("%d\t%d\n", i_offset, block_num);

  //go to the inode's block
  disk_read(block_num, block.data);

	int isize = (POINTERS_PER_INODE+POINTERS_PER_BLOCK)*4096;
	int bytes_left = ((isize-offset) < length) ? isize-offset : length;

	//printf("%d\t%d\n", offset, isize);
	inode = block.inode[i_offset];

  if(!inode.isvalid) return 0;

  //go through each direct pointer in the inode
  for(i=pointer_offset; i<POINTERS_PER_INODE; i++){
		//printf("\n\n\n%s\n\n\n", data);
		//printf("%d\n",inode.direct[i]);
    dblock = inode.direct[i];
		//to_write = ((bytes_left-bytes_written) < 4096) ? bytes_left-bytes_written : 4096;
    disk_write(dblock, total_data);
    bytes_written += ((bytes_left-bytes_written) < 4096) ? bytes_left-bytes_written : 4096;
		strcpy(total_data, &data[bytes_written]);
    if(bytes_written >= bytes_left){ 
			block.inode[i_offset].size = offset+bytes_written;
			printf("\n\n%d\n\n", block.inode[i_offset].size);
 			disk_write(block_num, block.data);
			return bytes_written;
		}
  }

  disk_read(inode.indirect, indirect_block.data);


  //go through all the pointers in the indirect block
	for(j=(pointer_offset<5) ? 0 : pointer_offset-5; j<POINTERS_PER_BLOCK; j++) {
		//printf("\n\n\n%s\n\n\n", data);
		//printf("%d\t%d\n", i_offset, block_num);
    disk_write(indirect_block.pointers[j], total_data);
    bytes_written += ((bytes_left-bytes_written) < 4096) ? bytes_left-bytes_written : 4096;
		strcpy(total_data, &data[bytes_written]);
    if(bytes_written >= bytes_left) {
			block.inode[i_offset].size = offset+bytes_written;
			printf("\n\n%d\n\n", block.inode[i_offset].size);
			disk_write(block_num, block.data);
			return bytes_written;
		}
  }

  return bytes_written;
}

