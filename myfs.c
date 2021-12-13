#define FUSE_USE_VERSION 31


// Preprocessor Directives
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h> 
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>


// File Operations Prototypes
//static void *fs_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int fs_mkdir(const char *path, mode_t mode);
static int fs_rmdir(const char *path);
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int fs_open(const char *path, struct fuse_file_info *fi);
static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fs_rm(const char *path);
//static int fs_rename(const char *from, const char *to, unsigned int flags);
//static int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi);
 

// Fuse Operations
static struct fuse_operations fs_oper = {
 	// .init 		= fs_init,
	.getattr    = fs_getattr,
    .readdir	= fs_readdir,
    .mkdir		= fs_mkdir,
    .rmdir		= fs_rmdir,
    .open       = fs_open,
    .create     = fs_create,
    .read       = fs_read,
    .write      = fs_write,
    .unlink	 	= fs_rm,
    // .rename 		= fs_rename,
    // .truncate 	= fs_truncate
};


// Structure for Inodes
typedef struct 
{
	bool used;                  // Checks the validity of the inodes, whether it is available
    int id;						// ID for the inode
    size_t size;				// Size of the file
    int data;					// Offset of data block
    bool directory;				// Checks if the entity is a Directory or a File
    int link_count; 			// Link Count: 2 -> Directory, 1 -> File
    int last_accessed;			// Last accessed time
    int last_modified;			// Last modified time
} __attribute__((packed, aligned(1))) inode;


// Structure for Directory Entry
typedef struct
{
	char filename[15];
	int file_inode;
} dirent;

// Size of dirent = 16 bytes
// Hence in a 512 B block we can have 32 directory entries


// Macros
#define BLK_SIZE (1 << 12)

#define N_INODES 100
#define DBLKS_PER_INODE 1
#define DBLKS (DBLKS_PER_INODE * N_INODES)

#define ROUND_UP_DIV(x, y) (((x) + (y) - 1) / (y))

#define FREEMAP_BLKS ROUND_UP_DIV(DBLKS, BLK_SIZE)
#define INODE_BLKS ROUND_UP_DIV(N_INODES*sizeof(inode), BLK_SIZE)
#define INODE_MAP_BLKS ROUND_UP_DIV(DBLKS, BLK_SIZE)

#define FS_SIZE (INODE_MAP_BLKS + INODE_BLKS + FREEMAP_BLKS + DBLKS) * BLK_SIZE

#define MAX_NO_OF_OPEN_FILES 10

#define DEBUG 2
 
 
// Helper Functions
int initialise_inodes(int* i);
int initialise_freemap(int* map);
int return_first_unused_inode(int* i);
int return_offset_of_first_free_datablock(int *freemap);
int isDir(char *path);
void path_to_inode(const char* path, int *ino);
void allocate_inode(char *path, int *ino, bool dir);
void print_inode(inode *i);


// Global Variables
int fs_file;
char *fs;												// The start of the FileSystem in the memory
int *inode_map;
inode *inodes;											// The start of the inode block
int *freemap;											// The start of the free-map block
char *datablks;											// The start of the data_blockss

