//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "vm.h"
#include "elf.h"
#include "string.h"

//arch/riscv/kernel/proc.c
int task_cnt = 2;  //IDLE task 和 初始化时新建的 task 各占用一个
extern void __dummy();
extern void* memcpy(void*dst,void* src,uint64 n);
extern char _sramdisk[];
extern char _eramdisk[];
struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此
extern unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

// 用于初始化 task[i].priority 的数组

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr)
{
    if (task == NULL)
        return NULL;
   for(int i = 0; i < task->vma_cnt; ++i){
       if(task->vmas[i].vm_start <= addr && task->vmas[i].vm_end > addr){
           return &(task->vmas[i]);
       }
   }
    return NULL;
}

void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file)
{
    if (task == NULL)
        return;

    // // 1. 检查 addr 是否已经被映射
    struct vm_area_struct *existing_vma = find_vma(task, addr);
    // if (existing_vma != NULL)
    //     return;

    // // 2. 检查 addr + length - 1 是否已经被映射
    // existing_vma = find_vma(task, addr + length - 1);
    // if (existing_vma != NULL)
    //     return;

    // 3. 检查 addr ~ addr + length - 1 之间是否有重叠的 VMA
    // existing_vma = task->vmas;
    // while (existing_vma != NULL) {
    //     if ((addr >= existing_vma->vm_start && addr < existing_vma->vm_end) ||
    //         (addr + length > existing_vma->vm_start && addr + length <= existing_vma->vm_end))
    //         return;
    //     existing_vma++;
    // }

    // 4. 为新的 VMA 分配空间
    struct vm_area_struct *new_vma = task->vmas + task->vma_cnt;
    task->vma_cnt++;
    new_vma->vm_start = addr;
    new_vma->vm_end = addr + length;
    // printk("[S]vm_start = %lx\n", new_vma->vm_start);
    // printk("[S]vm_end = %lx\n", new_vma->vm_end);
    // printk("end - start = %lx\n", new_vma->vm_end - new_vma->vm_start);
    new_vma->vm_flags = flags;
    new_vma->vm_content_offset_in_file = vm_content_offset_in_file;
    new_vma->vm_content_size_in_file = vm_content_size_in_file;
    
}


static uint64 load_program(struct task_struct* task) {
    // 一个指向 ELF 文件头的指针 ehdr
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)_sramdisk;
    // 程序头表的起始地址 phdr_start 以及程序头表中的条目数 phdr_cnt
    //  是一个 Phdr 数组，其中的每个元素都是一个 Elf64_Phdr
    uint64 phdr_start = (uint64_t)ehdr+ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr *phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        //指针 phdr 指向当前程序头的地址，通过将 phdr_start 加上当前程序头的偏移量乘以程序头的大小
        // Elf64_Phdr* phdrs = (Elf64_Phdr*)(_sramdisk + ehdr->phoff)，是一个 Phdr 数组
        // 其中的每个元素都是一个 Elf64_Phdr
        phdr= (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) *i);
        if (phdr->p_type == PT_LOAD) {
            load_phdr_cnt ++ ;
            // 当前Segment的内存大小 p_mem_sz、在文件占用的大小 file_sz 和所占页数 pg_cnt。
            uint64 p_mem_sz = phdr->p_memsz;
            // uint64 pg_cnt = phdr->p_filesz/PGSIZE + (phdr->p_filesz % PGSIZE != 0);
            uint64 pg_cnt = PGROUNDUP(p_mem_sz)>>12;
            //调用 alloc_page 函数为程序分配相应数量的页
            uint64 user_space = alloc_page(pg_cnt);
            // memset((char*)user_space, 0, p_mem_sz); 
            
            uint64 p_offset = phdr->p_offset;
            uint64_t flag =  PTE_U | PTE_V;
            uint64_t p_flag = phdr->p_flags;
            if (p_flag & 1) flag |= PTE_X;
            if (p_flag & 2) flag |= PTE_W;
            if (p_flag & 4) flag |= PTE_R;
            // create_mapping((uint64*)(task->pgd),phdr->p_vaddr,user_space - PA2VA_OFFSET,p_mem_sz,flag);
            // printk("p_vaddr = %lx\n", phdr->p_vaddr);
            // uapp 主程序
            //代码和数据区域：该区域从 ELF 给出的 Segment 起始地址 phdr->p_offset 开始，权限参考 phdr->p_flags 进行设置。
            do_mmap(task, phdr->p_vaddr, p_mem_sz, p_flag, p_offset, phdr->p_filesz); 
        }  
    }
    // 用户程序的入口地址 ehdr->e_entry 设置为任务的线程程序计数器 task->thread.sepc
    task->thread.sepc = ehdr->e_entry;
    uint64 sstatus =csr_read(sstatus);
        sstatus &= ~(1 << 8);
        sstatus |= (1 << 5);
        sstatus |= (1 << 18);
    task->thread.sstatus = sstatus;
    task->thread.sscratch = USER_END;
}



