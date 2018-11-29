#include "lib.h"
#include <pthread.h>
#define MAX_OBJS 1000000
#define INODE_BIT_BLOCKS 31
#define OBJECT_BLOCK 22528
#define DATA_BLOCKS 256 
#define WRITE_OFFSET (int)(31+256+22528)
#define MAX_INODES 31744
#define MAX_OBJ_SIZE (int)(16*1024*1024)
#define CACHE_BLOCKS 32768

struct object{
     long id;
     long size;
     char key[48];
     int direct[4];
     int indirect[4];
};
struct object *objs;
int *bit_map_inode;
int *bit_map_data;
int *d_bit_map;
int *cache_array;
int *dirty_cache;
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
static int write_cached(struct objfs_state *objfs,char *buf,int block){
    char *cache_write=objfs->cache+((block%CACHE_BLOCKS)<<12);
    int cache_index=block/CACHE_BLOCKS;
    int cache_off=block%CACHE_BLOCKS;
    if(dirty_cache[cache_off]==1){
        write_block(objfs,WRITE_OFFSET+cache_array[cache_off]*CACHE_BLOCKS+cache_off,cache_write);    
    }
    memcpy(buf,cache_write,BLOCK_SIZE);
    cache_array[cache_off]=cache_index;
    dirty_cache[cache_off]=1;
    return 0;
}
static int read_cached(struct objfs_state *objfs,char * buf,int block){
    char *cache_write=objfs->cache+((block%CACHE_BLOCKS)<<12);
    int cache_index=block/(CACHE_BLOCKS);
    int cache_off=block/(CACHE_BLOCKS);
    if(cache_array[cache_off]!=cache_index){
        if(dirty_cache[cache_off]==1){
            write_block(objfs,WRITE_OFFSET+cache_array[cache_off]*CACHE_BLOCKS+cache_off,cache_write);     
        }
        read_block(objfs,WRITE_OFFSET+block,cache_write);
        memcpy(cache_write,buf,BLOCK_SIZE);
        cache_array[cache_off]=cache_index;
        dirty_cache[cache_off]=1;
    }
    else{
        memcpy(cache_write,buf,BLOCK_SIZE);
    }
}

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
                d_bit_map[(i*32+j)/46]=1;
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
                    d_bit_map[(i*32+j)/46]=1;
                    bit_map_inode[i]=bit_map_inode[i]^val;
                    cnt->size=0;
                    for(int l=0;l<4;l++){
                        if(cnt->direct[l]!=0){
                           int del=cnt->direct[l]-WRITE_OFFSET; 
                           int k=del/32;
                           int mo=del%32;
                           bit_map_data[k]^=(1<<mo);
                           cnt->direct[l]=0;
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
                           cnt->indirect[l]=0;
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
                    d_bit_map[(i*32+j)/46]=1;
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
    d_bit_map[(objid-2)/46]=1;
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
//                #ifdef CACHE
//                    write_cached(objfs,abc,i*32+j);
//                #else
                    write_block(objfs,WRITE_OFFSET+i*32+j,(char*)abc);
//                #endif
               // dprintf("setting %d\n",i*32+j);
                if(offie<4){
                    if(to_write->direct[offie]==0){
                        to_write->direct[offie]=WRITE_OFFSET+i*32+j;
                        to_write->size=to_write->size+size;
                    }
                    else{
                        int del=to_write->direct[offie];
                        int k=del/32;
                        int mo=del%32;
                        to_write->direct[offie]=i*32+j;
                        bit_map_data[k]^=(1<<mo);
                    }
                    return size;
                }
                offie-=4;
                int alpha,alp;
                alpha=offie/1024;
                alp=offie%1024;
                int *indirect_array;
                malloc_4k(indirect_array,BLOCK_SIZE);
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
                    indirect_array[alp]=WRITE_OFFSET+i*32+j;
//                    #ifdef CACHE
//                         write_cached(objfs,(char *)indirect_array,blk);
//                    #else
                         write_block(objfs,WRITE_OFFSET+blk,(char *)indirect_array);
//                    #endif
                    to_write->size+=size;
                    return size;
                }
                else{
  //                  #ifdef CACHE
    //                     read_cached(objfs,(char *)indirect_array,to_write->indirect[alpha]-WRITE_OFFSET);
      //              #else
                         read_block(objfs,to_write->indirect[alpha],(char *)indirect_array);
        //            #endif
                    if(indirect_array[alp]==0){
                        indirect_array[alp]=WRITE_OFFSET+32*i+j;
          //              #ifdef CACHE
            //                write_cached(objfs,(char *)indirect_array,32*i+j);
              //          #else
                            write_block(objfs,to_write->indirect[alpha],(char *)indirect_array);
                //        #endif
                        to_write->size+=size;
                    }
                    else{
                        int del=indirect_array[alp];
                        int k=del/32;
                        int mo=del%32;
                        indirect_array[alp]=WRITE_OFFSET+i*32+j;
                        bit_map_data[k]^=(1<<mo);
              //          #ifdef CACHE
                //            write_cached(objfs,(char *)indirect_array,to_write->indirect[alpha]-WRITE_OFFSET);
                  //      #else
                            write_block(objfs,to_write->indirect[alpha],(char *)indirect_array);
                    //    #endif
                    }
                    return size;
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
    for(int i=0;(i<4)&&(total_blocks>0);i++){
        if(offie>0){
            offie--;
            continue;
        }
//        #ifdef CACHE
  //           read_cached(objfs,read_array+iterator*BLOCK_SIZE,to_read->direct[i]-WRITE_OFFSET);
    //    #else
             read_block(objfs,to_read->direct[i],read_array+iterator*BLOCK_SIZE);
	  //  #endif
        total_blocks--;
        iterator++;
    }

    dprintf("total blocks=%d\n",total_blocks);
    for(int j=0;j<4;j++){
        int *temp;
        malloc_4k(temp,BLOCK_SIZE);
//        #ifdef CACHE
  //           read_cached(objfs,(char *)temp,to_read->indirect[j]-WRITE_OFFSET);
    //    #else
             read_block(objfs,to_read->indirect[j],(char *)temp);
//	    #endif
        for(int i=0;(i<1024)&&(total_blocks>0);i++){
            if(offie>0){
                offie--;
                continue;
            }
  //          #ifdef CACHE
    //             read_cached(objfs,read_array+iterator*BLOCK_SIZE,temp[i]-WRITE_OFFSET);
      //      #else
                 read_block(objfs,temp[i],read_array+iterator*BLOCK_SIZE);
	    //    #endif
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
    malloc_4k(d_bit_map,24000*4);
    malloc_4k(cache_array,CACHE_BLOCKS*4);
    malloc_4k(dirty_cache,CACHE_BLOCKS*4);
    for(int i=0;i<24000;i++){
        d_bit_map[i]=0;
    }
    for(int i=0;i<CACHE_BLOCKS;i++){
        cache_array[i]=-1;
        dirty_cache[i]=0;
    }
    char *do_it;
    malloc_4k(do_it,BLOCK_SIZE);
    for(int i=0;i<OBJECT_BLOCK;i++){
        read_block(objfs,i,(char *)(do_it));
        for(int k=0;k<4048;k++){
            *((char *)objs+i*4048+k)=do_it[k];
        }
    }
    free_4k(do_it,BLOCK_SIZE);
    for(int i=0;i<31;i++){
        read_block(objfs,i+OBJECT_BLOCK,(char *)((char *)bit_map_inode+(i)*BLOCK_SIZE));
    }
    for(int i=0;i<256;i++){
        read_block(objfs,i+OBJECT_BLOCK+31,(char *)((char *)bit_map_data+(i)*BLOCK_SIZE));
    }

    dprintf("Done objstore init\n");
    return 0;
}

/*
   Cleanup private data. FS is being unmounted
*/
int objstore_destroy(struct objfs_state *objfs){
    char *do_it;
    malloc_4k(do_it,BLOCK_SIZE);
    for(int i=0;i<OBJECT_BLOCK;i++){
        if(d_bit_map[i]==1){
            for(int k=0;k<4048;k++){
                do_it[k]=*((char *)objs+i*4048+k);
            }
            write_block(objfs,i,(char *)(do_it));
        }
    }
    free_4k(do_it,BLOCK_SIZE);
    for(int i=0;i<31;i++){
        write_block(objfs,i+OBJECT_BLOCK,(char *)((char *)bit_map_inode+(i)*BLOCK_SIZE));
    }
    for(int i=0;i<256;i++){
        write_block(objfs,i+OBJECT_BLOCK+31,(char *)((char *)bit_map_data+i*BLOCK_SIZE));
    }
    free_4k(temp,88);
    free_4k(bit_map_inode,INODE_BIT_BLOCKS*BLOCK_SIZE);
    free_4k(bit_map_data,DATA_BLOCKS*BLOCK_SIZE);
    free_4k(objs,OBJECT_BLOCK*BLOCK_SIZE);
    free_4k(d_bit_map,24000*4);
    free_4k(cache_array,CACHE_BLOCKS);
    free_4k(dirty_cache,CACHE_BLOCKS);
    dprintf("Done objstore destroy\n");
    return 0;
}

