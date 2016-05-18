/*
 * ============================================================================================
 * File Name : ext2_cp.c
 * Name : Seungkyu Kim
 * Created Date : Nov.1.2015
 * Modified Date : Nov.4.2015
 * Description  : This program takes three command line arguments. 
 *                The first is the name of an ext2 formatted virtual disk. 
 *                The second is the path to a file on your native operating system,
 *                and the third is an absolute path on your ext2 formatted disk. 
 *                The program should work like cp, copying the file on your native file system onto 
 *                the specified location on the disk. If the specified file or target location does not exist,
 *                then your program should return the appropriate error (ENOENT)
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

    if (argc != 4) {
        fprintf(stderr, "Usage: ext2_cp <image file name> "
            "<absolute path on native file system> "
            "<absolute path on the virtual disk>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }

    char *v_name = copy_arg(argv[3]);

    // ERRORTRAPPING OF INPUT

    FILE *native_fd = fopen(argv[2], "r");
    exit_if(!native_fd, ENOENT); // File not found in native file system

    struct ext2_inode* cur_dir = find_inode(v_name, disk);
    exit_if(cur_dir!=NULL, EEXIST);  // File already exists

    struct ext2_inode* p_dir = find_inode(get_pdir_name(v_name), disk);
    exit_if(!p_dir, ENOENT); // File not found in virtual file system

    ////////////////////////////////////////////
    
    // Gets the size of the file
    fseek(native_fd, 0L, SEEK_END);
    long int f_size = ftell(native_fd);

    // Allocates inodes & blocks for a new file
    unsigned int free_inode = alloc_file(disk, f_size, EXT2_S_IFREG);
    struct ext2_inode* n_inode = inum_to_inode(free_inode, disk);

    // Writes data into allocated blocks
    fseek(native_fd, 0L, SEEK_SET); 
    write_file(disk, n_inode, f_size, native_fd);

    // Creates a new directory entry for the newly copied file.
    add_dir_entr(disk, p_dir, free_inode, v_name, EXT2_FT_REG_FILE);

    // Writes all changes back into the .img file
    assert(write(fd, disk, EXT2_ADDR_PER_BLOCK * EXT2_BLOCK_SIZE)>0);
    return 0;
}