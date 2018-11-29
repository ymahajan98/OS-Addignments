#include<init.h>
#include<lib.h>
#include<memory.h>
#include<context.h>

int isvalid(u64 address,u32 frame){
    u64 shift;
    u64 temp=511;
    u64 * addr1;
    addr1=osmap(frame);
    shift=(address>>39)&temp;
    addr1+=shift;
    if(*addr1&1);
    else return -1;
    frame=*addr1>>12;

    addr1=osmap(frame);
    shift=(address>>30)&temp;
    addr1+=shift;
    if(*addr1&1);
    else return -1;
    frame=*addr1>>12;

    addr1=osmap(frame);
    shift=(address>>21)&temp;
    addr1+=shift;
    if(*addr1&1);
    else return -1;
    frame=*addr1>>12;
    
    addr1=osmap(frame);
    shift=(address>>12)&temp;
    addr1+=shift;
    if(*addr1&1);
    else return -1;
    return 1;
}
void free1(u64 address,u32 frame){
    u64 shift;
    u64 temp=511;
    u64 * addr1;
    addr1=osmap(frame);
    shift=(address>>39)&temp;
    addr1+=shift;
    if(!((*addr1)&1))return;
    frame=(*addr1)>>12;

    addr1=osmap(frame);
    shift=(address>>30)&temp;
    addr1+=shift;
    if(!((*addr1)&1))return;
    frame=(*addr1)>>12;

    addr1=osmap(frame);
    shift=(address>>21)&temp;
    addr1+=shift;
    if(!((*addr1)&1))return;
    frame=(*addr1)>>12;
    
    addr1=osmap(frame);
    shift=(address>>12)&temp;
    addr1+=shift;
   
    if((*addr1)&1){
        frame=(*addr1)>>12;
        os_pfn_free(USER_REG,frame);
        *addr1=0;
        asm volatile("invlpg (%0);" : : "r" (address) : "memory");
        return; 
    }
    else
        return;
}
/*System Call handler*/
long do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);
    switch(syscall)
    {
          case SYSCALL_EXIT:
                              printf("[GemOS] exit code = %d\n", (int) param1);
                              do_exit();
                              break;
          case SYSCALL_GETPID:
                              printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
                              return current->id;      
          case SYSCALL_WRITE:{  
                                if(param2> 1024)return -1;
                                u32 frame=current->pgd;
                                int z1=isvalid(param1,frame);
                                int z2=isvalid(param1+param2-1,frame);
                                if(z1==-1||z2==-1)
                                    return -1;
                                for(u64 i=0;i<param2;i++){
                                    char *print_var=(char*)param1;
                                    printf("%c",*(print_var+i));
                                }
                                return param2;
                             }
          case SYSCALL_EXPAND:{ 
                                  if(param2==MAP_RD){
                                    param1*=4096;
                                    u64 start=current->mms[MM_SEG_RODATA].next_free;
                                    u64 end=current->mms[MM_SEG_RODATA].end;
                                    if(start+param1>end)return NULL;
                                    current->mms[MM_SEG_RODATA].next_free+=param1;
                                    return start;
                                  }
                                  else{
                                    param1*=4096;
                                    u64 start=current->mms[MM_SEG_DATA].next_free;
                                    u64 end=current->mms[MM_SEG_DATA].end;
                                    if(start+param1>end)return NULL;
                                    current->mms[MM_SEG_DATA].next_free+=param1;
                                    return start;
                                  }
                             }
          case SYSCALL_SHRINK:{  
                                u64 var;
                                u32 frame=current->pgd;
                                if(param2==MAP_RD)
                                    var=MM_SEG_RODATA;
                                else
                                    var=MM_SEG_DATA;
                                u64 curr=current->mms[var].next_free;
                                u64 start=current->mms[var].start;
                                if((curr-param1*4096)<start)
                                    return NULL;
                                for(u64 i=0;i<param1;i++){
                                    free1(curr-(4096*(i+1)),frame);
                                }
                                current->mms[var].next_free-=param1*4096;
                                return current->mms[var].next_free;
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void){
    u64 *rip; 
    asm volatile( "mov %%rbp, %0" : "=rm" ( rip ));
    printf("Div-by-zero detected at [%x]\n",*(rip+1));
    do_exit();
    return 0;
}

extern int handle_page_fault(void){
    u64 temp1;
    asm volatile("push %rax;""push %rbx;""push %rcx;""push %rdx;""push %r8;""push %r9;""push %r10;""push %r11;""push %r12;""push %r13;""push %r14;""push %r15;""push %rsi;""push %rdi;");
    asm volatile("mov %%rsp, %0" : "=rm" (temp1));
    struct exec_context *current = get_current_ctx();
    u64 rip;
    u64 *rip1;
    asm volatile( "mov %%cr2, %0" : "=rm" ( rip ));
    u64 error_code;
    asm volatile( "mov %%rbp, %0" : "=rm" ( rip1 ));
    error_code=*(rip1+1);
    if(rip>=current->mms[MM_SEG_DATA].start&&rip<current->mms[MM_SEG_DATA].next_free&&(!(error_code&1))){
        u32 frame=current->pgd;
        u64 access_code=((current->mms[MM_SEG_DATA].access_flags)&2)|5;
        u64 shift,val,ctr=0;
        u64 *address,*address1;
        u64 temp=511;
        address=osmap(frame);
        shift=(rip>>39)&temp;
        address+=(shift);
        if((*address)&1){
            ctr++;
            frame=(*address)>>12;
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>30)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>21)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>12)&temp;
        address+=(shift); 
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        val=access_code;
        frame=os_pfn_alloc(USER_REG);
        val+=(frame<<12);
        *address=val;
    }
    else if(rip>=current->mms[MM_SEG_RODATA].start&&rip<current->mms[MM_SEG_RODATA].next_free&&(!(error_code&2))&&(!(error_code&1))){
        u32 frame=current->pgd;
        u64 access_code=((current->mms[MM_SEG_RODATA].access_flags)&2)|5;
        u64 shift,val,ctr=0;
        u64 *address,*address1;
        u64 temp=511;
        address=osmap(frame);
        shift=(rip>>39)&temp;
        address+=(shift);
        if((*address)&1){
            ctr++;
            frame=(*address)>>12;
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>30)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>21)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>12)&temp;
        address+=(shift); 
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        val=access_code;
        frame=os_pfn_alloc(USER_REG);
        val+=(frame<<12);
        *address=val;
    }
    else if(rip>=current->mms[MM_SEG_STACK].start&&rip<current->mms[MM_SEG_STACK].end&&(!(error_code&1))){
        u32 frame=current->pgd;
        u64 access_code=((current->mms[MM_SEG_STACK].access_flags)&2)|5;
        u64 shift,val,ctr=0;
        u64 *address,*address1;
        u64 temp=511;
        address=osmap(frame);
        shift=(rip>>39)&temp;
        address+=(shift);
        if((*address)&1){
            ctr++;
            frame=(*address)>>12;
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>30)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>21)&temp;
        address+=(shift);
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        if((*address)&1){
            ctr++;
            frame=(*address>>12);
            *address=(*address)|access_code;
        }
        else{
            frame=os_pfn_alloc(OS_PT_REG);
        }
        address1=address;
        address=osmap(frame);
        shift=(rip>>12)&temp;
        address+=(shift); 
        val=access_code;
        val+=(frame<<12);
        if((*address1)&1);
        else
            *address1=val;
        val=access_code;
        frame=os_pfn_alloc(USER_REG);
        val+=(frame<<12);
        *address=val;
    }
    else{
            printf("Page fault exception ,VA=%x,Error_code=%x,RIP=%x\n",rip,*(rip1+1),*(rip1+2));
            do_exit();
    }
    asm volatile("mov %0, %%rsp" : : "r" (temp1));
    asm volatile("pop %rdi;""pop %rsi;""pop %r15;""pop %r14;""pop %r13;""pop %r12;""pop %r11;""pop %r10;""pop %r9;""pop %r8;""pop %rdx;""pop %rcx;""pop %rbx;""pop %rax;");
    asm volatile("mov %rbp, %rsp;");
    asm volatile("pop %rbp;");
    asm volatile("add $8, %rsp;");
    asm volatile("iretq;");
    return 0;
}
