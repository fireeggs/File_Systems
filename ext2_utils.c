#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "ext2.h"
#include "ext2_utils.h"


// Returns a pointer to the superblock
struct ext2_super_block* get_sb (unsigned char* disk) {
    return (struct ext2_super_block*)(disk + EXT2_BLOCK_SIZE);
}
// Returns a pointer to the group descriptor block
struct ext2_group_desc* get_gd (unsigned char* disk) {
    return (struct ext2_group_desc*)(disk + 2 * EXT2_BLOCK_SIZE);
}
// Returns a pointer to the start of the itable
unsigned char* get_itbl (unsigned char* disk) {
    return (disk + EXT2_BLOCK_SIZE * get_gd(disk)->bg_inode_table);
}

/////////////////////////////////////////
// FUNCTIONS FOR ALLOCATING NEW INODES & 
// DIRECORY ENTRIES, AND WRITING DATA BLOCKS
/////////////////////////////////////////

/* Given a file size value, returns how many blocks 
 * will be needed to store a file of that size. */
unsigned int calc_blocks_needed(long int f_size) {

    int blocks_needed = f_size / EXT2_BLOCK_SIZE;   
    if(f_size % EXT2_BLOCK_SIZE)    // To round up
        blocks_needed++;
    if(blocks_needed > EXT2_NUM_DIR_PTRS) // If we'll need an indirection
        blocks_needed++;

    return blocks_needed;
}

/* Allocates & reserves a new inode and 'blocks_needed' associated data blocks.
 * Marks corresponding bits in the imap & bmap. 
 * Returns the new inode's number. 
 */
unsigned int alloc_file (unsigned char* disk,
                        long int f_size, unsigned short i_mode) {
    int i;
    unsigned int* i_tbl = (unsigned int*) get_itbl(disk);
    unsigned int blocks_needed = calc_blocks_needed(f_size);

    // Checks that enough free blocks are available for allocation
    exit_if(blocks_needed > get_sb(disk)->s_free_blocks_count, ENOSPC);

    // Create a new inode for the file
    unsigned int free_inode = find_free_inode_idx(disk);
    add_inode_to_imap(free_inode, disk);
    struct ext2_inode* n_inode = (struct ext2_inode*)(i_tbl) + free_inode - 1;

    n_inode->i_mode = i_mode; 
    n_inode->i_blocks = 2 * blocks_needed;
    n_inode->i_links_count = 1;
    n_inode->i_size = f_size;

    for (i = 0; i < EXT2_INODE_PTR_LEN; i++)
        n_inode->i_block[i] = 0;    

    // Allocates direct blocks
    for(i = 0; i < EXT2_NUM_DIR_PTRS && i < blocks_needed; i++) {
        n_inode->i_block[i] = find_free_block_idx(disk);
        add_block_to_bmap(n_inode->i_block[i], disk); 
    }

    // Reserves a single indirect block, if needed
    if(i >= EXT2_NUM_DIR_PTRS)
        alloc_indir_block(disk, n_inode, blocks_needed-EXT2_NUM_DIR_PTRS);

    return free_inode;
}

/* Given an inode, allocates a single indirect block with 
 * 'ptrs_needed' indirect pointers. */
void alloc_indir_block (unsigned char* disk, struct ext2_inode* n_inode,
                        unsigned int ptrs_needed) {
    int i;
    unsigned int idr_block_idx = find_free_block_idx(disk);
    add_block_to_bmap(idr_block_idx, disk);
    n_inode->i_block[EXT2_NUM_DIR_PTRS] = idr_block_idx;

    // Memory region for the indirect block
    unsigned int* indir_block = (unsigned int*)(bnum_to_block
                                                (idr_block_idx,disk));

    // Starts laying down pointers in the indirect block
    for(i = 0; i < ptrs_needed; i++) {
        indir_block[i] = find_free_block_idx(disk);
        add_block_to_bmap(indir_block[i], disk);
    }

    // Zeroes out the rest of the data in the indirect block
    for(i = ptrs_needed - EXT2_NUM_DIR_PTRS; i < EXT2_ADDR_PER_BLOCK; i++) 
        indir_block[i] = 0;
}


/* De-allocates & frees the given inode and all associated data blocks.
 * Frees corresponding bits in the imap & bmap. 
 */