dirent *root_directory; 								// Address of the block representing the root directory 
 
 
//-----------------------------------------------------------------------------------------MAIN (DRIVER) Function---------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	fs_file = open("MyFileSystem", O_RDWR);
	struct stat buf;
	fstat(fs_file, &buf);
	#ifdef DEBUG
	printf("MyFileSystem size = %d\n", buf.st_size);
	#endif
	fs = calloc(1, FS_SIZE);

	if(buf.st_size != 0)
	{
		read(fs_file, fs, FS_SIZE);
	}
	inode_map = (int *)fs;
	inodes = (inode *)(inode_map + INODE_BLKS * BLK_SIZE);
	freemap = (int *)(inodes + N_INODES * sizeof(inode));
	datablks = (char *)(freemap + FREEMAP_BLKS * BLK_SIZE);
	printf("fs = %u\n", fs);
	printf("inode_map = %u\n", inode_map);
	printf("inodes = %u\n", inodes);
	printf("freemap = %u\n", freemap);
	printf("datablks = %u\n", datablks);

	if(buf.st_size == 0)
	{
		initialise_inodes(inode_map);
		initialise_freemap(freemap);
	}

  	printf("Welcome!!\n\n");


	// Initialising the root directory
	root_directory = (dirent *)datablks;

	printf("root_directory = %u\n", root_directory);
	//printf("size of dirent = %d\n", sizeof(dirent));
	// Adding a welcome file to the root_directory
	//printf("%u\n%u\n", root_directory, root_directory -> filename);
  	if(buf.st_size == 0)
  	{
  		strcpy(root_directory -> filename, "Welcome");
	  	//printf("%u\n", root_directory -> filename);
	  	//printf("Here\n");
  		root_directory -> file_inode = return_first_unused_inode(inode_map);

	    inode *temp;
	    temp = inodes + ((root_directory -> file_inode) * sizeof(inode));
	  	temp -> id = 1;
	  	temp -> size = 30;
	  	temp -> data = return_offset_of_first_free_datablock(freemap);
	  	temp -> directory = false;
	  	temp -> last_accessed = 0;
	  	temp -> last_modified = 0;
	  	temp -> link_count = 1;

	  	//printf("inode of Welcome = %u\n", root_directory -> file_inode);
	    char *data_temp = (datablks + ((temp -> data)*BLK_SIZE));
	    strcpy(data_temp, "Welcome To Our File System!!!\n");

	    write(fs_file, fs, FS_SIZE);
	  	//printf("%d\n", return_offset_of_first_free_datablock(freemap));
	  	/*
	  	char *x = (char *)malloc(10);
	  	strcpy(x, "/Welcome");
	  	printf("inodes = %u\n", inodes);
	  	inode *t =  path_to_inode(x);
	  	printf("path_to_inode = \n%u\n", t);
	  	printf("Here\n");
	  	*/
	  	//printf("%d\n%d\n", inodes->used, (inodes + 1) -> used);
  	}
  	else
  	{
    	printf("File System restored!!\n");
  	}
  	return fuse_main(argc, argv, &fs_oper, NULL);
}


int initialise_inodes(int* i)
{
	// Initalise the inodes
	// return 0 on success and -1 on some error
	#ifdef DEBUG
	printf("Initialising inodes\n");
	#endif

	for(int x = 0; x < N_INODES; x++)
	{
		*i = 0;
		i ++;
	}
	return 0;
}


int initialise_freemap(int* map)
{
	// Initalise the freemap
	// return 0 on success and -1 on some error
	#ifdef DEBUG
	printf("Initialising freemap\n");
	#endif
	int x;
	for(x = 0; x < DBLKS; x++)
	{
		*(map + x) = 1;
	}
	*(map + x) = -1; // No more datablocks
	return 0;
}


//free inode function to search the inode bitmap
int return_first_unused_inode(int* i)
{
	int ix;
	for(ix = 1; ix < N_INODES; ix++,i++)
  	{
		if(*i == 0)
    	{
			*i = 1;
			return ix;
		}
	}
	return -1;
}


//free data block function to search the data bitmap
int return_offset_of_first_free_datablock(int *freemap)
{
	int i;
	for(i = 1; i < DBLKS; i++)
  	{
		//printf("%d\n", freemap[i]);
		if(freemap[i] == 1)
    	{
			freemap[i] = 0; //mark it as used now
			return (i);
		}
	}
	return -1;
}


