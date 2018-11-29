#include "lib.h"
#define MAX_OBJS 1000000
#define INODE_BIT_BLOCKS 31
#define OBJECT_BLOCK 22528
#define DATA_BLOCKS 256 
#define WRITE_OFFSET (int)(31+256+22528)
#define MAX_INODES 31744
#define MAX_OBJ_SIZE (int)(16*1024*1024)

struct object{
     long id;
     long size;
     int cache_index;
     int dirty;
     char key[32];
     int direct[4];
     int indirect[4];
};
struct object *objs;
int *bit_map_inode;
int *bit_map_data;
struct object *temp;
#define malloc_4k(x,y) do{\
                         (x) = mmap(NULL, y, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);\
                         if((x) == MAP_FAILED)\
                              (x)=NULL;\
                     }while(0); 
#define free_4k(x,y) munmap((x), y)
/*
       Returns the object ID.  -1 (invalid), 0, 1 - reserved
*/
long find_object_id(const char *key, struct objfs_state *objfs){
    dprintf("find key %s\n",key);
    struct object *cnt=objs;
    for(int i=0;i<(MAX_INODES);i++){
        int val=1;
        for(int j=0;j<32;j++){
            if(bit_map_inode[i]&val){
                if(!strcmp(cnt->key,key)){
                    temp=cnt;
                    return cnt->id;
                }
            }
            cnt++;
            val=val*2;
        }
    }
    return -1;   
}

/*
    Creates a new object with obj.key=key. Object ID must be >=2.
    Must check for duplicates.
    Return value: Success --> object ID of the newly created object
                  Failure --> -1
*/
long create_object(const char *key, struct objfs_state *objfs){
    struct object *cnt=objs;
    struct object *free=NULL;
    int flag=0;
    for(int i=0;i<(MAX_INODES);i++){
        int val=1;
        for(int j=0;j<32;j++){
            if((bit_map_inode[i]&val)==0){
                free=cnt;
                free->id=i*32+j+2;
                strcpy(free->key,key);
                bit_map_inode[i]=bit_map_inode[i]|val;
                flag=1;
            }
            if(flag==1)
                break;
            cnt++;
            val=val*2;
        }
        if(flag==1)
            break;
    }
    if(!free){
        dprintf("%s: objstore full\n", __func__);
        return -1;
    }
    return free->id;
}
/*
    One of the users of the object has dropped a reference
    Can be useful to implement caching.
    Return value: Success --> 0
                  Failure --> -1
*/
long release_object(int objid, struct objfs_state *objfs){
    return 0;
}

/*
      Destroys an object with obj.key=key. Object ID is ensured to be >=2.
    
      Return value: Success --> 0
                    Failure --> -1
*/
long destroy_object(const char *key, struct objfs_state *objfs){
    struct object *cnt=objs;
    for(int i=0;i<(MAX_INODES);i++){
        int val=1;
        for(int j=0;j<32;j++){
            if(bit_map_inode[i]&val){
                if(!strcmp(cnt->key,key)){
                    bit_map_inode[i]=bit_map_inode[i]^val;
                    cnt->size=0;
                    for(int l=0;l<4;l++){
                        if(cnt->direct[l]!=0){
                           int del=cnt->direct[l]-WRITE_OFFSET; 
                           int k=del/32;
                           int mo=del%32;
                           bit_map_data[k]^=(1<<mo);
                        }
                    }
                    for(int l=0;l<4;l++){
                        if(cnt->indirect[l]!=0){
                           int del=cnt->indirect[l]-WRITE_OFFSET; 
                           int k=del/32;
                           int mo=del%32;
                           int *tem;
                           malloc_4k(tem,BLOCK_SIZE);
                           read_block(objfs,cnt->indirect[l],(char *)tem);
                           for(int al=0;al<1024;al++){
                                if(tem[al]){
                                    int del1=tem[al]-WRITE_OFFSET;
                                    int k1=del1/32;
                                    int mo1=del1%32;
                                    bit_map_data[k1]^=(1<<mo1);
                                }
                           }
                           bit_map_data[k]^=(1<<mo);
                        }
                    }
                    return 0;
                }
            }
            cnt++;
            val=val*2;
        }
    }
    return -1;   
}

/*
      Renames a new object with obj.key=key. Object ID must be >=2.
      Must check for duplicates.  
      Return value: Success --> object ID of the newly created object
                    Failure --> -1
*/

long rename_object(const char *key, const char *newname, struct objfs_state *objfs){
    struct object *cnt=objs;
    for(int i=0;i<(MAX_INODES);i++){
        int val=1;
        for(int j=0;j<32;j++){
            if(bit_map_inode[i]&val){
                if(!strcmp(cnt->key,key)){
                    strcpy(cnt->key,newname);
                    return cnt->id;
                }
            }
            cnt++;
            val=val*2;
        }
    }
    return -1;   
}

