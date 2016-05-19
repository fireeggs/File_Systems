/*
 * ============================================================================================
 * File Name : ext2_ln.c
 * Name : Seungkyu Kim
 * Created Date : Nov.3.2015
 * Modified Date : Nov.7.2015
 * Description  : This program takes three command line arguments. 
 *                The first is the name of an ext2 formatted virtual disk. 
 *                The other two are absolute paths on your ext2 formatted disk. 
 *                The program should work like ln, creating a link from the 
 *                first specified file to the second specified path. If the first 
 *                location does not exist (ENOENT), if the second location already 
 *                exists (EEXIST), or if either location refers to a directory (EISDIR), 
 *                then your program should return the appropriate error. 
 *                Note that this version of ln only works with files.
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

    if(argc != 4) {
        fprintf(stderr, "Usage: ext2_ls <image file name> \
            <link target> <link storage location>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }

    char* target = copy_arg(argv[2]);
    char* new_loc = copy_arg(argv[3]);

    // ERRORTRAPPING OF INPUT
    struct ext2_inode *tar_inode = find_inode(target, disk);
    exit_if(!tar_inode, ENOENT);   // File does not exist

    exit_if(tar_inode->i_mode & EXT2_S_IFDIR, EISDIR); // File is a directory

    struct ext2_inode *new_inode = find_inode(new_loc, disk);
    exit_if(new_inode!=NULL, EEXIST);    // New location is occupied

    //////////////////////////////////////////

    // Gets the inode number for the target file.
    struct ext2_dir_entry_2* tar_dir_entry = find_dir_entry(target, disk);
    unsigned int tar_inum = tar_dir_entry->inode;

    tar_inode->i_links_count++;     

    // Find the parent directory of the path where we'll make the new link
    struct ext2_inode* par_dir = find_inode(get_pdir_name(new_loc), disk);
    
    // Makes a new directory entry for the new hard link 
    add_dir_entr(disk, par_dir, tar_inum, new_loc, EXT2_FT_REG_FILE);

    // Writes all changes back into the .img file
    assert(write(fd, disk, EXT2_ADDR_PER_BLOCK * EXT2_BLOCK_SIZE)>0);
    return 0;
}