//Parse the path to reach the correct inode using the directory entries
void path_to_inode(const char* path, int *ino)
{
	// Given the path name it will return the inode address if it exists else it returns NULL
	#ifdef DEBUG
	printf("path_to_inode - path - %s\n", path);
	#endif

  	//if path itself is root -> already have pointer to the root directory
  	//so dont search further
	if(strcmp("/", path) == 0)
  	{
		*ino = (root_directory->file_inode);
	}

  	//if not root -> start at root and search further
	else
  	{
  		char *token = strtok(path, "/");
  		dirent *temp;
  		temp = root_directory;

  		printf("root_directory -> filename = %s\n", (root_directory) -> filename);
  	  	printf("temp -> filename = %s\n", temp->filename);

      	while (token != NULL)
      	{
  			printf("token = %s\n", token);

			while((strcmp(temp -> filename, "") != 0) && (strcmp(temp -> filename, token) != 0))
          	{
				temp++;
				printf("temp = %u\n", temp);
				printf("temp -> filename = %s\n", temp->filename);
			}

        	//if reached end of the path and temp is empty => NOT FOUND
  			if(strcmp(temp -> filename, "") == 0)
        	{
  				#ifdef DEBUG
  				printf("Inode doesnt exist!! for the path %s\n", path);
  				#endif
  				*ino = -1;
  			}
  			else
        	{
          		printf("INODE FOUND !!! \n\n\n");

  				*ino = (temp -> file_inode);
          		inode *temp_ino = inodes + ((*ino) * sizeof(inode));
  				if(temp_ino -> directory)
          		{
  					temp = (dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE));
  				}

  			}
  			token = strtok(NULL, "/");
  		}
	}
}


//Traverse the path till "/", and check the dirent for pointer to inode 
//inode -> check the bool variable 
int isDir(char *path)
{

  //if root => directory
	if (strcmp("/", path) == 0)
  	{
		return 1;
	}

	else
  	{
		char *token = strtok(path, "/");
		dirent *temp;
		temp = root_directory;
	
		int dir = -1;
    	while (token != NULL)
    	{
    		while(((strcmp(temp -> filename, "") != 0) || (temp -> file_inode) != 0))
      		{
				if((strcmp(temp -> filename, "") != 0))
        		{
					if((strcmp(temp -> filename, token) == 0))
          			{
						break;
					}
				}
				temp ++;

			}

			if((strcmp(temp -> filename, "") == 0) && temp -> file_inode == 0)
      		{
				#ifdef DEBUG
				printf("Invalid Path - %s\n", path);
				#endif
				return -1;
				// Invalid path
			}

			else
      		{
        		inode *temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));

        		//if bool is true
				if(temp_ino -> directory)
        		{
					dir = 1;
				}
				else
        		{
					dir = 0;
				}

				if(dir == 1)
				{
					temp = (dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE));
				}
				// not handled : case where file/directory is there

			}
  			token = strtok(NULL, "/");
  		}

		#ifdef DEBUG
		if(dir == 0)
    	{
			printf("%s - FILE\n", path);
		}
		else if(dir == 1)
    	{
			printf("%s - DIRECTORY\n", path);
		}
		else
    	{
			printf("%s - ERROR PATH\n", path);
		}
		#endif
  		return dir;
	}
}

//Sets the parameters for the free location found in inode bit map
void allocate_inode(char *path, int *ino, bool dir)
{
	//*ino = return_first_unused_inode(inodes);
	#ifdef DEBUG
	printf("ALLOCATING INODE\n");
	printf("inode address to allocate : %u\n", *ino);
	#endif

	//position the pointer to correct address
	inode *temp_ino = inodes + ((*ino) * sizeof(inode));

	temp_ino -> id = rand() % 5000;
	temp_ino -> size = 0;
	temp_ino -> data = return_offset_of_first_free_datablock(freemap);
	temp_ino -> directory = dir;
	temp_ino -> last_accessed = 0;
	temp_ino -> last_modified = 0;

  	if(dir)
  	{
    	temp_ino -> link_count = 2;
  	}
  	else
  	{
    	temp_ino -> link_count = 1;
  	}
}


//Print inode related things
void print_inode(inode *i)
{
	printf("used : %d\n", i -> used);
	printf("id : %d\n", i -> id);
	printf("size : %d\n", i -> size);
	printf("data : %u\n", i -> data);
	printf("directory : %d\n", i -> directory);
	printf("last_accessed : %d\n", i -> last_accessed);
	printf("last_modified : %d\n", i -> last_modified);
	printf("link_count : %d\n", i -> link_count);
}

//---------------------------------------------------------------------------------------FUSE FUNCTIONS--------------------------------------------------------------------------------------------------

