#include<context.h>
#include<memory.h>
#include<lib.h>

void reset(u32 addr) {
	u64 *va = (u64 *)osmap(addr);
	for(int i = 0;i < 512;i++) {
		*va = 0;
		va++;
	}
}

void prepare_context_mm(struct exec_context *ctx)
{
	// new to get address of first level translation, and save in pgd
	// arg_pfn = PFN of MM_SEG_DATA

	// virtual address for stack differs since stack grows from higher address.
	u64 vaddr_stack = (ctx -> mms[MM_SEG_STACK]).end - 0x1000;
	u64 vaddr_data  = (ctx -> mms[MM_SEG_DATA]).start;
	u64 vaddr_code  = (ctx -> mms[MM_SEG_CODE]).start;

	// some masks
	// --- extraction of radix tree index ---
	u64 pde_index_mask_l4 = 0x0000FF8000000000;
	u64 pde_index_mask_l3 = 0x0000007FC0000000;
	u64 pde_index_mask_l2 = 0x000000003FE00000;
	u64 pde_index_mask_l1 = 0x00000000001FF000;
	// shift values
	u16 pde_index_shift_l4 = 39;
	u16 pde_index_shift_l3 = 30;
	u16 pde_index_shift_l2 = 21;
	u16 pde_index_shift_l1 = 12;

	// variables to generate PTE
	u16 pte_shift     = 0x000C;
	u64 read_enabled  = 0x0000000000000000;
	u64 write_enabled = 0x0000000000000002;
	// if present just take or with 1;



	// allocate frame for L4 table: needed for CR3
	u32 l4_pfn   = os_pfn_alloc(OS_PT_REG);
	ctx -> pgd   = l4_pfn;
	reset(l4_pfn);
	u64 *l4_vaddr = (u64 *)osmap(l4_pfn); // as computation can only be done through virtual address



	// forming page table entries for code
	u64 code_access_flag = 0;
	if((ctx -> mms[MM_SEG_CODE]).access_flags & 2)
		code_access_flag = 2;
	
	u64 *pde_code_l4   = l4_vaddr + ((vaddr_code & pde_index_mask_l4) >> pde_index_shift_l4);
	// since the table is empty, corresponding page table doesnt exist
	u32 l3_code_pfn    = os_pfn_alloc(OS_PT_REG);
	reset(l3_code_pfn);
	u64 *l3_code_vaddr = (u64 *)osmap(l3_code_pfn);
	// since this table is present and third bit should be 1
	*pde_code_l4       = (l3_code_pfn << pte_shift) | 5 | code_access_flag; 

	u64 *pde_code_l3   = l3_code_vaddr + ((vaddr_code & pde_index_mask_l3) >> pde_index_shift_l3);
	// now we create l2 table
	u32 l2_code_pfn    = os_pfn_alloc(OS_PT_REG);
	reset(l2_code_pfn);
	u64 *l2_code_vaddr = (u64 *)osmap(l2_code_pfn);
	*pde_code_l3       = (l2_code_pfn << pte_shift) | 5 | code_access_flag; 

	u64 *pde_code_l2   = l2_code_vaddr + ((vaddr_code & pde_index_mask_l2) >> pde_index_shift_l2);
	// now for l1
	u32 l1_code_pfn    = os_pfn_alloc(OS_PT_REG);
	reset(l2_code_pfn);
	u64 *l1_code_vaddr = (u64 *)osmap(l1_code_pfn);
	*pde_code_l2       = (l1_code_pfn << pte_shift) | 5 | code_access_flag;

	u64 *pde_code_l1 = l1_code_vaddr + ((vaddr_code & pde_index_mask_l1) >> pde_index_shift_l1);
	// create data entry in l1 
	u32 final_table_code_pfn       = os_pfn_alloc(USER_REG);
	reset(final_table_code_pfn);
	u64 *final_table_code_vaddr    = (u64 *)osmap(final_table_code_pfn); // TODO check if this is not needed
	*pde_code_l1                   = (final_table_code_pfn << pte_shift) | 5 | code_access_flag;


	// page table entries for stack
	u64 stack_access_flag = 0;
	if((ctx -> mms[MM_SEG_STACK]).access_flags & 2)
		stack_access_flag = 2;

	u64 *pde_stack_l4  = l4_vaddr + ((vaddr_stack & pde_index_mask_l4) >> pde_index_shift_l4);
	u64 *l3_stack_vaddr;

	if((*pde_stack_l4 & 1) == 1) {// already present in l4 table
	    *pde_stack_l4 = *pde_stack_l4 | stack_access_flag;
		l3_stack_vaddr = (u64 *)(osmap(*pde_stack_l4 >> 12));

	}
	else {
		u32 l3_stack_pfn = os_pfn_alloc(OS_PT_REG);
		reset(l3_stack_pfn);
		l3_stack_vaddr   = (u64 *)osmap(l3_stack_pfn);
		*pde_stack_l4    = (l3_stack_pfn << pte_shift) | 5 | stack_access_flag;
	}


	u64 *pde_stack_l3 = l3_stack_vaddr + ((vaddr_stack & pde_index_mask_l3) >> pde_index_shift_l3);
	u64 *l2_stack_vaddr;
	if((*pde_stack_l3 & 1) == 1) {
		l2_stack_vaddr = (u64 *)(osmap(*pde_stack_l3 >> 12));
		*pde_stack_l3 = *pde_stack_l3 | stack_access_flag;
	}
	else {
		u32 l2_stack_pfn = os_pfn_alloc(OS_PT_REG);
		reset(l2_stack_pfn);
		l2_stack_vaddr    = (u64 *)osmap(l2_stack_pfn);
		*pde_stack_l3    = (l2_stack_pfn << pte_shift) | 5 | stack_access_flag;
	}
	u64 *pde_stack_l2 = l2_stack_vaddr + ((vaddr_stack & pde_index_mask_l2) >> pde_index_shift_l2);
	u64 *l1_stack_vaddr;

	if((*pde_stack_l2 & 1) == 1) {
		l1_stack_vaddr = (u64 *)(osmap(*pde_stack_l2 >> 12));
		*pde_stack_l2 = *pde_stack_l2 | stack_access_flag;
	}
	else {
		u32 l1_stack_pfn = os_pfn_alloc(OS_PT_REG);
		reset(l1_stack_pfn);
		l1_stack_vaddr    = (u64 *)osmap(l1_stack_pfn);
		*pde_stack_l2    = (l1_stack_pfn << pte_shift) | 5 | stack_access_flag;
	}
	u64 *pde_stack_l1 =  l1_stack_vaddr + ((vaddr_stack & pde_index_mask_l1) >> pde_index_shift_l1);
	// since all 3 pages (code, stack, memory) are on different page, we have to create a new entry in l1
	u32 final_table_stack_pfn       = os_pfn_alloc(USER_REG);
	reset(final_table_stack_pfn);
	u64 *final_table_stack_vaddr    = (u64 *)osmap(final_table_code_pfn); // TODO check if this is not needed
	*pde_stack_l1                   = (final_table_stack_pfn << pte_shift) | 5 | stack_access_flag;


	// page table entries for data
	u64 data_access_flag = 0;
	if((ctx -> mms[MM_SEG_DATA]).access_flags & 2)
		data_access_flag = 2;

	u64 *pde_data_l4  = l4_vaddr + ((vaddr_data & pde_index_mask_l4) >> pde_index_shift_l4);
	u64 *l3_data_vaddr;

	if((*pde_data_l4 & 1) == 1) {// already present in l4 table
		l3_data_vaddr = (u64 *)(osmap(*pde_data_l4 >> 12));
		*pde_data_l4 = *pde_data_l4 | *pde_data_l4;
	}
	else {
		u32 l3_data_pfn = os_pfn_alloc(OS_PT_REG);
		reset(l3_data_pfn);
		l3_data_vaddr   = (u64 *)osmap(l3_data_pfn);
		*pde_data_l4    = (l3_data_pfn << pte_shift) | 5 | data_access_flag;
	}

	u64 *pde_data_l3 = l3_data_vaddr + ((vaddr_data & pde_index_mask_l3) >> pde_index_shift_l3);
	u64 *l2_data_vaddr;

	if((*pde_data_l3 & 1) == 1) {
		l2_data_vaddr = (u64 *)(osmap(*pde_data_l3 >> 12));
		*pde_data_l3 = *pde_data_l3 | data_access_flag;
	}
	else {
		u32 l2_data_pfn = os_pfn_alloc(OS_PT_REG);
		l2_data_vaddr    = (u64 *)osmap(l2_data_pfn);
		reset(l2_data_pfn);
		*pde_data_l3    = (l2_data_pfn << pte_shift) | 5 | data_access_flag;
	}

	u64 *pde_data_l2 = l2_data_vaddr + ((vaddr_data & pde_index_mask_l2) >> pde_index_shift_l2);
	u64 *l1_data_vaddr;

	if((*pde_data_l2 & 1) == 1) {
		l1_data_vaddr = (u64 *)(osmap(*pde_data_l2 >> 12));
		*pde_data_l2 = *pde_data_l2 | data_access_flag;
	}
	else {
		u32 l1_data_pfn = os_pfn_alloc(OS_PT_REG);
		reset(l1_data_pfn);
		l1_data_vaddr    = (u64 *)osmap(l1_data_pfn);
		*pde_data_l2    = (l1_data_pfn << pte_shift) | 5 | data_access_flag;
	}

	u64 *pde_data_l1 =  l1_data_vaddr + ((vaddr_data & pde_index_mask_l1) >> pde_index_shift_l1);
	// but at this index we are required to store arg_pfn
	*pde_data_l1     = ((ctx -> arg_pfn) << pte_shift) | 5 | data_access_flag;

   	return;
}