/*
      Writes the content of the buffer into the object with objid = objid.
      Return value: Success --> #of bytes written
                    Failure --> -1
*/
long objstore_write(int objid, const char *buf, int size, struct objfs_state *objfs,off_t offset){
    if(objid<2)
        return -1;
    struct object* to_write=objs+objid-2;
    char * abc;
    int offie=offset/BLOCK_SIZE;
    malloc_4k(abc,BLOCK_SIZE);
    for(int i=0;i<size;i++){
        abc[i]=buf[i];
    }
    for(int i=0;i<(DATA_BLOCKS*BLOCK_SIZE)/4;i++){
        int val=1;
        for(int j=0;j<32;j++){
            if((bit_map_data[i]&val)==0){
                bit_map_data[i]=bit_map_data[i]|val;
                write_block(objfs,WRITE_OFFSET+i*32+j,(char*)abc);
               // dprintf("setting %d\n",i*32+j);
                if(offie<4){
                    
                }
                for(int alpha=0;alpha<4;alpha++){
                    if(to_write->direct[alpha]==0){
                        to_write->direct[alpha]=WRITE_OFFSET+i*32+j;
                        to_write->size=to_write->size+size;
                        return size;
                     }
                }
                int *indirect_array;
                malloc_4k(indirect_array,BLOCK_SIZE);
                for(int alpha=0;alpha<4;alpha++){
                    if(to_write->indirect[alpha]==0){
                        int blk=-1;
                        for(int k1=0;k1<(DATA_BLOCKS*BLOCK_SIZE)/4;k1++){
                            int val1=1;
                            for(int j1=0;j1<32;j1++){
                                if((bit_map_data[k1]&val1)==0){
                                    bit_map_data[k1]|=val1;
                                    blk=k1*32+j1;
                                }
                                if(blk!=-1)
                                    break;
                                val1*=2;
                            }
                            if(blk!=-1)
                                break;
                        }
                        to_write->indirect[alpha]=WRITE_OFFSET+blk;
                        for(int al=0;al<1024;al++)
                            indirect_array[al]=0;
                        indirect_array[0]=WRITE_OFFSET+i*32+j;
                        dprintf("Indirect vlock number = %d\n",blk);
                        write_block(objfs,WRITE_OFFSET+blk,(char *)indirect_array);
                        to_write->size+=size;
                        return size;
                    }
                    else{
                        read_block(objfs,to_write->indirect[alpha],(char *)indirect_array);
                        for(int al=0;al<1024;al++){
                            if(indirect_array[al]==0){
                                indirect_array[al]=WRITE_OFFSET+32*i+j;
                                write_block(objfs,to_write->indirect[alpha],(char *)indirect_array);
                                to_write->size+=size;
                                return size;
                            }
                        }
                    }
                }
            }
            val=val*2;
        }
    }
    return -1;
    
}

/*
      Reads the content of the object onto the buffer with objid = objid.
      Return value: Success --> #of bytes written
                    Failure --> -1
*/
long objstore_read(int objid, char *buf, int size, struct objfs_state *objfs,off_t offset){
    if(objid<2)
        return -1;
    char *read_array;
    int offie=offset/BLOCK_SIZE;
    int iterator=0;
    malloc_4k(read_array,size);
    struct object* to_read=objs+objid-2;
    int total_blocks=size/(BLOCK_SIZE);
    if((size%BLOCK_SIZE)>0)
        total_blocks++;
    dprintf("total blocks=%d\n",total_blocks);
    for(int i=0;(i<4)&&(total_blocks>0);i++){
        if(offie>0){
            offie--;
            continue;
        }
        read_block(objfs,to_read->direct[i],read_array+iterator*BLOCK_SIZE);
	    total_blocks--;
        iterator++;
    }

    dprintf("total blocks=%d\n",total_blocks);
    for(int j=0;j<4;j++){
        int *temp;
        malloc_4k(temp,BLOCK_SIZE);
        read_block(objfs,to_read->indirect[j],(char *)temp);
        for(int i=0;(i<1024)&&(total_blocks>0);i++){
            if(offie>0){
                offie--;
                continue;
            }
            read_block(objfs,temp[i],(char *)((char *)read_array+(iterator)*BLOCK_SIZE));
	        total_blocks--;
            iterator++;
        }
    }    

    for(int i=0;i<size;i++)
            buf[i]=read_array[i];
    return size;
}

/*
  Reads the object metadata for obj->id = buf->st_ino
  Fillup buf->st_size and buf->st_blocks correctly
  See man 2 stat 
*/
int fillup_size_details(struct stat *buf,struct objfs_state *objfs){
	struct object *obj = temp;
	if(buf->st_ino < 2 || obj->id != buf->st_ino)
	    return -1;
	buf->st_size = obj->size;
	buf->st_blocks = obj->size >> 9;
	if(((obj->size >> 9) << 9) != obj->size)
	    buf->st_blocks++;
	return 0;
}

/*
        Set your private pointer, anyway you like.
        j   
*/
int objstore_init(struct objfs_state *objfs){
    malloc_4k(temp,88);
    malloc_4k(bit_map_inode,INODE_BIT_BLOCKS*BLOCK_SIZE);
    malloc_4k(bit_map_data,DATA_BLOCKS*BLOCK_SIZE);
    malloc_4k(objs,OBJECT_BLOCK*BLOCK_SIZE);
    /*    for(int i=0;i<31;i++){
        read_block(objfs,i,(char *)((char *)bit_map_inode+i*BLOCK_SIZE));
    }
    for(int i=31;i<287;i++){
        read_block(objfs,i,(char *)((char *)bit_map_data+i*BLOCK_SIZE));
    }*/
    for(int i=0;i<OBJECT_BLOCK;i++){
        read_block(objfs,i,(char *)((char *)objs+i*BLOCK_SIZE));
    }
    dprintf("Done objstore init\n");
    return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs){
       dprintf("Done objstore destroy\n");
       return 0;
}
