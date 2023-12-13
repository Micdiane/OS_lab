#include "printk.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
void clock_set_next_event() ;
extern char _sramdisk[];
struct pt_regs {
    uint64 x[32];
    uint64 sepc;
	uint64 stattus;
};
extern struct task_struct* current;
extern char __ret_from_fork[];
extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern int task_cnt;
extern struct task_struct* task[NR_TASKS];
extern uint64 _stext; 	// kernel text
extern uint64 _srodata;	// kernel rodata
extern uint64 _sdata;	// kernel data



uint64 sys_clone(struct pt_regs* regs){
	// printk("sys_clone\n");
	// return 0;
	// 复制task_struct 其中包括 栈、陷入内核态时的寄存器，还有上一次发生调度，调用 switch_to 时的 thread_struct
	struct task_struct *new_task = (struct task_struct*)alloc_page();
	memcpy(new_task, current, PGSIZE);
	new_task->pid = task_cnt;
	// 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
    // 并将其中的 a0, sp, sepc 设置成正确的值
	uint64_t old_sp = csr_read(sscratch);
	

	new_task->thread.ra = (uint64)__ret_from_fork;
	new_task->thread.sp = regs->x[2] ; // 系统栈指针
	// printk("new_task->thread.ra = %lx\n", new_task->thread.ra);
	
	regs->x[10] = 0;
	new_task->thread.sepc = regs->sepc ;
	new_task->thread.sscratch = old_sp; // 用户栈指针
	
	//之前的用户栈 sp 交换到了 sscratch
	
	
	// 为 child task 申请 user stack，并将 parent task 的 user stack 数据复制到其中。
	uint64 user_stack = alloc_page();
	memcpy((uint64_t *)user_stack, current->thread.sp, PGSIZE);
	printk("user_stack = %lx\n", current->thread.sp);
	create_mapping((uint64*)new_task->pgd, USER_END - PGSIZE, user_stack - PA2VA_OFFSET, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V);

	// 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射
	new_task->pgd = alloc_page();
	memcpy(new_task->pgd, swapper_pg_dir, PGSIZE);
	create_mapping((uint64*)new_task->pgd, USER_START, PGSIZE, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V );

	for (int i = 0; i < current->vma_cnt; i++) {
		struct vm_area_struct *vma = &(current->vmas[i]);
		uint64 va = PGROUNDDOWN(vma->vm_start);
		int p_memsz = vma->vm_end - vma->vm_start;
		uint64_t flag =  PTE_U | PTE_V;
        uint64_t p_flag = vma->vm_flags;
        if (p_flag & 1) flag |= PTE_X;
        if (p_flag & 2) flag |= PTE_W;
        if (p_flag & 4) flag |= PTE_R;
		uint64 user_space = alloc_page(PGROUNDUP(p_memsz)>>12);
		uint64 p_offset = vma->vm_content_offset_in_file;
		uint64 p_filesz = vma->vm_content_size_in_file;
		memcpy(user_space+ p_offset,vma->vm_start ,p_memsz);
		create_mapping((uint64*)new_task->pgd, va, user_space + p_offset - PA2VA_OFFSET, p_memsz, flag);
	}
	// . 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存


	task[task_cnt++] = new_task;
	printk("task_cnt = %d\n", task_cnt);
	for (int i = 0; i < task_cnt; i++) {
		printk("task[%d] = %d\n", i, task[i]->pid);
	}
	return new_task->pid;
}

uint64 sys_write(uint64 fd,const char* buf, size_t count){
    if(fd == 1){
        for(int i = 0; i < count; i++){
            char out = buf[i];
            putc(out);
        }
        return count;
    }
    return -1;
}

uint64 sys_getpid() {
    return current->pid;
}