static int fs_getattr(const char *path, struct stat *stbuf,
  		       struct fuse_file_info *fi)
{
  	#ifdef DEBUG
  	printf("%s\n", path);
  	#endif

  	int res = 0;

  	char *path2;
  	path2 = (char *)malloc(sizeof(path));
  	strcpy(path2, path);

    //buffer to get the attributes
  	memset(stbuf, 0, sizeof(struct stat));

  	int ino;
  	path_to_inode(path, &ino); //find inode using the path

  	int directory_flag = isDir(path2); //check if it is a directory

  	if (directory_flag == 1) 
    {
  		printf("getattr - Directory\n");
  		stbuf->st_mode = S_IFDIR | 0777;
  		stbuf->st_nlink = 2;
  	}

  	else if (directory_flag == 0)
    {
      	inode *temp_ino = inodes + (ino * sizeof(inode));
  		stbuf->st_mode = S_IFREG | 0444;
  		stbuf->st_nlink = 1;
  		stbuf->st_size = temp_ino -> size;
  		stbuf->st_blocks = (int)((temp_ino->size)+512)/512;
  	}

  	else
    {
  		res = -ENOENT;
  	}
  	return res;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	#ifdef DEBUG
  	printf("ReadDir - %s\n", path);
  	#endif
  	(void) offset;
  	(void) fi;
  	
  	int ino; //inode index in the array
  	path_to_inode(path, &ino);// read the path to find the inode

  	printf("ino returned for %s - %d\n", path, ino);

    //if inode not found
  	if (ino == -1)
    {
  		return -ENOENT;
  	}

  	else
    {
  		dirent *temp;
  		filler(buf, ".", NULL, 0);
  		filler(buf, "..", NULL, 0);
  		//printf("filled some stuff\n");

      	//if directory is root
  		if(strcmp(path, "/") == 0)
      	{
  			temp = root_directory;
  		}

      	//get the dirent using inode found at inode number
  		else
      	{
        	inode *temp_ino = inodes + (ino * sizeof(inode));
  			temp = (dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE));	
  		}

      	//read all files/entries in the DIR in a loop
  		while((strcmp(temp -> filename, "") != 0))
      	{
  			//printf("I'm here\n");
  			filler(buf, temp -> filename, NULL, 0);
  			temp++;
  			//temp = (dirent *)temp
  		}
  	}
  	return 0;
}


static int fs_mkdir(const char *path, mode_t mode)
{

  	#ifdef DEBUG
  	printf("mkdir\n");
  	printf("%s\n", path);
  	printf("%d", mode);
  	#endif

    //ALLOCATE AND SET new directory

    //find free inode -> set inode parameters at index
  	int ino = return_first_unused_inode(inode_map);
  	allocate_inode(path, &ino, true);

    //access the inode
    inode *temp_ino = inodes + (ino * sizeof(inodes));
  	print_inode(temp_ino);
  	printf("inode address for %s - %u\n", path, ino);

  	// go through the path
  	char *token = strtok(path, "/");
  	dirent *temp;
  	temp = root_directory;

  	while (token != NULL)
    {
  		printf("token = %s\n", token);
  		printf("temp -> filename = %s\n", temp->filename);

  		while((strcmp(temp -> filename, "") != 0) && (strcmp((temp -> filename), token) != 0))
      	{
  			temp++;
  		}

  		if((strcmp(temp -> filename, "") == 0))
      	{
  			// Add the new directory to temp
  			//printf("HERE\n");
  			temp = (dirent *)temp;
  			//printf("temp = %u\n temp -> filename = %u\n", temp, temp -> filename);
  			//(temp -> filename) = (char *)malloc(15);
  			strcpy((temp -> filename), token);
  			temp -> file_inode = ino;

        	printf("\n\nPersisting the directory creation\n\n");
        	lseek(fs_file, 0, SEEK_SET);
        	write(fs_file, fs, FS_SIZE);
  			return 0;
  		}
  		else
      	{
        	temp_ino = (inodes + ((temp -> file_inode) * sizeof(inode)));
  			if(temp_ino -> directory)
        	{
  				temp = (dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE));
  			}
  		}
  		token = strtok(NULL, "/");
  	}
}

