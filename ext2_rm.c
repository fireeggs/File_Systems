/*
 * ============================================================================================
 * File Name : ext2_rm.c
 * Name : Seungkyu Kim
 * Created Date : Nov.7.2015
 * Modified Date : Nov.10.2015
 * Description  : This program takes two command line arguments. 
 *                The first is the name of an ext2 formatted virtual disk, and the second 
 *                is an absolute path to a file or link (not a directory) on that disk. 
 *                The program should work like rm, removing the specified file from the disk. 
 *                If the file does not exist or if it is a directory, 
 *                then your program should return the appropriate error.
 * 
 * Copyright 2015 Seungkyu Kim all rights reserved
 * ============================================================================================
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "ext2_utils.h"

unsigned char *disk;

int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_rm <image file name> "
                         "<absolute path on the disk> \n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }

    char* target = copy_arg(argv[2]);
    char* target_final = pathname_final(target);

    // ERRORTRAPPING OF INPUT
    struct ext2_inode *tar_inode = find_inode(target, disk);
    exit_if(!tar_inode,ENOENT); // File not found
    exit_if(tar_inode->i_mode & EXT2_S_IFDIR, EISDIR); // Is a directory

    //////////////////////////////////////////

    tar_inode->i_links_count--;

    if (tar_inode->i_links_count == 0)
        dealloc_file(disk, tar_inode); // Deallocates the data blocks

    // Gets the parent directory's inode 
    struct ext2_inode* p_inode = find_inode(get_pdir_name(target), disk);
    struct ext2_dir_entry_2* d_entry = NULL;

    int b_num, to_break;
    to_break=0;
    unsigned int offset, prev_offset;

    // Traverses the direct pointers of the parent dir
    for (b_num=0; !to_break && b_num<12; b_num++) {  

        // Corresponding data block for the parent, FOR NOW****
        d_entry = (struct ext2_dir_entry_2 *)(disk+
            (p_inode->i_block[b_num]*EXT2_BLOCK_SIZE));

        offset=0;

        // In the current inode, looks for a dir entry that matches in name
        while (offset < EXT2_BLOCK_SIZE) {
            if(!strncmp(target_final, extract_name(d_entry), MAX_STR_LEN)) {
                // Quits both loops
                to_break=1;
                break;
            }
            prev_offset=offset;
            offset += d_entry->rec_len;
            d_entry = (struct ext2_dir_entry_2*)((char*)d_entry+
                d_entry->rec_len);
        }
    }
    b_num--;

    // Now that we have the target file's offset, delink its directory entry!
    struct ext2_dir_entry_2* prev = (struct ext2_dir_entry_2*)(disk+
            (p_inode->i_block[b_num]*EXT2_BLOCK_SIZE)+prev_offset);

    prev->rec_len += d_entry->rec_len;

    rem_inode_from_imap(d_entry->inode, disk);
  
    // Writes all changes back into the .img file
    assert(write(fd, disk, EXT2_ADDR_PER_BLOCK * EXT2_BLOCK_SIZE)>0);
    return 0;
}