void trap_handler(unsigned long scause, unsigned long sepc ,struct pt_regs* regs) {
    // 通过 `scause` 判断trap类型
    // 如果是interrupt 判断是否是timer interrupt
    // 如果是timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.3.4 节
    // 其他interrupt / exception 可以直接忽略
	uint64 ret = -1 ;
	// printk("Exception = %d!\n", exceptioncode);
	uint64 sepc_val = csr_read(sepc);
	// printk("sepc = %llx\n", sepc_val);
	uint64 stval = csr_read(stval);
	// printk("stval = %llx\n", stval);
	// printk("scause = %llx\n", scause);
	
    // YOUR CODE HERE
	if(scause >> 63){ // 如果最高位 == 1
		int exceptioncode = scause & 0x7FFFFFFFFFFFFFFF;//取得后63位
		switch (exceptioncode)
		{
		case 5: // timer interrupt
			// printk("[S] Supervisor Mode Timer Interrupt\n");
			clock_set_next_event();
			do_timer(); // 调用dotimer
			break; 

		default: // 其他interrupt 
			break;
		}

		
	}else{ //Exception的情况
		int exceptioncode = scause & 0x7FFFFFFFFFFFFFFF;//取得后63位
		
		switch (exceptioncode)
		{
			printk("Exception = %d!\n", exceptioncode);
			case 8: // ecall_U
				// printk("ecall_U\n");
				switch (regs->x[17]) {
				case 64: //	SYS_WRITE
					ret = sys_write(regs->x[10],regs->x[11],regs->x[12]);
					regs->sepc = regs->sepc + 0x4;
					regs->x[10] = ret;
					break;
				case 172: // SYS_GETPID
					ret = sys_getpid();
					regs->sepc = regs->sepc + 0x4;
					regs->x[10] = ret;
					break;
				case 220: // SYS_CLONE  
					regs->sepc = regs->sepc + 0x4;
					regs->x[10] = sys_clone(regs);
					break;
				}
				// regs->sepc = regs->sepc + 0x4;
				// printk("[ret] = %lx\n", ret);
				// regs->x[10] = ret;
			break;

			case 12:
			case 13:
			case 15:
			printk("Page Fault! on %lx\n", stval);       
			
			// for(int i = 0 ;i < current->vma_cnt ; i++){
			// 	printk("vma[%d] = %lx\n", i, current->vmas[i].vm_start);
			// 	printk("vma[%d] = %lx\n", i, current->vmas[i].vm_end);
			// 	printk("vma[%d] = %lx\n", i, current->vmas[i].vm_content_offset_in_file);
			// 	printk("vma[%d] = %lx\n", i, current->vmas[i].vm_content_size_in_file);
			// 	printk("vma[%d] = %lx\n", i, current->vmas[i].vm_flags);
			// 	printk("地址vma[%d] = %lx\n", i, &(current->vmas[i]));
			// }
			struct vm_area_struct* vma = find_vma(current, stval);
			printk("%lx\n", vma);
			if (scause == 12) {
				
				//如果访问的页是存在数据的，如访问的是代码，则需要从文件系统中读取内容，随后进行映射
			uint64 va = PGROUNDDOWN(stval);
			int pmemsz = vma->vm_end - vma->vm_start;
			uint64 p_offset = vma->vm_content_offset_in_file;
			uint64 pfilesz = vma->vm_content_size_in_file;
			uint64_t flag =  PTE_U | PTE_V;
            uint64_t p_flag = vma->vm_flags;
            if (p_flag & 1) flag |= PTE_X;
            if (p_flag & 2) flag |= PTE_W;
            if (p_flag & 4) flag |= PTE_R;
			
			if(current->thread_info.kernel_sp == 0x54188){
				current->thread_info.kernel_sp = 0;
				// create_mapping((uint64*)current->pgd, stval, user_space + p_offset - PA2VA_OFFSET, pmemsz, flag);
				return;
			}
			int pgcnt = PGROUNDUP(pmemsz)>>12;
			uint64 user_space = alloc_page(pgcnt);
			memset(user_space, 0, pgcnt*PGSIZE);
			uint64 pa = user_space + p_offset - PA2VA_OFFSET;
			memcpy(user_space + p_offset,_sramdisk + p_offset,pmemsz);
			
			memset(user_space + p_offset + pfilesz, 0, pmemsz - pfilesz);
			create_mapping((uint64*)current->pgd, va, pa, pmemsz, flag);
			
			

			
			} else {
				//否则是匿名映射，即找一个可用的帧映射上去即可
				uint64 va = PGROUNDDOWN(stval);
				uint64 user_stack = alloc_page();
				uint64 perm = vma->vm_flags | PTE_U | PTE_V;
        		create_mapping((uint64*)current->pgd, va, user_stack - PA2VA_OFFSET, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V);
			}
			// regs->sepc = regs->sepc + 0x4;
			
			break;

			
		}
		
	}
	
}