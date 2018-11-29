#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
#include<apic.h>
#include<init.h>
#include<types.h>
#include<context.h>
#include<idt.h>
static u64 numticks;

static void save_current_context(){
  /*Your code goes in here*/ 
} 

static void schedule_context(struct exec_context *next){
  /*Your code goes in here. get_current_ctx() still returns the old context*/
 struct exec_context *current = get_current_ctx();
 printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
/*These two lines must be executed*/
 set_tss_stack_ptr(next);
 set_current_ctx(next);
/*Your code for scheduling context*/
 return;
}

static struct exec_context *pick_next_context(struct exec_context *list){
  /*Your code goes in here*/
  
   return NULL;
}
static void schedule(){
/* 
 struct exec_context *next;
 struct exec_context *current = get_current_ctx(); 
 struct exec_context *list = get_ctx_list();
 next = pick_next_context(list);
 schedule_context(next);
*/     
}

static void do_sleep_and_alarm_account(){
 /*All processes in sleep() must decrement their sleep count*/ 
}

/*The five functions above are just a template. You may change the signatures as you wish*/
void handle_timer_tick(){
 /*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule
 */
  u64* t1;
  u64* t2;
  asm volatile("cli;":::"memory");
  asm volatile("push %rax;""push %rbx;""push %rcx;""push %rdx;""push %r8;""push %r9;""push %r10;""push %r11;""push %r12;""push %r13;""push %r14;""push %r15;""push %rsi;""push %rdi;");
  asm volatile("mov %%rsp,%0":"=rm"(t1));
  asm volatile("mov %%rbp,%0":"=rm"(t2));
  printf("Got a tick. #ticks = %u\n", numticks++);   /*XXX Do not modify this line*/ 
  struct exec_context *current=get_current_ctx();
  int wait=0;
  for(int i=0;i<=15;i++){
    struct exec_context *schedule=get_ctx_by_pid(i);
    schedule->ticks_to_sleep=schedule->ticks_to_sleep-1;
    if(schedule->ticks_to_sleep==1)
        schedule->state=READY;
    if(schedule->state==WAITING)
        wait++;
  }
  int current_pid=current->pid;
  int changed_pid=-1;
  for(int i=1;i<=15;i++){
      int j=(current_pid+i)%16;
      if(j==0)continue;
      struct exec_context *schedule=get_ctx_by_pid(j);
      if(schedule->state==READY){
          changed_pid=j;
          break;
      }
  }
  if(changed_pid==-1&&wait==0&&current->pid==0){
        do_exit();
  }
  if(changed_pid==-1)
      changed_pid=current_pid;
  if(changed_pid!=current_pid){
    printf("Change from %x to %x\n",current->pid,changed_pid);
    struct exec_context *init=get_ctx_by_pid(changed_pid);
    init->ticks_to_sleep=0;
    current->state=READY;
    init->state=RUNNING;

    current->regs.rdi=*t1;
    current->regs.rsi=*(t1+1);
    current->regs.r15=*(t1+2);
    current->regs.r14=*(t1+3);
    current->regs.r13=*(t1+4);
    current->regs.r12=*(t1+5);
    current->regs.r11=*(t1+6);
    current->regs.r10=*(t1+7);
    current->regs.r9=*(t1+8);
    current->regs.r8=*(t1+9);
    current->regs.rdx=*(t1+10);
    current->regs.rcx=*(t1+11);
    current->regs.rbx=*(t1+12);
    current->regs.rax=*(t1+13);

    current->regs.rbp=*t2;
    current->regs.entry_rip=*(t2+1);
    current->regs.entry_cs=*(t2+2);
    current->regs.entry_rflags=*(t2+3);
    current->regs.entry_rsp=*(t2+4);
    current->regs.entry_ss=*(t2+5);

    *t2=init->regs.rbp;
    *(t2+1)=init->regs.entry_rip;
    *(t2+2)=init->regs.entry_cs;
    *(t2+3)=init->regs.entry_rflags;
    *(t2+4)=init->regs.entry_rsp;
    *(t2+5)=init->regs.entry_ss;
    asm volatile("mov %0,%%r15" : : "r" (init->regs.r15));
    asm volatile("mov %0,%%r14" : : "r" (init->regs.r14));
    asm volatile("mov %0,%%r13" : : "r" (init->regs.r13));
    asm volatile("mov %0,%%r12" : : "r" (init->regs.r12));
    asm volatile("mov %0,%%r11" : : "r" (init->regs.r11));
    asm volatile("mov %0,%%r10" : : "r" (init->regs.r10));
    asm volatile("mov %0,%%r9" : : "r"  (init->regs.r9));
    asm volatile("mov %0,%%r8" : : "r"  (init->regs.r8));
    asm volatile("mov %0,%%rdx" : : "r" (init->regs.rdx));
    asm volatile("mov %0,%%rcx" : : "r" (init->regs.rcx));
    asm volatile("mov %0,%%rbx" : : "r" (init->regs.rbx));
    asm volatile("mov %0,%%rax" : : "r" (init->regs.rax));
    asm volatile("mov %0,%%rsi" : : "r" (init->regs.rsi));
    asm volatile("mov %0,%%rdi" : : "r" (init->regs.rdi));
    set_tss_stack_ptr(init);
    set_current_ctx(init);
    ack_irq();
    asm volatile("mov %%rbp, %%rsp;""pop %%rbp;""iretq;":::"memory");
  }
  current->ticks_to_alarm=current->ticks_to_alarm-1;
  if(current->ticks_to_alarm==0){
    current->ticks_to_alarm=current->alarm_config_time;
    invoke_sync_signal(SIGALRM,t2+4,t2+1);
  } 
  ack_irq();  /*acknowledge the interrupt, before calling iretq */
  asm volatile("mov %0, %%rsp"::"r"(t1));
  asm volatile("pop %rdi;""pop %rsi;""pop %r15;""pop %r14;""pop %r13;""pop %r12;""pop %r11;""pop %r10;""pop %r9;""pop %r8;""pop %rdx;""pop %rcx;""pop %rbx;""pop %rax;");
  asm volatile("mov %%rbp, %%rsp;""pop %%rbp;""iretq;":::"memory");
}