//remove a directory only if the directory is empty
static int fs_rmdir(const char *path)
{

	#ifdef DEBUG
	printf("\trmdir\n");
	printf("path : %s\n", path);
	#endif

	int i;
	//start from back and get the last directory
	i = strlen(path);
	while(path[i] != '/')
	{
		i--;
	}

	char *subpath;
	subpath = (char *)malloc(strlen(path)+ 1);

	//basically not including the non directory part
	strncpy(subpath, path, i);
	subpath[i] = '/';
	subpath[i+1] = '\0';

	#ifdef DEBUG
	printf("Subpath = %s\n", subpath);
	printf("Name of directory to delete = %s\n", path + (i+1));
	#endif

	int ino;
	dirent *temp;
	dirent *temp_data;
	inode *temp_ino;

	//if root 
	if(strcmp(subpath, "/") == 0)
	{
	  	temp = root_directory;
	}

	//get inode number using the original path given -> and thus the dirent
	else
	{
	  	path_to_inode(subpath, &ino);
	  	printf("Inode for the path - %s - %d\n", path, ino);

	  	temp_ino = inodes + (ino * sizeof(inode)); //get inode at offset
		temp = datablks + ((temp_ino->data) * BLK_SIZE); // get dirent using the inode
	}

	//do similar for the subpath = to check if the directory is empty or not

	printf("ino -> data = %u\n", temp);
	printf("root_directory = %u\n", root_directory);

	while(((strcmp(temp -> filename, "") != 0) || (temp -> file_inode) != 0) && strcmp(path + (i+1), temp -> filename) != 0)
	{
		temp ++;
	}

	if((strcmp(temp -> filename, "") != 0))
	{
	 	//printf("I'm here!!");
	  	printf("temp -> file_inode = %d\n", temp -> file_inode);

		if(temp -> file_inode != 0)
  		{
		    temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
		    temp_data = ((dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE)));

		    //directory has stuff
		    if(strcmp((temp_data -> filename), "") != 0)
		    {
					
      			#ifdef DEBUG
				printf("Directory isnt empty!!\n");
				#endif

				return -1;
			}

	    	//if it is empty -> free the bitmaps(inode and the databitmap)
			strcpy(temp ->filename, "");
			freemap[(temp_ino -> data)] = 1;
			inode_map[(temp -> file_inode)] = 0;

		}
	}

	printf("\n\nPersisting the directory deletion\n\n");
	lseek(fs_file, 0, SEEK_SET);
	write(fs_file, fs, FS_SIZE);

	return 0;
}


// Implemented in Copy
static int fs_rename(const char *from, const char *to, unsigned int flags)
{
	return -1;
}


// To truncate a file and write
static int fs_truncate(const char *path, off_t size,struct fuse_file_info *fi)
{
	return -1;
}


// Create new file -> for touch
static int fs_create(const char *path, mode_t mode,struct fuse_file_info *fi)
{

  	#ifdef DEBUG
  	printf("\tCreate called\n");
  	#endif

    //Finds the first free inode and initializes it
  	int ino = return_first_unused_inode(inode_map);
  	allocate_inode(path, &ino, false);

    //Root - starting point
  	char *token = strtok(path, "/");
  	dirent *temp;
  	temp = root_directory;
  	
  	char *file = (char *)malloc(15);
  	while (token != NULL)
    {
  		
      	//loop till the end of the path
  		while((strcmp(temp -> filename, "") != 0) && (strcmp(temp -> filename, token) != 0))
      	{
  			temp++;
  		}

      	//if it is end of path , attach the inode and set dirent
  		if((strcmp(temp -> filename, "") != 0))
      	{
          	inode *temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
			if(temp_ino -> directory)
          	{
				temp = datablks + ((temp_ino -> data) * BLK_SIZE);
			}
			else
          	{
				return -1;
			}
  		}

  		strcpy(file, token);
  		token = strtok(NULL, "/");
  	}

  	strcpy(temp -> filename, file);
  	temp -> file_inode = ino;
    printf("\n\nPersisting the create!!\n\n");
    lseek(fs_file, 0, SEEK_SET);
    printf("%d\n", write(fs_file, fs, FS_SIZE));
  	return 0;
}