void task_init() {
    test_init(NR_TASKS);
    // 1. 调用 kalloc() 为 idle 分配一个物理页
	idle = (struct task_struct*)kalloc();

    // 2. 设置 state 为 TASK_RUNNING;
	idle->state = TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
	idle->counter  = 0;
	idle->priority = 0;
    // 4. 设置 idle 的 pid 为 0
	idle->pid = 0;

    // 5. 将 current 和 task[0] 指向 idle
	current = idle;
	task[0] = idle;	
    

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
	for(int i = 0; i< task_cnt ; ++i){
		struct task_struct* temtask = (struct task_struct*)alloc_page();
		// 为每个 temtask 分配一个物理页

		temtask->state = TASK_RUNNING;
		temtask->counter = task_test_counter[i];
		temtask->priority = task_test_priority[i];
		temtask->pid = i;
		temtask->thread.ra = (uint64)__dummy;
		//其中 `ra` 设置为 __dummy的地址
        temtask->thread.sp = (uint64)temtask + PGSIZE;

        temtask->thread.sepc = USER_START;
        uint64 sstatus =csr_read(sstatus);
        sstatus &= ~(1 << 8);
        sstatus |= (1 << 5);
        sstatus |= (1 << 18);
        temtask->thread.sstatus = sstatus;
        temtask->thread.sscratch = USER_END;
        
        temtask->vma_cnt = 0;
        // temtask->vmas = NULL;
        // printk("vma = %lx\n", temtask->vmas[0]);
        
        // 用户栈
        //用户栈：范围为 [USER_END - PGSIZE, USER_END) ，权限为 VM_READ | VM_WRITE, 并且是匿名的区域。
        do_mmap(temtask, USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK , 0, 0);
        
        // 申请页表空间，将swapper_pg_dir中的内容复制到页表中
        temtask->pgd = (uint64*)alloc_page();
        memcpy((uint64*)temtask->pgd, swapper_pg_dir, PGSIZE);

        
        create_mapping((uint64*)temtask->pgd, USER_START, PGSIZE, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V);
        // 而为了减少 task 初始化时的开销，我们对一个 Segment 或者 用户态的栈 只需分别建立一个 VMA。
        // 代码和数据区域：该区域从 ELF 给出的 Segment 起始地址 phdr->p_offset 开始，权限参考 phdr->p_flags 进行设置。
        // 用户栈：范围为 [USER_END - PGSIZE, USER_END) ，权限为 VM_READ | VM_WRITE, 并且是匿名的区域。
        // 为了简化实现，我们只需要考虑一个 Segment 的情况，即只有一个 LOAD 类型的 Segment。
        




        // 创建映射 user stack
        // uint64 user_stack = alloc_page();
   
        // create_mapping((uint64*)temtask->pgd, USER_END - PGSIZE, user_stack - PA2VA_OFFSET, PGSIZE, PTE_R | PTE_W | PTE_U | PTE_V);

        
        
        // uint64 user_page = alloc_page();
        // memcpy((uint64*)user_page,_sramdisk,PGSIZE);

        // create_mapping((uint64*)temtask->pgd, USER_START, user_page - PA2VA_OFFSET, PGSIZE, PTE_X | PTE_R | PTE_W | PTE_U | PTE_V);
        
        load_program(temtask);
		//`sp` 设置为 该线程申请的物理页的高地址
		task[i] = temtask; 
	}for(int i = task_cnt ; i < NR_TASKS ; ++i){
        task[i] = NULL;
        //IDLE task 和 初始化时新建的 task 各占用一个 如果 task[pid] == NULL, 说明这个 pid 还没有被占用，可以作为新 task 的 pid
    }

    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址
    printk("...proc_init done!\n");
}

extern uint64 check_pte(uint64 *pgtbl, uint64 va );
// arch/riscv/kernel/proc.c
void dummy() {
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d at 0x%016lx\n ", current->pid, auto_inc_local_var,current);
			// printk("tempte =  %lx\n", check_pte(swapper_pg_dir, (uint64)current));
        }
    }
}

extern void __switch_to(struct task_struct* prev, struct task_struct* next);