void do_exit(){
    u64 *t2;
    asm volatile("mov %%rbp,%0":"=rm"(t2));
    struct exec_context *current=get_current_ctx();
    printf("Exiting %x\n",current->pid);
    if(current->pid==0)
        do_cleanup();
    current->state=UNUSED;
    int wait=0;
    int unused=0;
    for(int i=1;i<=15;i++){
    struct exec_context *schedule=get_ctx_by_pid(i);
      if(schedule->state==WAITING)
          wait++;
      if(schedule->state==UNUSED)
          unused++;
    }
    if(unused==15)
        do_cleanup();
    int current_pid=current->pid;
    int changed_pid=-1;
    for(int i=1;i<=15;i++){
        int j=(current_pid+i)%16;
        if(j==0)continue;
        struct exec_context *schedule=get_ctx_by_pid(j);
        if(schedule->state==READY){
            changed_pid=j;
            break;
        }
    }
    if(changed_pid==-1)
        changed_pid=0;
    printf("Change from after exiting %x to %x\n",current->pid,changed_pid);
    struct exec_context *init=get_ctx_by_pid(changed_pid);
    init->state=RUNNING;

    current->regs.rbp=*t2;
    current->regs.entry_rip=*(t2+1);
    current->regs.entry_cs=*(t2+2);
    current->regs.entry_rflags=*(t2+3);
    current->regs.entry_rsp=*(t2+4);
    current->regs.entry_ss=*(t2+5);

    *t2=init->regs.rbp;
    *(t2+1)=init->regs.entry_rip;
    *(t2+2)=init->regs.entry_cs;
    *(t2+3)=init->regs.entry_rflags;
    *(t2+4)=init->regs.entry_rsp;
    *(t2+5)=init->regs.entry_ss;
    asm volatile("mov %0,%%r15" : : "r" (init->regs.r15));
    asm volatile("mov %0,%%r14" : : "r" (init->regs.r14));
    asm volatile("mov %0,%%r13" : : "r" (init->regs.r13));
    asm volatile("mov %0,%%r12" : : "r" (init->regs.r12));
    asm volatile("mov %0,%%r11" : : "r" (init->regs.r11));
    asm volatile("mov %0,%%r10" : : "r" (init->regs.r10));
    asm volatile("mov %0,%%r9" : : "r"  (init->regs.r9));
    asm volatile("mov %0,%%r8" : : "r"  (init->regs.r8));
    asm volatile("mov %0,%%rdx" : : "r" (init->regs.rdx));
    asm volatile("mov %0,%%rcx" : : "r" (init->regs.rcx));
    asm volatile("mov %0,%%rbx" : : "r" (init->regs.rbx));
    asm volatile("mov %0,%%rax" : : "r" (init->regs.rax));
    asm volatile("mov %0,%%rsi" : : "r" (init->regs.rsi));
    asm volatile("mov %0,%%rdi" : : "r" (init->regs.rdi));
    set_tss_stack_ptr(init);
    set_current_ctx(init);
    asm volatile("mov %%rbp, %%rsp;""pop %%rbp;""iretq;":::"memory");
}

