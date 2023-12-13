	.extern start_kernel
	.section .text.init
    .globl _start
	.globl ft
_start: 
	la sp,boot_stack_top
	li t0, 0xffffffdf80000000 # PA2VA_OFFSET
	sub sp, sp, t0	
    # lui sp, 0x40104
	# slli sp, sp, 1
	# ram (wxa!ri): ORIGIN = 0x0000000080000000 + (0x200000), LENGTH = 128 * 1024 * 1024 - (0x200000)
    
	
    call setup_vm
    call relocate
ft:

    jal ra, mm_init 	# 设置物理内存管理
    
	jal ra, setup_vm_final # set up real virtual memory

    jal ra, task_init 	# 进行线程初始化
	

	# Direct模式写入stvec trap处理函数基地址 
	# 
	la t0, _traps
	csrw stvec, t0

	# set sie[5] 启用时钟中断
	li t0, 1 << 5
	csrs sie, t0

	

	# 设置第一次时钟中断, 1s后触发
    rdtime t0
    li t1, 10000000
    add a0, t0, t1  
    mv a1, x0
	mv a2, x0
	mv a3, x0
	mv a4, x0
	mv a5, x0
    mv a6, x0 
    mv a7, x0  
    ecall
	
	# 先将 status[SIE] 设置为 0。这样在跳转到 start_kernel 之后，如果有中断发生，就不会立即进入中断处理程序。
	# 等 start_kernel 初始化完毕后，会重新设置 status[SIE] 为 1，这样中断就可以被响应了。
	# 允许中断响应 sstatus[SIE] = 1
	# li t0, 1<<1
	# csrw sstatus, t0

	# 计算新开栈地址,赋给sp 并跳转到start_kernel
    la sp,boot_stack_top
	
	
	# j _stext
    j start_kernel

relocate:
	# 由于后续启动虚拟地址映射后，物理地址会被映射到0xffffffdf80000000，而此时没有等值映射
	# 所以需要启动stvec检测异常并进行icache和tlb的刷新 
	# 也就是利用异常实现等值映射 做完接下的部分
	la t0, change_satp // change_satp段实现flush tlb和icache操作
    csrw stvec, t0

	# 启动临时页表
    li t0, 0xffffffdf80000000 # PA2VA_OFFSET
    add ra, ra, t0
    add sp, sp, t0

	
    # 设置mode
	li t1, 8
    slli t1, t1, 60     # mode 部分设置为 8
    
	# satp MODE | ASID | PPN 
	#      63-60 59-44   43-0 
	la t2, early_pgtbl  # 填入页表物理地址的PPN部分
    sub t2, t2, t0		
	add t0,x0,t2		
	srli t0, t0, 12     # PPN 部分设置为页表物理地址右移 12 位 参考PA结构 后12bit为offset
    or t0, t0, t1
    csrw satp, t0

	

    # flush tlb
    sfence.vma zero, zero
	# flush icache
    fence.i
	

    ret
change_satp:
	# flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i
	ret


    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096

    .globl boot_stack_top
boot_stack_top:
	