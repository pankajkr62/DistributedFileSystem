#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include "helper.h"
#include <sys/stat.h>


#define BUFFER_SIZE (1000)

// global file descriptor 
int gfd = -1; 

MFS_CheckpointReg_t* checkpoint = NULL;

const char entry1[2] = ".";
const char entry2[3] = "..";
const char entry3[1] = "";

int Lookup(int pinum,char *name){

    // given valid pinum , now find tthe imap 
    // piece from checkpoint region
    int pimap = pinum/16;
    if(checkpoint->inodemap[pimap] == -1){
        //printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    MFS_imap_t imap_p;
    pread(gfd,&imap_p,sizeof(MFS_imap_t),checkpoint->inodemap[pimap]);
    // now find the index in imap piece 
    int index = pinum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        //printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now , read the parent inode 
    inode_t parent;
    pread(gfd,&parent,sizeof(inode_t),imap_p.inodmap[index]);
    // now you got the inode 
    if(parent.type != MFS_DIRECTORY){
        //printf(" Parent is not a direcory ");
        return -1;
    } 

    // now we have to iterate through all parent datablocks
    // to check the mapping. 
    char tempbuff[MFS_BLOCK_SIZE];
    for(int i = 0;i<14 && parent.dpointer[i] != -1;i++){
        pread(gfd,tempbuff,MFS_BLOCK_SIZE,parent.dpointer[i]);

        directoryData_t *currDirContents = (directoryData_t*) tempbuff;
        // this will contain entries 
        // I have take max 14 directory contents only 
        for(int d = 0;d<128;d++){
            MFS_DirEnt_t* cdir = &currDirContents->dirfiles[d];
            if(strcmp(cdir->name,name) == 0){
                return cdir->inum;
            }
        }
    }
    return -1;
}


int Stat(int inum,MFS_Stat_t* m){

    // given valid pinum , now find tthe imap 
    // piece from checkpoint region
    int pimap = inum/16;
    if(checkpoint->inodemap[pimap] == -1){
        printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    MFS_imap_t imap_p;
    pread(gfd,&imap_p,sizeof(MFS_imap_t),checkpoint->inodemap[pimap]);
    // now find the index in imap piece 
    int index = inum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now , read the parent inode 
    inode_t parent;
    pread(gfd,&parent,sizeof(inode_t),imap_p.inodmap[index]);
    // now you got the inode , read the contents and send 
    m->size = parent.size;
    m->type = parent.type;
    
    return 0; // on sucess 
}

int Read(int inum, char *buffer, int block){

    // both are valid 
    int pimap = inum/16;
    if(checkpoint->inodemap[pimap] == -1){
        printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    //int imap_p[16];
    MFS_imap_t imap_p;
    pread(gfd,&imap_p,16*sizeof(int),checkpoint->inodemap[pimap]);
    // now find the index in imap piece 
    int index = inum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now you got the inode address
    // read the inode 
    inode_t curr;
    pread(gfd,&curr,sizeof(inode_t),imap_p.inodmap[index]);

    // you got the inode, now read the block in buffer
    pread(gfd,buffer,MFS_BLOCK_SIZE,curr.dpointer[block]);
    return 0;
}


int Write(int inum, char *buffer, int block){

    MFS_imap_t imap_p;
    
    int imap_piece = checkpoint->inodemap[inum/16];

    if(imap_piece == -1)
        return -1;
    
    pread(gfd,&imap_p,sizeof(MFS_imap_t),imap_piece);

    int tinode = imap_p.inodmap[inum%16];
    if(tinode == -1)
        return -1;
    inode_t curr;
    pread(gfd,&curr,sizeof(inode_t),tinode);
    if(curr.type )
    if(curr.type != MFS_REGULAR_FILE){
        //printf(" not a regular file");
        return -1;
    }

    // make the block  to write 
    char *currt = buffer;
    char segment[4096];
    for(int i  =0;i<4096;i++){
        if(currt != NULL){
            segment[i] = *currt;
            currt += 1;
        }
        else
            segment[i] = '\0';
    }
    int data_offset = checkpoint->disk_pointer;
    pwrite(gfd,segment,MFS_BLOCK_SIZE,data_offset);
    checkpoint->disk_pointer += MFS_BLOCK_SIZE;

    // new inode 
    inode_t newInode;
    newInode.type = curr.type;
    if(curr.size > (block+1)*4096)
        newInode.size = curr.size;
    else{
        newInode.size = (block+1)*4096;
    }

    for(int i = 0;i<14;i++){
        newInode.dpointer[i] = curr.dpointer[i];
        if(i == block)
            newInode.dpointer[block] = data_offset;

    }

    int inode_offset = checkpoint->disk_pointer;
    checkpoint->disk_pointer += sizeof(inode_t);
    pwrite(gfd,&newInode,sizeof(inode_t),inode_offset);

    // now, we hvae to update the imap 
    MFS_imap_t newimap;
    for(int i=0;i<16;i++){
    newimap.inodmap[i] = imap_p.inodmap[i];
    if(i == inum%16)
        newimap.inodmap[i] = inode_offset; 
    }

    // write the new imap 
    int imap_offset = checkpoint->disk_pointer;
    checkpoint->disk_pointer += sizeof(MFS_imap_t);
    pwrite(gfd,&newimap,sizeof(MFS_imap_t),imap_offset);

    // now update the checkpoint
    checkpoint->inodemap[inum/16] = imap_offset;
    pwrite(gfd,checkpoint,sizeof(checkpoint),0);

    fsync(gfd);
    return 0;
}

int create(int pinum, int type, char* name)
{
    int filepointer_map = -1;
    MFS_imap_t map;

    int i,j,k,l;

    if(Lookup(pinum, name) != -1) 
    {
        return 0;
    }

    k = pinum / 16;
    filepointer_map = checkpoint->inodemap[k];
    l = pinum % 16;

    MFS_imap_t imap_par;
    pread(gfd, &imap_par, sizeof(MFS_imap_t),filepointer_map);

    int filepointer_nd = imap_par.inodmap[l];
    
    inode_t node_par;
    pread(gfd, &node_par, sizeof(inode_t),filepointer_nd);


    if(node_par.type != MFS_DIRECTORY)
    {
        if(pinum ==0 && (type ==1))
        {
            //random comment
        }      
        else
        {
            return -1;
        }
    }
    int free_inum_val = -1;
    int is_free_inum = 0;


    int offset;
    int step;
    i=0;
    while(i<256) {
        
        filepointer_map = checkpoint->inodemap[i];

        if(filepointer_map != -1) 
        {
            MFS_imap_t imap_par;
            pread(gfd, &imap_par, sizeof(MFS_imap_t),filepointer_map);

            for(j=0; j<16; j++)
            {
                filepointer_nd = imap_par.inodmap[j];
                if(filepointer_nd == -1)
                {
                    free_inum_val = i*16 + j;
                    is_free_inum = 1;
                    break;
                }
            } 
        }
        else
        {
            MFS_imap_t map_new;

            for(j =0; j< 16;j++)
            {
                map_new.inodmap[j] = -1;
            }
            offset = checkpoint->disk_pointer;
            step = sizeof(MFS_imap_t);
            checkpoint->disk_pointer += step;

            pwrite(gfd, &map_new, step,offset);

            checkpoint->inodemap[i] = offset;
            pwrite(gfd, checkpoint, sizeof(MFS_CheckpointReg_t),0);

            fsync(gfd);

            filepointer_nd = map_new.inodmap[0];
            free_inum_val = i*16;
            is_free_inum = 1;
            break;
        }
        
        if (is_free_inum) 
        {
            break;
        }

        i++;
    }

    char data_buffer[MFS_BLOCK_SIZE]; 
    directoryData_t* directory_buffer = NULL;
    int flag_found_entry = 0;
    int block_par = 0;

    inode_t* par_nd = &node_par;

    int filepointer_blk;
    for(i=0; i< 14; i++) 
    {   
        filepointer_blk= par_nd->dpointer[i];
        block_par = i;

        if(filepointer_blk == -1)
        {
            directoryData_t* par_dir = (directoryData_t*) data_buffer;
            
            for(i=0; i< 128; i++)
            {
                memcpy(par_dir->dirfiles[i].name, entry3,strlen(entry3));
                par_dir->dirfiles[i].inum = -1;
            }
        

            offset = checkpoint->disk_pointer;
            step = MFS_BLOCK_SIZE;
            checkpoint->disk_pointer += step; 
            pwrite(gfd, par_dir, sizeof(directoryData_t),offset);

            filepointer_blk = offset;

            inode_t node_dir_new;
            node_dir_new.size = node_par.size;
            node_dir_new.type = MFS_DIRECTORY;

            for (int i = 0; i < 14; i++)
            {
                node_dir_new.dpointer[i] = node_par.dpointer[i];
            }

            node_dir_new.dpointer[block_par] = offset;
        
            par_nd = &node_dir_new;

            offset = checkpoint->disk_pointer;
            step = sizeof(inode_t);
            checkpoint->disk_pointer += step;
            pwrite(gfd, &node_dir_new, step,offset);

            MFS_imap_t map_dir_new; 

            for(int i = 0; i< 16; i++) 
            {
                map_dir_new.inodmap[i] = imap_par.inodmap[i];
            }
            map_dir_new.inodmap[l] = offset; 

            offset = checkpoint->disk_pointer;
            step = sizeof(MFS_imap_t);
            checkpoint->disk_pointer += step;
            pwrite(gfd, &map_dir_new, step,offset);

            checkpoint->inodemap[k] = offset;
            pwrite(gfd, checkpoint, sizeof(MFS_CheckpointReg_t),0);

            fsync(gfd);
        }

        pread(gfd, data_buffer, MFS_BLOCK_SIZE,filepointer_blk);

        directory_buffer = (directoryData_t*)data_buffer;
        for(j=0; j<128; j++)
        {
            MFS_DirEnt_t* parent_de = &directory_buffer->dirfiles[j];
            if(parent_de->inum == -1) 
            {
                parent_de->inum = free_inum_val;
                memcpy(parent_de->name,name,strlen(name)+1);
                flag_found_entry = 1;
                break;
            }
        }
        if(flag_found_entry)
        {
            break;
        }
    }
    if(flag_found_entry == 0)
        return -1;
    char write_buffer[ MFS_BLOCK_SIZE ];
    for(i=0; i<MFS_BLOCK_SIZE; i++) 
    {
        write_buffer[i] = '\0';
    }


    int inum = free_inum_val;
    int is_old_map_found = 0;


    if(type == MFS_DIRECTORY)
    {

        // create the directory block;
        directoryData_t* par_dir = (directoryData_t*) write_buffer;

        for(int i = 0;i<128;i++){
            if(i == 0){
                // root dir
                memcpy(par_dir->dirfiles[i].name,entry1,strlen(entry1)+1);
                par_dir->dirfiles[i].inum = inum;
            }
            else if(i == 1){
                // again root
                memcpy(par_dir->dirfiles[i].name,entry2,strlen(entry2)+1);
                par_dir->dirfiles[i].inum = pinum;
            }
            else{
                memcpy(par_dir->dirfiles[i].name,entry3,strlen(entry3)+1);
                par_dir->dirfiles[i].inum = -1;
            }
        }
        
    }
    offset = checkpoint->disk_pointer;
    step = MFS_BLOCK_SIZE;
    checkpoint->disk_pointer += step;
    pwrite(gfd, directory_buffer, sizeof(directoryData_t),offset);

    inode_t node_par_new;
    node_par_new.size = par_nd->size;
    node_par_new.type = MFS_DIRECTORY;
    for (i = 0; i < 14; i++)
    {
        node_par_new.dpointer[i] = par_nd->dpointer[i];
    }  
    node_par_new.dpointer[block_par] = offset; 

    offset = checkpoint->disk_pointer;
    step = sizeof(inode_t);
    checkpoint->disk_pointer += step;
    pwrite(gfd, &node_par_new, step,offset);


    MFS_imap_t map_par_new;
    for(i = 0; i< 16; i++) 
    {
        map_par_new.inodmap[i] = imap_par.inodmap[i]; 
    }
    map_par_new.inodmap[l] = offset;

    offset = checkpoint->disk_pointer;
    step = sizeof(MFS_imap_t);
    checkpoint->disk_pointer += step;

    pwrite(gfd, &map_par_new, step,offset);

    checkpoint->inodemap[k] = offset;
    pwrite(gfd, checkpoint, sizeof(MFS_CheckpointReg_t),0);

    fsync(gfd);

    if(type == MFS_DIRECTORY)
    {
        offset = checkpoint->disk_pointer;
        step = MFS_BLOCK_SIZE;
        checkpoint->disk_pointer += step;
        pwrite(gfd, write_buffer, step,offset );
    }

    k = inum / 16; 
    filepointer_map =  checkpoint->inodemap[k];

    MFS_imap_t map_new;
    if(filepointer_map != -1)
    {
        is_old_map_found =1;

        l = inum % 16;
        pread(gfd, &map, sizeof(MFS_imap_t),filepointer_map);
        filepointer_nd = map.inodmap[l];

        for(i = 0; i< 16; i++) 
        {
            map_new.inodmap[i] = map.inodmap[i];
        }
    }
    inode_t node_new;
    node_new.size = 0;
    node_new.type = type;
    for (i = 0; i < 14; i++)
    {
        node_new.dpointer[i] = -1;
    }

    if(type == MFS_DIRECTORY)
    {
        node_new.dpointer[0] = offset;
    }

    offset = checkpoint->disk_pointer;
    step = sizeof(inode_t);
    checkpoint->disk_pointer += step;
    pwrite(gfd, &node_new, step ,offset);



    
    if(is_old_map_found == 0){
        for(i = 0; i< 16; i++) 
        {
            map_new.inodmap[i] = -1 ; 
        }
    } 

    map_new.inodmap[l] = offset; 
    offset = checkpoint->disk_pointer;
    step = sizeof(MFS_imap_t);
    checkpoint->disk_pointer += step;
    pwrite(gfd, &map_new, step ,offset);


    checkpoint->inodemap[k] = offset;
    pwrite(gfd, checkpoint, sizeof(MFS_CheckpointReg_t),0);
    fsync(gfd);
    return 0;
}

int Unlink(int pinum, char *name){

    // check for the directory using lookup function 
    int dir_inum = Lookup(pinum,name);
    if(dir_inum == -1){
        // directory name not present 
        // still return Success as per description
        return 0;
    }

    // checkpoint -> imap_piece
    int imap_p_loc = checkpoint->inodemap[dir_inum/16];
    MFS_imap_t imap_p;
    pread(gfd,&imap_p,sizeof(MFS_imap_t),imap_p_loc);
    // check if it the only entry also
    int only_entry_in_imap_piece = 1;
    for(int i = 0;i<16;i++){
        if(i != (dir_inum%16) && imap_p.inodmap[i] != -1){
            only_entry_in_imap_piece = 0;
            break;
        }
    }

    // now we have the imap, now get the inode 
    inode_t fdir_inode;
    int fdir_inode_loc = imap_p.inodmap[dir_inum%16];
    pread(gfd,&fdir_inode,sizeof(inode_t),fdir_inode_loc);
    

    // If we have to delete directory 
    // check whether it is empty directory or not 
    int empty_dir_check = 1;
    if(fdir_inode.type == MFS_DIRECTORY){
        directoryData_t* dir_content;
        for(int i =0;i<14;i++){
            if(fdir_inode.dpointer[i] != -1){
                // read each non-empty data block 
                // in one of the directoris, it will containd "." and ".."
                char data_block[4096];
                pread(gfd,data_block,MFS_BLOCK_SIZE,fdir_inode.dpointer[i]);

                dir_content = (directoryData_t*) data_block;
                // now we got the directory content
                for(int d  = 0;d<128;d++){
                    MFS_DirEnt_t* temp = &dir_content->dirfiles[d];
                    if(temp->inum == -1 || temp->inum == pinum || temp->inum == dir_inum)
                        continue;
                    else{
                        // not an empty directoey
                        empty_dir_check = 0;
                        break;
                    }
                }
                if(empty_dir_check == 0)
                    break;
            }    
        }
    }
    if(empty_dir_check == 0){
        return -1;
    }

    // nnow , since it is non-empty
    // we have to remove all th e data pointers in it 
    for(int i =0;i<14;i++){
        fdir_inode.dpointer[i] = -1;
    }
    // now, we have to remove/unlink the imap
    MFS_imap_t new_imap_piece;
    if(only_entry_in_imap_piece == 0){
        // we may need not update the imap
        // just update the checkpoint region to -1
        // this should unlink the inode 
        checkpoint->inodemap[dir_inum/16] = -1;
    }
    else{
        //here , we have other directories/files in too imap piece
        // just make a new imap with file/dir component as -1
        for(int i = 0;i<16;i++){
            if(i == dir_inum%16){
                new_imap_piece.inodmap[i] = -1;
            }
            else{
                new_imap_piece.inodmap[i] = imap_p.inodmap[i];
            }
        }
        // copy the new imap piece at log end 
        int log_end = checkpoint->disk_pointer;
        pwrite(gfd,&new_imap_piece,sizeof(MFS_imap_t),log_end);
        // update the checkpoint diskpointer
        checkpoint->disk_pointer += (int)sizeof(MFS_imap_t);
    }
    // write the updated checkpoint
    pwrite(gfd,checkpoint,sizeof(MFS_CheckpointReg_t),0);
    fsync(gfd);

    // we also have to make some small update in parent node
    // parent data block update , make data entry as -1
    int imap_par_loc = checkpoint->inodemap[pinum/16];
    MFS_imap_t imap_par;
    pread(gfd,&imap_par,sizeof(MFS_imap_t),imap_par_loc);

    // now we have the imap, now get the inode 
    inode_t pardir_inode;
    int pardir_inode_loc = imap_par.inodmap[pinum%16];
    pread(gfd,&pardir_inode,sizeof(inode_t),pardir_inode_loc);

    //char par_data_block[4096];
    directoryData_t* par_dir_content;
    int found = 0;
    int dci = 0;
    while(dci < 14 && found == 0){
        if(pardir_inode.dpointer[dci] != -1){
            // read each non-empty data block 
            // in one of the directoris, it will containd "." and ".."
            char data_block[4096];
            pread(gfd,data_block,MFS_BLOCK_SIZE,pardir_inode.dpointer[dci]);
            par_dir_content = (directoryData_t*) data_block;
            // now we got the directory content
            for(int d  = 0;d<128;d++){
                MFS_DirEnt_t* temp = &par_dir_content->dirfiles[d];
                const char nullt[9] = "nullfile";
                if(temp->inum == dir_inum){
                    memcpy(temp->name,nullt,strlen(nullt)+1);
                    temp->inum = -1; 
                    found = 1;
                    break;
                }
            }
        }
        dci += 1;
    }
    // copy the data block to the end of log 
    int db_eol = checkpoint->disk_pointer;
    pwrite(gfd,par_dir_content,sizeof(directoryData_t),db_eol);
    checkpoint->disk_pointer += MFS_BLOCK_SIZE;

    // now update the inode of the parent
    inode_t new_par_inode;
    new_par_inode.type = pardir_inode.type;
    // reduction of one block
    new_par_inode.size -= MFS_BLOCK_SIZE;
    if(new_par_inode.size < 0)
        new_par_inode.size = 0;
    for(int i =0;i<14;i++){
        new_par_inode.dpointer[i] = pardir_inode.dpointer[i];
        if(i == dci-1)
            new_par_inode.dpointer[i] = db_eol;
    }

    // write the new parent inode
    int new_par_inode_eol = checkpoint->disk_pointer;
    pwrite(gfd,&new_par_inode,sizeof(inode_t),checkpoint->disk_pointer);

    checkpoint->disk_pointer += (int)sizeof(inode_t);

    // make a new imappiece containing updated 
    MFS_imap_t new_par_imap;
    for(int i=0;i<16;i++){
        if(i == pinum%16){
            new_par_imap.inodmap[i] = new_par_inode_eol;
        }
        else{
            new_par_imap.inodmap[i] = imap_par.inodmap[i];
        }
    }

    // copy to end 
    int new_par_imap_eol = checkpoint->disk_pointer;
    pwrite(gfd,&new_par_imap,sizeof(MFS_imap_t),new_par_imap_eol);
    checkpoint->disk_pointer += (int)sizeof(MFS_imap_t);

    checkpoint->inodemap[pinum/16] = new_par_imap_eol;
    pwrite(gfd,checkpoint,sizeof(MFS_CheckpointReg_t),0);
    fsync(gfd);
    return 0;
}

int Shutdown(){
    fsync(gfd);
    exit(0);
}


int server_fs_initialize(int port, char *filesystem){
    checkpoint = (MFS_CheckpointReg_t *)malloc(sizeof(MFS_CheckpointReg_t));

    gfd = open(filesystem, O_RDWR|O_CREAT, S_IRWXU);
    MFS_CheckpointReg_t *temp= (MFS_CheckpointReg_t *)malloc(sizeof(MFS_CheckpointReg_t));
    
    pread(gfd, temp, sizeof(MFS_CheckpointReg_t),0);
    if(temp->disk_pointer > 0){
        pread(gfd, checkpoint, sizeof(MFS_CheckpointReg_t),0);
    }
    else{
        // need to create system image file at that location 
        int gfs = open(filesystem, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
        checkpoint->disk_pointer = sizeof(MFS_CheckpointReg_t);

        int tot_inodes_piece = (int)(4096/16);

        for(int i  =0;i<tot_inodes_piece;i++)
            checkpoint->inodemap[i] = -1;


        // make an data block for the root directory
        directoryData_t rootdata;
        for(int i = 0;i<128;i++){
            if(i == 0){
                // root dir
                memcpy(rootdata.dirfiles[i].name,entry1,strlen(entry1)+1);
                rootdata.dirfiles[i].inum = 0;
            }
            else if(i == 1){
                // again root
                memcpy(rootdata.dirfiles[i].name,entry2,strlen(entry2)+1);
                rootdata.dirfiles[i].inum = 0;
            }
            else{
                memcpy(rootdata.dirfiles[i].name,entry3,strlen(entry3)+1);
                rootdata.dirfiles[i].inum = -1;
            }
        }

        pwrite(gfs,&rootdata,sizeof(directoryData_t),checkpoint->disk_pointer);
        int data_offset = checkpoint->disk_pointer;
        checkpoint->disk_pointer += MFS_BLOCK_SIZE;

        // make an inode block for root directory 
        inode_t rootinode;
        rootinode.type = MFS_DIRECTORY;
        rootinode.size = 0;
        for(int i = 0;i<14;i++){
            rootinode.dpointer[i] = -1;
            if(i == 0){
                rootinode.dpointer[i] = data_offset;
            }
        }

        pwrite(gfs,&rootinode,sizeof(inode_t),checkpoint->disk_pointer);
        int inode_offset = checkpoint->disk_pointer;
        checkpoint->disk_pointer += sizeof(inode_t);

        // now create an imap piece 
        MFS_imap_t rootimap;
        for(int i  = 0;i<16;i++){
            rootimap.inodmap[i] = -1;
            if(i == 0){
                rootimap.inodmap[i] = inode_offset;
            }
        }

        pwrite(gfs,&rootimap,sizeof(MFS_imap_t),checkpoint->disk_pointer);
        int imap_offset = checkpoint->disk_pointer;
        checkpoint->disk_pointer += sizeof(MFS_imap_t);

        // update the checkpoint table 
        checkpoint->inodemap[0] = imap_offset; 
        pwrite(gfs,checkpoint,sizeof(MFS_CheckpointReg_t),0);
        fsync(gfs);
    }
    
    int sd = UDP_Open(port);
    if(sd < 0){
        return -1;
    }


    struct sockaddr_in s;
    MSG_t rec_msg;
    MSG_t read_msg;

    while (1) {
        if( UDP_Read(sd, &s, (char *)&read_msg, sizeof(MSG_t)) < 1){
            continue;
        }

        // lookup
        if(read_msg.req_type == 1){
            rec_msg.req_type = 1;
            rec_msg.inum = Lookup(read_msg.inum, read_msg.name);
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));

        }
        else if(read_msg.req_type == 2){
            // stat
            rec_msg.inum = Stat(read_msg.inum, &(rec_msg.statinfo));
            rec_msg.req_type = 2;
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
        }
        else if(read_msg.req_type == 3){
            // write 
            rec_msg.req_type = 3;
            rec_msg.inum = Write(read_msg.inum, read_msg.buffer, read_msg.block);
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
        }
        else if(read_msg.req_type == 4){
            // read
            rec_msg.req_type = 4;
            rec_msg.inum = Read(read_msg.inum, rec_msg.buffer, read_msg.block);
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
        }
        else if(read_msg.req_type == 5){
            // creat
            rec_msg.req_type = 5;
            rec_msg.inum = create(read_msg.inum, read_msg.type, read_msg.name);
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
        }
        else if(read_msg.req_type == 6){
            // unlink
            rec_msg.req_type = 6;
            rec_msg.inum = Unlink(read_msg.inum, read_msg.name);
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
        }
        else if(read_msg.req_type == 7) {
            rec_msg.req_type = 7;
            UDP_Write(sd, &s, (char*)&rec_msg, sizeof(MSG_t));
            Shutdown();
        }
        else {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]){
    if(argc != 3){
        // printf(" not enough arguments");
        exit(1);
    }

    server_fs_initialize(atoi ( argv[1] ) ,argv[2] );
    return 0;
}