/*system call handler for sleep*/
long do_sleep(u32 ticks){
    u64 *temp1;
    u64 *rip;
    asm volatile("mov %%rsp,%0":"=rm"(temp1));
    asm volatile("mov %%rbp,%0":"=rm"(rip));
    struct exec_context *current=get_current_ctx();
    int current_pid=current->pid;
    int changed_pid=-1;
    for(int i=1;i<=15;i++){
        int j=(current_pid+i)%16;
        if(j==0)continue;
        struct exec_context *schedule=get_ctx_by_pid(j);
        if(schedule->state==READY){
            changed_pid=j;
            break;
        }
    }
    if(changed_pid==-1)
        changed_pid=0;
    struct exec_context *swp=get_ctx_by_pid(changed_pid);
    current->ticks_to_sleep=ticks+1;
    current->state=WAITING;
    swp->state=RUNNING;
    u64* temp2= (u64*)((((u64)current->os_stack_pfn+1)<<12)-8);

    *(rip)=swp->regs.rbp;
    *(rip+1)=swp->regs.entry_rip;
    *(rip+2)=swp->regs.entry_cs;
    *(rip+3)=swp->regs.entry_rflags;
    *(rip+4)=swp->regs.entry_rsp;
    *(rip+5)=swp->regs.entry_ss;

    current->regs.rbp = *(temp2-10);
    current->regs.entry_rip = *(temp2-4);
    current->regs.entry_cs = 0x23;
    current->regs.entry_rflags = *(temp2-2);
    current->regs.entry_rsp = *(temp2-1);
    current->regs.entry_ss = 0x2b;
    current->regs.rbx=*(temp2-5);
    current->regs.rcx=*(temp2-6);
    current->regs.rdx=*(temp2-7);
    current->regs.rsi=*(temp2-8);
    current->regs.rdi=*(temp2-9);
    current->regs.rbp=*(temp2-10);
    current->regs.r8=*(temp2-11);
    current->regs.r9=*(temp2-12);
    current->regs.r10=*(temp2-13);
    current->regs.r11=*(temp2-14);
    current->regs.r12=*(temp2-15);
    current->regs.r13=*(temp2-16);
    current->regs.r14=*(temp2-17);
    current->regs.r15=*(temp2-18);
    set_tss_stack_ptr(swp);
    set_current_ctx(swp);
    asm volatile("mov %0,%%r15" : : "r" (swp->regs.r15));
    asm volatile("mov %0,%%r14" : : "r" (swp->regs.r14));
    asm volatile("mov %0,%%r13" : : "r" (swp->regs.r13));
    asm volatile("mov %0,%%r12" : : "r" (swp->regs.r12));
    asm volatile("mov %0,%%r11" : : "r" (swp->regs.r11));
    asm volatile("mov %0,%%r10" : : "r" (swp->regs.r10));
    asm volatile("mov %0,%%r9" : : "r" (swp->regs.r9));
    asm volatile("mov %0,%%r8" : : "r" (swp->regs.r8));
    asm volatile("mov %0,%%rdx" : : "r" (swp->regs.rdx));
    asm volatile("mov %0,%%rcx" : : "r" (swp->regs.rcx));
    asm volatile("mov %0,%%rbx" : : "r" (swp->regs.rbx));
    asm volatile("mov %0,%%rax" : : "r" (swp->regs.rax));
    asm volatile("mov %0,%%rsi" : : "r" (swp->regs.rsi));
    asm volatile("mov %0,%%rdi" : : "r" (swp->regs.rdi));
    asm volatile("mov %%rbp, %%rsp;""pop %%rbp;""iretq;":::"memory");
}

