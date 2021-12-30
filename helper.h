#ifndef __HELPER_h__
#define __HELPER_h__



// other definitons 
#define total_inodes (4096)


typedef struct __inode_t {
    int size;
    int type;
    int dpointer[14];
} inode_t;

typedef struct __MFS_imap_t {
    int inodmap[16];
} MFS_imap_t;

typedef struct __MFS_CheckpointReg_t {
    int disk_pointer;
    int inodemap[256];    // 4096 ( total number of inodes) / 16 (imap piece size)
} MFS_CheckpointReg_t;


typedef struct __directoryData_t {
    //MFS_DirEnt_t dirfiles[14];
    MFS_DirEnt_t dirfiles[128];
} directoryData_t;


typedef struct __MSG_t {
    // this is actually an union of all the 
    // input fields in the MFS function.
    int inum;    // inode number 
    int type;    // type of file : file or directory 
    char name[28]; // name as in file name or directory name : 28 bytes max 
    char buffer[4096]; // data buffer : 4096 bytes 
    int block;
    MFS_Stat_t statinfo;
    int req_type; // type of request or indirectly what functiSon to call :
} MSG_t;

// '''
// Req-type 
// int MFS_init(int , char* ); --- 0
// int MFS_lookup(int, char* );   -- 1
// int MFS_stat(int , MFS_Stat_t* );   -- 2
// int MFS_write(int , char* , int );   -- 3
// int MFS_read(int , char* , int );   --- 4
// int MFS_creat(int , int , char* );   --- 5 
// int MFS_unlink(int , char* );   --- 6 
// int MFS_shutdown();  --- 7
// '''


#endif // __HELPER_h__