void dealloc_file (unsigned char* disk, struct ext2_inode* inode) {
    int i;
    
    unsigned int* ptrs = inode->i_block;
    // Frees the direct blocks
    for(i = 0; i < EXT2_NUM_DIR_PTRS && ptrs[0]; i++) {
        rem_block_from_bmap(ptrs[i],disk);
        ptrs[i] = 0;
    }

    // Clears the single indirect block, if needed
    if(!ptrs[EXT2_NUM_DIR_PTRS])
        return;
    
    unsigned int idr_block_idx = inode->i_block[EXT2_NUM_DIR_PTRS];
    rem_block_from_bmap(idr_block_idx,disk);

    // The array of pointers stored in the single indirect block
    unsigned int* indir_block = (unsigned int*)(disk + 
        EXT2_BLOCK_SIZE * (idr_block_idx));

    // Clears all the (non-zero) pointers in the indirect block
    for(i = 0; i < EXT2_NUM_DIR_PTRS && indir_block[i]; i++)
        rem_block_from_bmap(indir_block[i],disk);

    return;
}

/* Given an target inode and a file descriptor corresponding 
 * to a file on the native file system, writes the contents 
 * of that file into the inode's data blocks. 
 * Returns a pointer to the newly modified disk image.
 */
void write_file (unsigned char* disk, 
                struct ext2_inode* n_inode, 
                long int f_size, FILE* native_fd) {
    int i, result;
    void *block; 
    unsigned int blocks_needed = calc_blocks_needed(f_size);

    // Writes to the direct blocks
    for(i = 0; i < blocks_needed && i < EXT2_NUM_DIR_PTRS; i++) {
        block = (void*)(bnum_to_block(n_inode->i_block[i], disk));
        result = fread(block, sizeof(char), 
                        EXT2_BLOCK_SIZE / sizeof(char), native_fd);
        assert(result>=0);
    }

    // Writes data into (already-allocated) the single indirect block if needed
    if(blocks_needed <= EXT2_NUM_DIR_PTRS)
        return;
    
    unsigned int idr_block_idx = n_inode->i_block[EXT2_NUM_DIR_PTRS];

    // The array of pointers stored in the single indirect block
    unsigned int* indir_block = (unsigned int*)(disk + 
        EXT2_BLOCK_SIZE * (idr_block_idx));

    // Follows those pointers to where the data will actually be deposited
    for(i = 0; i < blocks_needed - EXT2_NUM_DIR_PTRS; i++) {
        block = (void*)(bnum_to_block(indir_block[i], disk));
        result = fread(block, sizeof(char), 
                        EXT2_BLOCK_SIZE / sizeof(char), native_fd);
        assert(result>=0);
    }

    return;
}

/* Given the length of a dir entry's name, returns how much space
 * the dir entry will need in total. */
unsigned int calc_d_entr_size (unsigned int name_len) {
    return (sizeof(struct ext2_dir_entry_2)) + name_len + (4 - (name_len % 4));
}

/* Given the inode of a (parent) directory, 
 * adds a new directory entry in its data block.
 * Returns a pointer to the directory entry just created.
 */
struct ext2_dir_entry_2* add_dir_entr (unsigned char* disk,
                                    struct ext2_inode* p_inode, 
                                    unsigned int inode_to_add,
                                    char* name,
                                    unsigned char type) {
    int i, quit_loop;
    quit_loop = 0;
    unsigned int t_size; // To keep track of where we are in the data block
    unsigned int spc_needed;

    struct ext2_dir_entry_2 *p_entry;
    char *t_name = pathname_final(name);
    unsigned int p_size;

    // For each non-zero ptr held in the parent directory inode...
    for(i=0; i < EXT2_NUM_DIR_PTRS && p_inode->i_block[i] && !quit_loop; i++) {

        p_entry = (struct ext2_dir_entry_2 *)(disk + 
            (p_inode->i_block[i] * EXT2_BLOCK_SIZE));
        t_size = EXT2_BLOCK_SIZE;
        
        // Traverses all directory entries to look for
        // one that's claiming more space than it needs
        while(t_size) {

            // (Actual) amt of space needed by p_entry
            p_size = (sizeof(struct ext2_dir_entry_2)) + 
                    p_entry->name_len + (4 - (p_entry->name_len % 4));

            // Amt of space needed by the entry we want to add
            spc_needed = (sizeof(struct ext2_dir_entry_2)) + 
            strlen(t_name) + (4 - (strlen(t_name) % 4));

            // If the current rec_len is longer than needed: space found!
            if(p_entry->rec_len - p_size >= spc_needed) {
                quit_loop = 1; // Breaks out of both loops
                break;
            }
            t_size -= p_entry->rec_len;
            p_entry = (struct ext2_dir_entry_2*)((char*)p_entry + 
                p_entry->rec_len);
        }
    } // TODO STILL: what if the parent inode takes up multiple data blocks?

    // Reclaims excess space from p_entry for the new dir entry
    i--; 
    struct ext2_dir_entry_2 *new_d_entry;
    if(p_inode->i_block[i]) { // New dir entry goes in parent dir's data block

        new_d_entry = (struct ext2_dir_entry_2*)((char*)p_entry + p_size);
        new_d_entry->rec_len = p_entry->rec_len - p_size;
        p_entry->rec_len = p_size;
    }
    // If there is no space in any of the parent   
    // directory's blocks, allocates a new block
    else {
        p_inode->i_block[i] = find_free_block_idx(disk);
        add_block_to_bmap(p_inode->i_block[i], disk);
        
        new_d_entry = (struct ext2_dir_entry_2 *)(disk + 
                                (p_inode->i_block[i] * EXT2_BLOCK_SIZE));
        new_d_entry->rec_len = EXT2_BLOCK_SIZE;
        p_inode->i_blocks += 2;
        p_inode->i_size += EXT2_BLOCK_SIZE;
    }   

    new_d_entry->file_type = type;
    new_d_entry->inode = inode_to_add;
    new_d_entry->name_len = strlen(t_name);
    strncpy(new_d_entry->name, t_name, new_d_entry->name_len);

    return new_d_entry;
}