/*
  system call handler for clone, create thread like 
  execution contexts
*/
long do_clone(void *th_func, void *user_stack){
    struct exec_context *clone=get_new_ctx();
    struct exec_context *current=get_current_ctx();
    printf("Created new context %x\n",clone->pid); 
    u64* address = (u64*)((((u64)current->os_stack_pfn+1)<<12)-8);
    clone->type=current->type;
    clone->used_mem=current->used_mem;
    clone->pgd=current->pgd;
    clone->os_rsp=current->os_rsp;

    char new_name[CNAME_MAX];
    int d=strlen(current->name);
    memcpy(new_name,current->name,CNAME_MAX);
    int d2=clone->pid;
    if(d2<10){
        new_name[d]=d2+'0';
    }
    if(d2>10){
        int d3=d2%10;
        new_name[d]='1';
        new_name[d+1]=d3%10+'0';
    }
    memcpy(clone->name,new_name,CNAME_MAX);

    for(int i=0;i<MAX_MM_SEGS;i++){
        clone->mms[i]=current->mms[i];
    }

    clone->pending_signal_bitmap=clone->pending_signal_bitmap;

    for(int i=0;i<MAX_SIGNALS;i++){
        clone->sighandlers[i]=current->sighandlers[i];
    }

    clone->ticks_to_sleep=current->ticks_to_sleep;
    clone->alarm_config_time=current->alarm_config_time;
    clone->ticks_to_alarm=current->ticks_to_alarm;
    clone->regs=current->regs;
    
    clone->os_stack_pfn=os_pfn_alloc(OS_PT_REG);
    clone->regs.entry_cs=0x23;
    clone->regs.entry_ss=0x2b;
  	clone->regs.entry_rflags = *(address-2); 
    clone->regs.entry_rip=(u64)th_func;
    clone->regs.entry_rsp=(u64)user_stack;
    clone->regs.rbp=(u64)user_stack;
    clone->state=READY;
}

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip){
    printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
    struct exec_context *current=get_current_ctx();
    if(current->sighandlers[signo]!=NULL){
       u64* temp=(u64*)*ustackp;
       *(temp-1)=*urip;
       u64 temp2=(u64)(temp-1);
       *ustackp=temp2;
       u64 temp3=(u64)current->sighandlers[signo];
       *urip=temp3;
    }
    if(signo != SIGALRM&&(current->sighandlers[signo]==NULL))
      do_exit();
}
/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler){
    struct exec_context *current=get_current_ctx();
    current->sighandlers[signo]=(void *)handler;
}

/*system call handler for alarm*/
long do_alarm(u32 ticks){
    struct exec_context *current=get_current_ctx();
    current->alarm_config_time=ticks;
    current->ticks_to_alarm=ticks;
}