//Traverses the path and finds the File Descriptor
static int fs_open(const char *path, struct fuse_file_info *fi)
{
	#ifdef DEBUG
	printf("Opening File - %s\n", path);
	#endif

	int ino;
	path_to_inode(path, &ino);
	printf("inode returned = %u\n", ino);

	if(ino == -1)
	{
		return -1;
	}

	#ifdef DEBUG
	printf("Successful open\n");
	#endif

	return 0;
}


static int fs_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	int ino;
	path_to_inode(path, &ino);
	size_t len;
	(void) fi;

	if(ino == -1)
		return -ENOENT;

	inode *temp_ino = (inodes + (ino * sizeof(inode)));
	len = temp_ino->size;

	if (offset < len) 
	{
		if (offset + size > len)
			size = len - offset;
  		char *temp_data = datablks + ((temp_ino -> data) * BLK_SIZE);
		memcpy(buf, temp_data + offset, size);
	} 

	else
		size = 0;

	return size;
}


static int fs_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi)
{
	#ifdef DEBUG
	printf("Write called!!\n");
	#endif

	int ino;
	path_to_inode(path, &ino);
	inode *temp_ino = inodes + (ino * sizeof(inode));
	memcpy(((datablks + ((temp_ino -> data) * BLK_SIZE)) + offset), (buf), size);
	temp_ino -> size = (temp_ino -> size) +  size;

	printf("\n\nPersisting the write\n\n");
	lseek(fs_file, 0, SEEK_SET);
	printf("%d\n", write(fs_file, fs, FS_SIZE));
	return 0;
}


// To remove a file
static int fs_rm(const char *path)
{
  	#ifdef DEBUG
  	printf("rm called\n");
  	#endif

    int i;
  	i = strlen(path);
  	while(path[i] != '/')
    {
  		i--;
  	}

  	char *subpath;
  	subpath = (char *)malloc(strlen(path)+ 1);
  	strncpy(subpath, path, i);
  	subpath[i] = '/';
  	subpath[i+1] = '\0';

  	#ifdef DEBUG
  	printf("Subpath = %s\n", subpath);
  	printf("Name of directory to delete = %s\n", path + (i+1));
  	#endif

  	int ino;
    dirent *temp;
    dirent *temp_data;
    inode *temp_ino;

    //if root
    if(strcmp(subpath, "/") == 0)
    {
      	temp = root_directory;
    }

    //original path
    else
    {
      	path_to_inode(subpath, &ino);
      	printf("Inode for the path - %s - %d\n", path, ino);

      	temp_ino = inodes + (ino * sizeof(inode));
    	temp = datablks + ((temp_ino->data) * BLK_SIZE);
    }

    //subpath

  	printf("ino -> data = %u\n", temp);
  	printf("root_directory = %u\n", root_directory);

  	while(((strcmp(temp -> filename, "") != 0) || (temp -> file_inode) != 0) && strcmp(path + (i+1), temp -> filename) != 0)
  	{
  		temp ++;
  	}

  	if((strcmp(temp -> filename, "") != 0))
    {
      	printf("I'm here!!");
      	printf("temp -> file_inode = %d\n", temp -> file_inode);
  		if(temp -> file_inode != 0)
      	{
        	temp_ino = inodes + ((temp -> file_inode) * sizeof(inode));
        	temp_data = ((dirent *)(datablks + ((temp_ino -> data) * BLK_SIZE)));
  			strcpy(temp ->filename, "");
  			//freemap[(temp_ino -> data)] = 1;
  			inode_map[(temp -> file_inode)] = 0;
  		}
  	}

    printf("\n\nPersisting the file deletion\n\n");
    lseek(fs_file, 0, SEEK_SET);
    write(fs_file, fs, FS_SIZE);
  	return 0;
}