/////////////////////////////////////////
// PATHNAME MANIPULATION FUNCTIONS
/////////////////////////////////////////

/* Given an absolute path 'dir_name', returns a string
 * representing the absolute path of its parent directory.
 */
char *get_pdir_name(char *dir_name) {
    
    char *p_path = malloc(sizeof(char)*strlen(dir_name));
    strncpy(p_path, dir_name, strlen(dir_name));
    int i;

    for (i = strlen(dir_name)-2; i>=0; i--) {
        if (p_path[i] == '/') {
            p_path[i+1] = '\0';
            break;
        }

    }
    return p_path;
}

/* Given an absolute path 'path', returns the "last" part 
 * of the path after the last "/".
 */
char* pathname_final(char *path){
    return &(path[strlen(get_pdir_name(path))]);
}


/////////////////////////////////////////
// INODE, DIRECTORY ENTRY, & DATA BLOCK LOOKUP
/////////////////////////////////////////

/* Given an absolute path 'dir_name', 
 * returns the corresponding directory entry */
struct ext2_dir_entry_2* find_dir_entry(char* dir_name, unsigned char* disk) {

    char *spl="/";

    struct ext2_inode* i_tbl = (struct ext2_inode*) get_itbl(disk);

    char *p_path = malloc(sizeof(char)*(strlen(dir_name)+1));
    strncpy(p_path, dir_name, strlen(dir_name));

    // The inode and directory entry currently being looked at 
    struct ext2_inode *cur_inode = &(i_tbl[EXT2_ROOT_INO-1]); // starts @ root
    struct ext2_dir_entry_2 *d_entry; 

    char *spl_path = strtok(p_path, spl); // splits the path string by '/'
    unsigned int b_num=0;   // loop counter traversing each inode's pointers
    int offset=0;   // offset into data block, for when we traverse dir entries

    // For each DIRECTORY inode, looks at each inode it points to
    while (spl_path != NULL && b_num < EXT2_INODE_PTR_LEN && 
        (cur_inode->i_mode & EXT2_S_IFDIR) && cur_inode->i_block[b_num]) {  

        d_entry = (struct ext2_dir_entry_2 *)(disk+
            (cur_inode->i_block[b_num]*EXT2_BLOCK_SIZE));
        offset=0;

        // In the current data block, looks for 
        // a directory entry that matches in name
        while (offset < EXT2_BLOCK_SIZE) {
            if(!strncmp(spl_path, extract_name(d_entry), MAX_STR_LEN)) {
                cur_inode = &(i_tbl[d_entry->inode - 1]);
                b_num=0;
                spl_path = strtok(NULL, spl);
                offset=0;
                break;
            }
            offset += d_entry->rec_len;
            d_entry = (struct ext2_dir_entry_2 *)((char *)d_entry+
                d_entry->rec_len);
        }
        // The directory entry was not found, so increment b_num
        if(offset >= EXT2_BLOCK_SIZE)
            b_num++;
    }
    // Directory entry was found
    if(!spl_path)
       return d_entry;

    return NULL;
}

/* Given an inode number, returns a pointer to the corresponding inode struct */
struct ext2_inode* inum_to_inode(unsigned int inum, unsigned char* disk) {
   
    assert(inum<=EXT2_NUM_INODES);

    struct ext2_inode* i_tbl = (struct ext2_inode*) get_itbl(disk);
    return &(i_tbl[inum - 1]);
}

/* Given an absolute path 'dir_name', returns the corresponding inode */
struct ext2_inode* find_inode(char* dir_name, unsigned char* disk) {

    // Root inode case
    if (!strncmp(dir_name,"/",strlen(dir_name)))
        return inum_to_inode(EXT2_ROOT_INO, disk);

    struct ext2_dir_entry_2 *d_entry = find_dir_entry(dir_name,disk);

    if(!d_entry)
        return NULL;

    return inum_to_inode(d_entry->inode, disk);
}