void switch_to(struct task_struct* next) {
    if(current == next) return; // 如果是同一个线程,则什么都不做

	//否则调用__switch_to 实现线程切换
	struct task_struct* prev = current;
	current = next;
	printk("switch to [pid = %d COUNTER = %d]\n", next->pid,next->counter);
    // printTaskStruct(prev);
    // printTaskStruct(next);
	__switch_to(prev, next);
}

void printTaskStruct(struct task_struct* task){
    printk("pid = [%lx]\n", task->pid);
    printk("task->state = %lx\n", task->state);
    printk("task->counter = %lx\n", task->counter);
    printk("task->priority = %lx\n", task->priority);
    printk(" user_sp = %lx\n", task->thread_info.user_sp);
    printk("task->thread.ra = %lx\n", task->thread.ra);
    printk("task->thread.sp = %lx\n", task->thread.sp);
    printk("task->thread.sepc = %lx\n", task->thread.sepc);
    printk("task->thread.sstatus = %lx\n", task->thread.sstatus);
    printk("task->thread.sscratch = %lx\n", task->thread.sscratch);
    printk("task->pgd = %lx\n", task->pgd);
    printk("task->vma_cnt = %lx\n", task->vma_cnt);
    printk("task->vmas = %lx\n", task->vmas);
}

void do_timer(void){
	// 如果当前线程是 idle, 则直接进行调度
	if (current == idle || current->counter == 0) {
    	schedule();
	}
	//  如果当前线程不是 idle 对当前线程的运行剩余时间减1 
	//  若剩余时间仍然大于0 则直接返回 否则进行调度
	else {
    	current->counter--;
		if(current->counter > 0) return;
    	schedule();
	}
}


# ifdef SJF
// 短作业调度算法 
// 遍历所有线程 选出一个counter最小的 切换,
// 如果全都是0,那么则对 task[1] ~ task[NR_TASKS-1] 的运行剩余时间重新赋值 （使用 rand()）
void schedule(void) {
    uint64 minCounter = 0x7fffffffffffffff;
    struct task_struct* next = idle; // 设计下一个切换的线程
    while (1) {
        for (int i = 1; i < task_cnt; ++i) {
            if (task[i]->state == TASK_RUNNING && task[i]->counter != 0 && task[i]->counter < minCounter) {
                minCounter = task[i]->counter;
                next = task[i];
            }
        }
		// 维护一个最小值,设置为next线程
        if (next != idle) break; // 找到了就直接退出 并切换到next
		// 如果都是 0 那么就给所有线程的counter随机赋值
        for (int i = 0; i < task_cnt; i++) {
            printk("task[%d] = %d\n", i, task[i]->pid);
        }
        for (int i = 1; i < task_cnt; ++i) {
            task[i]->counter = rand() % 5 + 1;
            task[i]->priority = rand() % (PRIORITY_MAX - PRIORITY_MIN + 1) + PRIORITY_MIN;
			if (i == 1) printk("\n");
            printk("SET[PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
        }
    }
    switch_to(next);
}

#else
void schedule(void) {
    uint64 maxpriority = 0;
    struct task_struct* next = idle; // 设计下一个切换的线程
    while (1) {
        for (int i = 1; i < NR_TASKS; ++i) {
            if (task[i]->state == TASK_RUNNING && task[i]->counter != 0 && task[i]->priority > maxpriority) {
                maxpriority = task[i]->priority;
                next = task[i];
            }
        }
		// 维护一个最小值,设置为next线程
        if (next != idle) break; // 找到了就直接退出 并切换到next
		// 如果都是 0 那么就给所有线程的counter随机赋值
        for (int i = 1; i < NR_TASKS; ++i) {
            task[i]->counter = rand() % 5 + 1;
            task[i]->priority = rand() % (PRIORITY_MAX - PRIORITY_MIN + 1) + PRIORITY_MIN;
			if (i == 1) printk("\n");
            printk("SET[PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
        }
    }
    switch_to(next);
}
// 优先级算法
//
// void schedule(void) {
// 	printk("schedule\n");
// 	uint64 c;
// 	struct task_struct* next = idle;
//     while (1) {
// 		c = 0;
// 		for (int i = 1; i < NR_TASKS; ++i) {
//             if (task[i]->state == TASK_RUNNING && task[i]->counter > c){
// 				c = task[i]->counter;
// 				next = task[i];
// 				printk("找");
// 			}
// 		}
// 		if (c) break;
// 		for (int i = 1; i < NR_TASKS; ++i) {
//             task[i]->counter = rand() % 5 + 1;
//             task[i]->priority = rand() % (PRIORITY_MAX - PRIORITY_MIN + 1) + PRIORITY_MIN;
// 			if (i == 1) printk("\n");
//             printk("SET[PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
//         }
// 	}
//     switch_to(next);
// }

#endif

