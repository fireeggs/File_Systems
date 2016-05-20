/*
 * ============================================================================================
 * File Name : ext2_mkdir.c
 * Name : Seungkyu Kim
 * Created Date : Nov.3.2015
 * Modified Date : Nov.7.2015
 * Description  : This program takes two command line arguments. 
 *                The first is the name of an ext2 formatted virtual disk. 
 *                The second is an absolute path on your ext2 formatted disk. 
 *                The program should work like mkdir, creating the final directory on the 
 *                specified path on the disk. If any component on the path to the location 
 *                where the final directory is to be created does not exist or 
 *                if the specified directory already exists, then your program should return 
 *                the appropriate error (ENOENT or EEXIST).
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
#include <assert.h>
#include "ext2_utils.h"
#include <errno.h>

unsigned char *disk;

int main(int argc, char **argv) {
    
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_cp <image file name> "
            "<absolute path on ext2 formatted disk>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }

    char* v_name = copy_arg(argv[2]);

    // ERRORTRAPPING OF INPUT
    
    struct ext2_inode* cur_dir = find_inode(v_name, disk);
    exit_if(cur_dir!=NULL,EEXIST); // Specified directory already exists

    struct ext2_inode* p_directory = find_inode(get_pdir_name(v_name), disk);
    exit_if(!p_directory,ENOENT); // Parent of specified directory doesn't exist

    ////////////////////////////////////////////////
   
    // Allocates an inode & a directory entry for the new directory itself
    unsigned int n_inode_idx = alloc_file(disk, EXT2_BLOCK_SIZE, EXT2_S_IFDIR);
    struct ext2_inode* n_inode = inum_to_inode(n_inode_idx,disk);
    add_dir_entr(disk, p_directory, n_inode_idx, v_name, EXT2_FT_DIR);

    // Adds '.' to the new directory
    add_dir_entr(disk, n_inode, n_inode_idx, ".", EXT2_FT_DIR);
    n_inode->i_links_count++;

    // Adds '..' to the new directory
    add_dir_entr(disk, n_inode, n_inode_idx, "..", EXT2_FT_DIR);
    p_directory->i_links_count++;

    // Writes all changes back into the .img file
    assert(write(fd, disk, EXT2_ADDR_PER_BLOCK * EXT2_BLOCK_SIZE)>0);
    return 0;
}