/* Given a directory entry 'd_entry', returns a
 * correct end-truncated name (based on the name_len field)
 */
char* extract_name(struct ext2_dir_entry_2* d_entry){
    int name_len = d_entry->name_len;
    char *fname = malloc(name_len*(sizeof(char)));
    strncpy(fname, d_entry->name, name_len);
    return fname;
}

/* Given an block number, returns a pointer to the the block */
unsigned char* bnum_to_block(unsigned int bnum, unsigned char *disk) {
    assert(bnum<=EXT2_BLOCKS_PER_BG);
    return disk + bnum * EXT2_BLOCK_SIZE;
}

/////////////////////////////////////////
// BITMAP SEARCHING & MANIPULATION
/////////////////////////////////////////

// Returns the index of the lowest-numbered free inode available
unsigned int find_free_inode_idx(unsigned char *disk) {
    int i = 1;
    int j = (EXT2_GOOD_OLD_FIRST_INO-1)%8;
    
    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc* gd = get_gd(disk);
    int max_pos_inodes = sb->s_inodes_count / 8;
 
    if(sb->s_free_inodes_count) {
        char *bmap = (char *)(disk + (gd->bg_inode_bitmap*EXT2_BLOCK_SIZE));
        
        while (i < max_pos_inodes) {
            while(j < 8) {
                if((int)((bmap[i] >> j)&1)==0) {
                    break;
                } 
                j++;
            } 
            if (j < 8) {
                break;
            }
            
            j = 0;
            i++;
        }
        return (i*8)+j+1;   
    }

    return 0;
}

// Returns the index of the lowest-numbered data block available
unsigned int find_free_block_idx(unsigned char *disk) {
    int i = 0;
    int j = 0;
    
    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);
    char *bitmap = (char *)(disk + (gd->bg_block_bitmap * EXT2_BLOCK_SIZE));
    int max_pos_blocks = sb->s_blocks_count / 8;
    
    if(sb->s_free_blocks_count) {
        while (i < max_pos_blocks) {
            while(j < 8){
                if((int)((bitmap[i] >> j)&1) == 0){
                    break;
                }
                j++;
            }
            if (j < 8)
                break;

            j = 0;
            i++;
        }

        return (i*8)+j+1;
    }
    return 0;
}


// Updates inode bitmap upon the allocation of a new inode
void add_inode_to_imap(unsigned int i_num, unsigned char *disk) {

    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);

    // decrease free inode count
    sb->s_free_inodes_count--;

    // get inode bitmap ptr and update
    char *bmap = (char *)(disk + (gd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    char bt = bmap[(i_num-1)/8];
    int place = (i_num-1)%8;

    bmap[(i_num-1)/8] = bt | (1<<place);
}

// Updates inode bitmap upon the deallocation of an inode
void rem_inode_from_imap(unsigned int i_num, unsigned char *disk) {

    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);

    sb->s_free_inodes_count++;

    // get inode bitmap ptr and update
    char *bmap = (char *)(disk + (gd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    char bt = bmap[(i_num-1)/8];
    int place = (i_num-1)%8;

    bmap[(i_num-1)/8] = bt & ~(1 << place);
}

// Updates data block bitmap upon the allocation of a new data block
void add_block_to_bmap(unsigned int b_num, unsigned char *disk) {
 
    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);
    
    // decrease free block count
    sb->s_free_blocks_count--;

    // get block bitmap ptr and update
    char *bitmap = (char *)(disk + (gd->bg_block_bitmap * EXT2_BLOCK_SIZE));
    char bt = bitmap[(b_num-1)/8];
    int place = (b_num-1)%8;

    bitmap[(b_num-1)/8] = bt | (1<<place);
}

// Updates data block bitmap upon the deallocation of a new data block
void rem_block_from_bmap(unsigned int b_num, unsigned char *disk) {
 
    struct ext2_super_block* sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);
    
    sb->s_free_blocks_count++;

    // get block bitmap ptr and update
    char *bitmap = (char *)(disk + (gd->bg_block_bitmap * EXT2_BLOCK_SIZE));
    char bt = bitmap[(b_num-1)/8];
    int place = (b_num-1)%8;

    bitmap[(b_num-1)/8] = bt & ~(1 << place);
}


/////////////////////////////////////////
// MISC
/////////////////////////////////////////

// Exits the program and returns a message iff cond is true
void exit_if (int cond, int err_code) {
    if(!cond)
        return;

    fprintf(stderr, "ERROR: %s\n", strerror(err_code));
    exit(err_code);
}

/* Allocates a new character array space, and copies the given character
 * array into this new space. Returns a pointer to the new copy. */
char* copy_arg (char* arg_str) {
    char *my_copy = calloc(sizeof(char),MAX_STR_LEN);
    strncpy(my_copy,arg_str,MAX_STR_LEN);
    return my_copy;
}