void cleanup_context_mm(struct exec_context *ctx) {

	u32 pfn_l4 = ctx -> pgd;
	u64 *l4_vaddr = (u64 *)osmap(pfn_l4);

	// virtual address of all pages
	u64 vaddr_stack = (ctx -> mms[MM_SEG_STACK]).end - 0x1000;
	u64 vaddr_data  = (ctx -> mms[MM_SEG_DATA]).start;
	u64 vaddr_code  = (ctx -> mms[MM_SEG_CODE]).start;

	u64 pde_index_mask_l4 = 0x0000FF8000000000;
	u64 pde_index_mask_l3 = 0x0000007FC0000000;
	u64 pde_index_mask_l2 = 0x000000003FE00000;
	u64 pde_index_mask_l1 = 0x00000000001FF000;
	// shift values
	u16 pde_index_shift_l4 = 39;
	u16 pde_index_shift_l3 = 30;
	u16 pde_index_shift_l2 = 21;
	u16 pde_index_shift_l1 = 12;

	// variables to generate PTE
	u16 pte_shift = 0x000C;
	


	// freeing all table entries for code
	u32 pfn_code[4]; // stores PFN of pages which needs to be deleted
	u64 *temp;

	temp   = l4_vaddr + ((vaddr_code & pde_index_mask_l4) >> pde_index_shift_l4);
	pfn_code[0] = (*temp >> pte_shift) & 0xFFFFFFFF; // l3
	
	temp        = (u64 *)osmap(pfn_code[0]) + ((vaddr_code & pde_index_mask_l3) >> pde_index_shift_l3);
	pfn_code[1] = (*temp >> pte_shift) & 0xFFFFFFFF; // l2

	temp        = (u64 *)osmap(pfn_code[1]) + ((vaddr_code & pde_index_mask_l2) >> pde_index_shift_l2);
	pfn_code[2] = (*temp >> pte_shift) & 0xFFFFFFFF; // l1

	temp        = (u64 *)osmap(pfn_code[2]) + ((vaddr_code & pde_index_mask_l1) >> pde_index_shift_l1);
	pfn_code[3] = (*temp >> pte_shift) & 0xFFFFFFFF; // actual physical adress



	// freeing all table entries for stack
	u32 pfn_stack[4]; // stores PFN of pages which needs to be deleted
	u32 Max_N = 9;
	temp        = l4_vaddr + ((vaddr_stack & pde_index_mask_l4) >> pde_index_shift_l4);
	pfn_stack[0] = (*temp >> pte_shift) & 0xFFFFFFFF; // l3
	
	temp         = (u64 *)osmap(pfn_stack[0]) + ((vaddr_stack & pde_index_mask_l3) >> pde_index_shift_l3);
	pfn_stack[1] = (*temp >> pte_shift) & 0xFFFFFFFF; // l2

	temp        = (u64 *)osmap(pfn_stack[1]) + ((vaddr_stack & pde_index_mask_l2) >> pde_index_shift_l2);
	pfn_stack[2] = (*temp >> pte_shift) & 0xFFFFFFFF; // l1

	temp        = (u64 *)osmap(pfn_stack[2]) + ((vaddr_stack & pde_index_mask_l1) >> pde_index_shift_l1);
	pfn_stack[3] = (*temp >> pte_shift) & 0xFFFFFFFF; // actual physical address



	// freeing all table entries for data
	u32 pfn_data[4]; // stores PFN of pages which needs to be deleted

	temp   = l4_vaddr + ((vaddr_data & pde_index_mask_l4) >> pde_index_shift_l4);
	pfn_data[0] = (*temp >> pte_shift) & 0xFFFFFFFF; // l3
	
	temp        = (u64 *)osmap(pfn_data[0]) + ((vaddr_data & pde_index_mask_l3) >> pde_index_shift_l3);
	pfn_data[1] = (*temp >> pte_shift) & 0xFFFFFFFF; // l2

	temp        = (u64 *)osmap(pfn_data[1]) + ((vaddr_data & pde_index_mask_l2) >> pde_index_shift_l2);
	pfn_data[2] = (*temp >> pte_shift) & 0xFFFFFFFF; // l1

	temp        = (u64 *)osmap(pfn_data[2]) + ((vaddr_data & pde_index_mask_l1) >> pde_index_shift_l1);
	pfn_data[3] = (*temp >> pte_shift) & 0xFFFFFFFF; // actual physical address


	// check for duplicates
	u32 pfn_usr[3], pfn_os[Max_N], usr = 0, os = 0;
	for(int i = 0;i < 3;i++) {
		if(pfn_code[i] != pfn_stack[i]) {
			pfn_os[os++] = pfn_code[i];
			pfn_os[os++] = pfn_stack[i];
			if((pfn_data[i] != pfn_stack[i]) && (pfn_data[i] != pfn_code[i])) {
				pfn_os[os++] = pfn_data[i];
			}
		}
		else {
			pfn_os[os++] = pfn_code[i];
			if(pfn_data[i] != pfn_code[i])
				pfn_os[os++] = pfn_data[i];
		}
	}
	pfn_os[os++] = pfn_l4; // as this page table also needs to be cleaned


	if(pfn_code[3] != pfn_stack[3]) {
			pfn_usr[usr++] = pfn_code[3];
			pfn_usr[usr++] = pfn_stack[3];
			if(pfn_data[3] != pfn_stack[3] && pfn_data[3] != pfn_code[3])
				pfn_usr[usr++] = pfn_data[3];
		}
	else {
		pfn_usr[usr++] = pfn_code[3];
		if(pfn_data[3] != pfn_code[3])
			pfn_usr[usr++] = pfn_data[3];
	}
	
	// free all the pages
	for(int i = 0;i < usr;i++)
		os_pfn_free(USER_REG, pfn_usr[i]);
	for(int i = 0;i < os;i++)
		os_pfn_free(OS_PT_REG, pfn_os[i]);

   	return;
}
