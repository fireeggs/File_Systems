#include <stdio.h>
#include <stdlib.h>
#include "ext2.h"

#define MAX_STR_LEN	255

// Returns a pointer to the superblock
struct ext2_super_block* get_sb (unsigned char* disk); 

// Returns a pointer to the group descriptor block
struct ext2_group_desc* get_gd (unsigned char* disk);

// Returns a pointer to the start of the itable
unsigned char* get_itbl (unsigned char* disk);

/////////////////////////////////////////
// FUNCTIONS FOR ALLOCATING NEW INODES & 
// DIRECORY ENTRIES, AND WRITING DATA BLOCKS
/////////////////////////////////////////

/* Given a file size value, returns how many blocks 
 * will be needed to store a file of that size. */
unsigned int calc_blocks_needed(long int f_size);

/* Allocates & reserves a new inode and associated data block.
 * Marks both as used in the imap & bmap. 
 * Returns the new inode's number. 
 */
unsigned int alloc_file (unsigned char* disk, 
						long int f_size, unsigned short i_mode);

/* Given an inode, allocates a single indirect block with 
 * 'ptrs_needed' indirect pointers. */
void alloc_indir_block (unsigned char* disk, struct ext2_inode* n_inode,
                        unsigned int ptrs_needed);

/* Given an target inode and a file descriptor corresponding 
 * to a file on the native file system, writes the contents 
 * of that file into the inode's data blocks.
 */
void write_file(unsigned char* disk, 
				struct ext2_inode* n_inode, 
				long int f_size, FILE* native_fd);

/* Given the length of a dir entry's name, returns how much space
 * the dir entry will need in total. */
unsigned int calc_d_entr_size (unsigned int name_len);

/* Given the inode of a (parent) directory, 
 * adds a new directory entry in its data block.
 * Returns a pointer to the directory entry just created.
 */
struct ext2_dir_entry_2* add_dir_entr(unsigned char* disk,
                                    struct ext2_inode* p_directory, 
                                    unsigned int inode_to_add,
                                    char* name, unsigned char type);

/* De-allocates & frees the given inode and all associated data blocks.
 * Frees corresponding bits in the imap & bmap. 
 */
void dealloc_file(unsigned char* disk, struct ext2_inode* inode);

/////////////////////////////////////////
// PATHNAME MANIPULATION FUNCTIONS
/////////////////////////////////////////

/* Given an absolute path 'dir_name', 
 * returns the absolute path of its parent directory 
 */
char* get_pdir_name(char* dir_nam);

/* Given an absolute path 'path', returns the 
 * "last" part of the string after the final "/".
 */
char* pathname_final(char *path);


/////////////////////////////////////////
// INODE, DIRECTORY ENTRY, & DATA BLOCK LOOKUP
/////////////////////////////////////////

/* Given an absolute path 'dir_name', 
 * returns the corresponding directory entry */
struct ext2_dir_entry_2* find_dir_entry(char *dir_name, unsigned char *disk);

/* Given an inode number, returns a pointer to the corresponding inode struct */
struct ext2_inode* inum_to_inode(unsigned int inum, unsigned char *disk);

/* Given an absolute path 'dir_name', returns the corresponding inode */
struct ext2_inode* find_inode(char *dir_name, unsigned char *disk);

/* Given a directory entry 'd_entry', returns a 
 * correct end-truncated name (based on the name_len field) */
char* extract_name(struct ext2_dir_entry_2* d_entry);

/* Given an block number, returns a pointer to the the block */
unsigned char* bnum_to_block(unsigned int inum, unsigned char *disk);


/////////////////////////////////////////
// BITMAP SEARCHING & MANIPULATION
/////////////////////////////////////////

// Returns the index of the lowest-numbered free inode or disk block available
unsigned int find_free_inode_idx(unsigned char *disk);
unsigned int find_free_block_idx(unsigned char *disk);

// Updates inode and block bitmaps upon the allocation of inodes/blocks
void add_inode_to_imap(unsigned int i_num, unsigned char *disk);
void add_block_to_bmap(unsigned int b_num, unsigned char *disk);

// Updates inode and block bitmaps upon the deallocation of inodes/blocks
void rem_inode_from_imap(unsigned int i_num, unsigned char *disk);
void rem_block_from_bmap(unsigned int b_num, unsigned char *disk);


/////////////////////////////////////////
// MISC
/////////////////////////////////////////

// Exits the program and returns a message iff cond is true
void exit_if (int cond, int err_code);

/* Allocates a new character array space, and copies the given character
 * array into this new space. Returns a pointer to the new copy. */
char* copy_arg (char* arg_str);