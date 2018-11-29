#include<context.h>
#include<memory.h>
#include<lib.h>
void clean(u32 frame){
   u64 *addr=osmap(frame);
   for(u64 i=0;i<512;i++){
        *(addr+i)=0;
   }
}
void prepare_context_mm(struct exec_context *ctx){
    u32 level4frame=os_pfn_alloc(OS_PT_REG);
    clean(level4frame);
    ctx->pgd=level4frame;

    for(int i=0;i<3;i++){
        u64 start_address,shift4,shift3,shift2,shift1,level4val,level3val,level2val,level1val;
        u32 access_code,level3frame,level2frame,level1frame;
        u64 *level4address,*level3address,*level2address,*level1address;
        
        if(i==1)
           start_address=(ctx->mms[MM_SEG_CODE]).start;
        else if(i==0)
           start_address=(ctx->mms[MM_SEG_STACK]).end-0x1000;
        else
           start_address=(ctx->mms[MM_SEG_DATA]).start;

        if(i==1)
            access_code=ctx->mms[MM_SEG_CODE].access_flags;
        else if(i==0)
            access_code=ctx->mms[MM_SEG_STACK].access_flags;
        else
            access_code=ctx->mms[MM_SEG_DATA].access_flags;
      
        u64 temp=511;
        level4address=osmap(level4frame);
        shift4=(start_address>>39);
        shift4=shift4&temp;
        level4address+=(shift4);
         
        if((*level4address)&1){
            level3frame=(*level4address)>>12;
            if(access_code&2)*level4address=(*level4address)|7;
        }
        else{
            level3frame=os_pfn_alloc(OS_PT_REG);
            clean(level3frame);
        }
        
        level3address=osmap(level3frame);
            shift3=(start_address>>30);
            shift3=shift3&temp;
            level3address+=(shift3);
            level4val=0;
            level4val^=5;
            if(access_code&2)level4val^=2;
            level4val+=(level3frame<<12);
        if((*level4address)&1);
        else
            *level4address=level4val;
       
        if((*level3address)&1){
            level2frame=(*level3address>>12);
            if(access_code&2)*level3address=(*level3address)|7;
        }
        else{
            level2frame=os_pfn_alloc(OS_PT_REG);
            clean(level2frame);
        }
        
        level2address=osmap(level2frame);
            shift2=(start_address>>21);
            shift2=shift2&temp;
            level2address+=(shift2);
            level3val=0;
            level3val^=5;
            if(access_code&2)level3val^=2;
            level3val+=(level2frame<<12);
        
            if((*level3address)&1);
        else
            *level3address=level3val;
       
        if((*level2address)&1){
            level1frame=(*level2address>>12);
            if(access_code&2)*level2address=(*level2address)|7;
        }
        else{
            level1frame=os_pfn_alloc(OS_PT_REG);
            clean(level1frame);
        }
        
        level1address=osmap(level1frame);
            shift1=(start_address>>12);
            shift1=shift1&temp;
            level1address+=(shift1); 
            level2val=0;
            level2val^=5;
            if(access_code&2)level2val^=2;
            level2val+=(level1frame<<12);
        
            if((*level2address)&1);
        else
            *level2address=level2val;
        
        if((*level1address)&1)continue; 
        level1val=0;
        level1val^=5;
        if(access_code&2)level1val^=2;
        
        if(i!=2){
            u32 level0frame=os_pfn_alloc(USER_REG);
            u32 *level0address=osmap(level0frame);
            level1val+=(level0frame<<12);
            *level1address=level1val;
        }
        else{
            u32 *level0address=osmap(ctx->arg_pfn);
            level1val+=((ctx->arg_pfn)<<12);
            *level1address=level1val;
         }
    }
    return;
}
void cleanup_context_mm(struct exec_context *ctx){
    u32 level4frame=ctx->pgd;
    u32 lev3frame[3],lev2frame[3],lev1frame[3];
    u64 *lev1store[3],*lev2store[3],*lev3store[3],*lev4store[3];
    for(int i=0;i<3;i++){
        u64 start_address,shift4,shift3,shift2,shift1,level4val,level3val,level2val,level1val;
        u32 level3frame,level2frame,level1frame,level0frame;
        u64 *level4address,*level3address,*level2address,*level1address;
        if(i==0)
           start_address=(ctx->mms[MM_SEG_CODE]).start;
        else if(i==1)
           start_address=(ctx->mms[MM_SEG_STACK]).end-0x1000;
        else
           start_address=(ctx->mms[MM_SEG_DATA]).start;
        u64 temp=511;
        level4address=osmap(level4frame);
        shift4=(start_address>>39);
        shift4=shift4&temp;
        level4address+=(shift4);
        lev4store[i]=level4address;
        
        level3frame=*level4address>>12;
        level3address=osmap(level3frame);
        shift3=start_address>>30;
        shift3=shift3&temp;
        level3address+=shift3;
        lev3store[i]=level3address;

        level2frame=*level3address>>12;
        level2address=osmap(level2frame);
        shift2=start_address>>21;
        shift2=shift2&temp;
        level2address+=shift2;
        lev2store[i]=level2address;

        level1frame=*level2address>>12;
        level1address=osmap(level1frame);
        shift1=start_address>>12;
        shift1=shift1&temp;
        level1address+=shift1;
        lev1store[i]=level1address;
        level0frame=*level1address>>12;
        
        os_pfn_free(USER_REG,level0frame);
        lev1frame[i]=level1frame;
        lev2frame[i]=level2frame;
        lev3frame[i]=level3frame;
    }
    for(int i=0;i<3;i++){
        *lev1store[i]=0;
        *lev2store[i]=0;
        *lev3store[i]=0;
        *lev4store[i]=0;
        if(lev1frame[i]==0)continue;
        os_pfn_free(OS_PT_REG,lev1frame[i]);
        for(int j=i+1;j<3;j++){
            if(lev1frame[j]==lev1frame[i])lev1frame[j]=0;
        }
    }
    for(int i=0;i<3;i++){
        if(lev2frame[i]==0)continue;
        os_pfn_free(OS_PT_REG,lev2frame[i]);
        for(int j=i+1;j<3;j++){
            if(lev2frame[j]==lev2frame[i])lev2frame[j]=0;
        }
    }
    for(int i=0;i<3;i++){
        if(lev3frame[i]==0)continue;
        os_pfn_free(OS_PT_REG,lev3frame[i]);
        for(int j=i+1;j<3;j++){
            if(lev3frame[j]==lev3frame[i])lev3frame[j]=0;
        }
    }
    os_pfn_free(OS_PT_REG,level4frame);
    return;
}
