/*
 * ============================================================================================
 * File Name : ext2_ls.c
 * Name : Seungkyu Kim
 * Created Date : Nov.2.2015
 * Modified Date : Nov.5.2015
 * Description  : This program takes two command line arguments. 
 *                The first is the name of an ext2 formatted virtual disk. 
 *                The second is an absolute path on the ext2 formatted disk. 
 *                The program should work like ls -1, printing each directory entry on a separate line.
 *                Unlike ls -1, it should also print . and ... In other words, 
 *                it will print one line for every directory entry in the directory specified by the absolute path.
 *                If the directory does not exist print "No such file or diretory".
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
#include "ext2_utils.h"

unsigned char *disk;

int main(int argc, char **argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_ls <image file name> "
                         "<absolute path on the disk> \n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	   perror("mmap");
	   exit(1);
    }

    char* path = copy_arg(argv[2]);
    struct ext2_inode *cur_dir = find_inode(path, disk);
    if(!cur_dir) { // Invalid path
        fprintf(stderr, "No such file or directory\n");
        exit(1);
    } 

    // If the specified path is a regular file, just prints out its name.
    if (cur_dir->i_mode & EXT2_S_IFREG) {
        printf("%s\n", pathname_final(path));
        return 0;
    }

    // Prints out all the directory blocks' names
    unsigned int b;     // Block number
    struct ext2_dir_entry_2 *d_entry;
    unsigned int offset;
    for (b=0; b<EXT2_NUM_DIR_PTRS; b++) {
        if(cur_dir->i_block[b]) {
            while (offset < cur_dir->i_size){
                d_entry = (struct ext2_dir_entry_2*)(disk + 
                            EXT2_BLOCK_SIZE*cur_dir->i_block[b] + offset);
                printf("%s\n", extract_name(d_entry));
                offset += d_entry->rec_len;
            }
        }
    }

    return 0;
}