    .section .text.entry
    .align 2
    .globl _traps 
_traps:
	csrrw t0,sscratch,t0
    bge t0,sp,_NO_switch
    csrrw t0,sscratch,t0
    csrrw sp,sscratch,sp
    beq x0,x0,finish
_NO_switch: #   如果为0 不需要操作
    csrrw t0,sscratch,t0
finish:
	addi sp, sp, -8 * (32 + 2 + 1) 
    sd x0, 0(sp)
    sd x1, 8(sp)
    sd x2, 16(sp)
    sd x3, 24(sp)
    sd x4, 32(sp)
    sd x5, 40(sp)
    sd x6, 48(sp)
    sd x7, 56(sp)
    sd x8, 64(sp)
    sd x9, 72(sp)
    sd x10, 80(sp)
    sd x11, 88(sp)
    sd x12, 96(sp)
    sd x13, 104(sp)
    sd x14, 112(sp)
    sd x15, 120(sp)
    sd x16, 128(sp)
    sd x17, 136(sp)
    sd x18, 144(sp)
    sd x19, 152(sp)
    sd x20, 160(sp)
    sd x21, 168(sp)
    sd x22, 176(sp)
    sd x23, 184(sp)
    sd x24, 192(sp)
    sd x25, 200(sp)
    sd x26, 208(sp)
    sd x27, 216(sp)
    sd x28, 224(sp)
    sd x29, 232(sp)
    sd x30, 240(sp)
    sd x31, 248(sp)
    
    csrr t0, sepc
    sd t0, 256(sp)

    csrr t0,sstatus
    sd t0, 264(sp)

    # sd sscratch,272(sp)
	# 保存scause 记录trap类型
	# 保存sepc 记录触发trap的指令地址
	# 调用trap函数
	csrr a0, scause
	csrr a1, sepc
	mv a2,sp

	call trap_handler

	ld t0,264(sp)
    csrw sstatus, t0
    ld t0, 256(sp)
    csrw sepc, t0
    ld x0, 0(sp)
    ld x1, 8(sp)
    # ld x2, 16(sp)
    ld x3, 24(sp)
    ld x4, 32(sp)
    ld x5, 40(sp)
    ld x6, 48(sp)
    ld x7, 56(sp)
    ld x8, 64(sp)
    ld x9, 72(sp)
    ld x10, 80(sp)
    ld x11, 88(sp)
    ld x12, 96(sp)
    ld x13, 104(sp)
    ld x14, 112(sp)
    ld x15, 120(sp)
    ld x16, 128(sp)
    ld x17, 136(sp)
    ld x18, 144(sp)
    ld x19, 152(sp)
    ld x20, 160(sp)
    ld x21, 168(sp)
    ld x22, 176(sp)
    ld x23, 184(sp)
    ld x24, 192(sp)
    ld x25, 200(sp)
    ld x26, 208(sp)
    ld x27, 216(sp)
    ld x28, 224(sp)
    ld x29, 232(sp)
    ld x30, 240(sp)
    ld x31, 248(sp)
    ld x2, 16(sp)
    addi sp, sp, 8 * (32 + 2 + 1)
	# # 此时 sp 是系统栈， t0是用户栈
    csrrw t0,sscratch,t0
    ble t0,sp,_NO_switch1
    csrrw t0,sscratch,t0
    csrrw sp,sscratch,sp
    beq x0,x0,finish1
_NO_switch1: #   如果为0 不需要操作
    csrrw t0,sscratch,t0
finish1:
    sret

    .global __ret_from_fork
__ret_from_fork:
kk:
	ld t0,264(sp)
    csrw sstatus, t0
    ld t0, 256(sp)
    addi t0,t0,-4
    csrw sepc, t0
    ld x0, 0(sp)
    ld x1, 8(sp)
    # ld x2, 16(sp)
    ld x3, 24(sp)
    ld x4, 32(sp)
    ld x5, 40(sp)
    ld x6, 48(sp)
    ld x7, 56(sp)
    ld x8, 64(sp)
    ld x9, 72(sp)
    ld x10, 80(sp)
    ld x11, 88(sp)
    ld x12, 96(sp)
    ld x13, 104(sp)
    ld x14, 112(sp)
    ld x15, 120(sp)
    ld x16, 128(sp)
    ld x17, 136(sp)
    ld x18, 144(sp)
    ld x19, 152(sp)
    ld x20, 160(sp)
    ld x21, 168(sp)
    ld x22, 176(sp)
    ld x23, 184(sp)
    ld x24, 192(sp)
    ld x25, 200(sp)
    ld x26, 208(sp)
    ld x27, 216(sp)
    ld x28, 224(sp)
    ld x29, 232(sp)
    ld x30, 240(sp)
    ld x31, 248(sp)
    ld x2, 16(sp)
    mv a0,zero
    addi sp, sp, 8 * (32 + 2 + 1)
	# # 此时 sp 是系统栈， t0是用户栈
    csrrw t0,sscratch,t0
    ble t0,sp,_NO_switch2
    csrrw t0,sscratch,t0
    csrrw sp,sscratch,sp
    beq x0,x0,finish2
_NO_switch2: #   如果为0 不需要操作
    csrrw t0,sscratch,t0
finish2:
    sret


    .globl __dummy
__dummy:
# 切换栈空间到用户栈 USEREND
# 交换了栈空间，此时sscrach保存的是系统栈
	__dummy:
    csrrw sp, sscratch, sp
    sret




	.globl __switch_to
__switch_to:

    # save state to prev process
	sd ra,48(a0)
	sd sp,56(a0)
	sd s0,64(a0)
	sd s1,72(a0)
	sd s2,80(a0)
	sd s3,88(a0)
	sd s4,96(a0)
	sd s5,104(a0)
	sd s6,112(a0)
	sd s7,120(a0)
	sd s8,128(a0)
	sd s9,136(a0)
	sd s10,144(a0)
	sd s11,152(a0)
	csrr t1, sepc
    sd t1,160(a0)
    csrr t1, sstatus
    sd t1,168(a0)
    csrr t1, sscratch
    sd t1,176(a0)
	
	# 这里offset是48 
	# 将下一个线程的 thread_struct 中的相关数据载入到ra，sp，s0~s11中。
	//	下一个线程的寄存器回复
	ld ra,48(a1)
	ld sp,56(a1)
	ld s0,64(a1)
	ld s1,72(a1)
	ld s2,80(a1)
	ld s3,88(a1)
	ld s4,96(a1)
	ld s5,104(a1)
	ld s6,112(a1)
	ld s7,120(a1)
	ld s8,128(a1)
	ld s9,136(a1)
	ld s10,144(a1)
	ld s11,152(a1)
	ld t1,160(a1)
    csrw sepc, t1
    ld t1,168(a1)
    csrw sstatus, t1
    ld t1,176(a1)
    csrw sscratch, t1

stop:
	// 加载下一个线程的页表
	ld t3, 184(a1)
	li t4, 0xffffffdf80000000
    sub t3, t3, t4  // 物理地址
	
	srli t3, t3, 12
    li t2, 0x8000000000000000
    or t3, t3, t2
	// 得到其PPN 写入satp
	csrw satp, t3
	# flush icache
    fence.i
	# flush tlb
    sfence.vma zero, zero
	

    ret