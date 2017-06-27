#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Looks up the specified name (name) in the specified directory (dirinumber).  
 * If found, return the directory entry in space addressed by dirEnt.  Returns 0 
 * on success and something negative on failure. 
 */
int directory_findname(struct unixfilesystem *fs, const char *name, int dirinumber, struct direntv6 *dirEnt) {
	// get inode information
	struct inode my_node;
	int err = inode_iget(fs, dirinumber, &my_node);
	if(err < 0) return -1;

	// check whether it is a dir
	int is_dir = ((my_node.i_mode & IFMT) == IFDIR);
	if(!is_dir) return -1;

	// get dir size
	int dir_size = inode_getsize(&my_node);
	if(dir_size < 0) return -1;
	if(dir_size == 0) return -1;

	// loop block, then loop entries
	int total_block_num = (dir_size - 1) / DISKIMG_SECTOR_SIZE + 1;
	for(int i = 0; i < total_block_num; i++) {		// check all blocks
		struct direntv6 entries[DISKIMG_SECTOR_SIZE / sizeof(struct direntv6)];
		int valid_bytes = file_getblock(fs, dirinumber, i, entries);
		if(valid_bytes < 0) return -1;
		int total_entry_num = valid_bytes / sizeof(struct direntv6);
		for(int j = 0; j < total_entry_num; j++) {	// check all valid entries in a block
			int cmp = strcmp(entries[j].d_name, name);
			if(cmp == 0) {
				*dirEnt = entries[j];
				return 0;	
			}
		}
	}

	// no such entry, return -1
	return -1;
}
