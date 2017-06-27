
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DIR_MAX_LEN 14

int helper(struct unixfilesystem *fs, int dirinumber, const char* path); 

/**
 * Returns the inode number associated with the specified pathname.  This need only
 * handle absolute paths.  Returns a negative number (-1 is fine) if an error is 
 * encountered.
 */
int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
	int cmp = strcmp(pathname, "/");
	if(cmp == 0) {
		return ROOT_INUMBER;
	} else {
		const char* path = pathname + strlen("/");	// remove the forward slash
		return helper(fs, ROOT_INUMBER, path);
	}
}

/**
 * helper function, returning the same result as pathname_lookup,
 * but in a recursive way, and without tackling root director
 */
int helper(struct unixfilesystem *fs, int dirinumber, const char* path) {
	char* slash_start = strchr(path, '/');
	if(slash_start == NULL) {		// no slash in the path
		struct direntv6 entry;
		int err = directory_findname(fs, path, dirinumber, &entry);
		if(err < 0) return -1;
		return entry.d_inumber;
	} else {						// the path has slash
		char* newpath = slash_start + strlen("/");	// remove the forward slash
		int dirlen = strlen(path) - strlen(newpath);
		char dir[DIR_MAX_LEN];		// prepare for the first dir
		strncpy(dir, path, dirlen);
		dir[dirlen- 1] = '\0';		// set the terminal char for dir
		struct direntv6 entry;
		int err = directory_findname(fs, dir, dirinumber, &entry);
		if(err < 0) return -1;
		return helper(fs, entry.d_inumber, newpath);
	}
}
