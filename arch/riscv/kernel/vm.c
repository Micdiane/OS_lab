// arch/riscv/kernel/vm.c
#include "defs.h"
#include "types.h"
#include "mm.h"
#include "string.h"
#include "printk.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));
// 512 * 8 = 4096 = 4KB

// 将 0x80000000 开始的 1GB 空间映射到虚拟内存高地址处
void setup_vm(void) {
    /* 
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表 
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。 
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */
    // virtual address = 0x80000000 => VPN[2] = 2
    memset(early_pgtbl, 0x0, PGSIZE);
	// 建立一个页表项 这里只进行1GB的线性映射，所以只需要一个页表项即可

    int highindex = VPN2(VM_START); // 中间9 bit 作为 early_pgtbl 的 index
	int equalindex = VPN2(PHY_START); 
	// 低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 
	// 根页表的每个 entry 都对应 1GB 的区域。
	// VPN2 VPN1 VPN0 pageoffset
	// 9    9    9    12
	// 38	29	 20   11

	// PA >> 12 == PPN
	// physical address
	// PPN2 PPN1 PPN0 RSW D A G U X W R V 后10bit 是 flag
	// 26   9    9    2   1 1 1 1 1 1 1 1
	// 53   27   18   9   7 6 5 4 3 2 1 0 
	// 设置PPN2对应的物理地址为PHY_START 0x80000000 >> 30 = 2
	// 然后设置flag为V R W X 30bit 只有PPN2 对应的部分 也就是28bit及以上部分
	uint64 entity = (((PHY_START >> 30) & 0x3ffffff) << 28) | PTE_V | PTE_R | PTE_W | PTE_X;
    early_pgtbl[highindex] = entity; // 高映射
	early_pgtbl[equalindex]= entity; // 等值映射
	printk("early_pgtbl: %lx ,entity = %lx\n", early_pgtbl,entity);

	printk("setup_vm!\n");
	return ;
}




/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
// 512 * 8 = 4096 = 4KB
extern uint64 _stext; 	// kernel text
extern uint64 _srodata;	// kernel rodata
extern uint64 _sdata;	// kernel data
// 对应vmlinux 中分布好的地址，这里直接使用其地址即可

	
void setup_vm_final(void) {
	uint64 stext_ptr = (uint64)&_stext;
	uint64 srodata_ptr = (uint64)&_srodata;
	uint64 sdata_ptr = (uint64)&_sdata;

    memset(swapper_pg_dir, 0x0, PGSIZE); // 清空页表
	uint64* pgdir_ptr = (uint64*)swapper_pg_dir;
    // No OpenSBI mapping required
    // mapping kernel text X|-|R|V
	// text段
	uint64 size ;
	size = srodata_ptr - stext_ptr;
    create_mapping(pgdir_ptr, stext_ptr, stext_ptr - PA2VA_OFFSET, size, 11);


    // mapping kernel rodata -|-|R|V
	// rodata段
	size = sdata_ptr - srodata_ptr;
    create_mapping(pgdir_ptr, srodata_ptr, srodata_ptr - PA2VA_OFFSET, size, 3);


    // mapping other memory -|W|R|V
	// data段
	// 所有余下物理内存 (128M - rodata - text) 进行映射
	size = PHY_SIZE - (sdata_ptr - stext_ptr );
    create_mapping(pgdir_ptr, (uint64)&_sdata, sdata_ptr - PA2VA_OFFSET, size, 7);


    // set satp with swapper_pg_dir
	// mode 8 代表 Sv39
	// 低 60 bit 为页表的物理地址
    uint64 _satp = (((uint64)(swapper_pg_dir) - PA2VA_OFFSET) >> 12) | (8L << 60);
	printk("satp: %lx\n", _satp);
	csr_write(satp, _satp);

    // flush TLB
    asm volatile("sfence.vma zero, zero");
	// flush icache
    asm volatile("fence.i");

    printk("setup_vm_final !\n");
	
    return;
}

void setup_vm_final_2(void) {
	uint64 stext_ptr = (uint64)&_stext;
	uint64 srodata_ptr = (uint64)&_srodata;
	uint64 sdata_ptr = (uint64)&_sdata;

    memset(swapper_pg_dir, 0x0, PGSIZE); // 清空页表
	uint64* pgdir_ptr = (uint64*)swapper_pg_dir;
    // No OpenSBI mapping required
    // mapping kernel text X|-|R|V
	// text段
	uint64 size ;
	size = srodata_ptr - stext_ptr;
    create_mapping_2(pgdir_ptr, stext_ptr, stext_ptr - PA2VA_OFFSET, size, 11);


    // mapping kernel rodata -|-|R|V
	// rodata段
	size = sdata_ptr - srodata_ptr;
    create_mapping_2(pgdir_ptr, srodata_ptr, srodata_ptr - PA2VA_OFFSET, size, 3);


    // mapping other memory -|W|R|V
	// data段
	// 所有余下物理内存 (128M - rodata - text) 进行映射
	size = PHY_SIZE - (sdata_ptr - stext_ptr );
    create_mapping_2(pgdir_ptr, (uint64)&_sdata, sdata_ptr - PA2VA_OFFSET, size, 7);


    // set satp with swapper_pg_dir
	// mode 8 代表 Sv39
	// 低 60 bit 为页表的物理地址
    uint64 _satp = (((uint64)(swapper_pg_dir) - PA2VA_OFFSET ) >> 12) | (8L << 60);
	printk("satp: %lx\n", _satp);
	csr_write(satp, _satp);

    // flush TLB
    asm volatile("sfence.vma zero, zero");
	// flush icache
    asm volatile("fence.i");

    printk("setup_vm_final !\n");
	
    return;
}


/*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小，单位为字节
    perm 为映射的权限 (即页表项的低 8 位)

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
*/

void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    /*
    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */
   	// PA >> 12 == PPN
	// physical address
	// PPN2 PPN1 PPN0 RSW D A G U X W R V 后10bit 是 flag
	// 26   9    9    2   1 1 1 1 1 1 1 1
	// 53   27   18   9   7 6 5 4 3 2 1 0 
	// 
	printk("根页表地址 %lx, 虚拟地址[%lx, %lx) -> 物理地址[%lx, %lx), 权限: %x\n", pgtbl, va, va+sz,pa, pa+sz, perm);
    uint64 va_end = va + sz;
    uint64 *temtbl, temvpn, tempte;
    while (va < va_end) {
        temtbl = pgtbl; // 根页表的物理地址
		temvpn = VPN2(va); // 当前虚拟地址的VPN2
		tempte = temtbl[temvpn]; // 取出根页表中的对应项
		if ((tempte & PTE_V) == 0) { // 如果V bit 为 0， 则需要分配一个页作为页表
			uint64 new_page_phy = (uint64)kalloc() - PA2VA_OFFSET; // 分配一个页作为页表，已经自动转化为物理地址
			tempte = ((uint64)new_page_phy >> 12) << 10 | PTE_V; // 设置 V bit 为 1， 并将页表的物理地址写入
			temtbl[temvpn] = tempte; // 这里在二级页表的位置存入了一个一级页表的实际物理地址
		}
		
		temtbl = (uint64*)(((tempte >> 10) << 12) + PA2VA_OFFSET);
		temvpn = VPN1(va); 
		tempte = temtbl[temvpn]; // 取出根页表中的对应项
		if ((tempte & PTE_V) == 0) { // 如果V bit 为 0， 则需要分配一个页作为页表
			uint64 new_page_phy = (uint64)kalloc() - PA2VA_OFFSET; // 分配一个页作为页表
			tempte = ((uint64)new_page_phy >> 12) << 10 | PTE_V; // 设置 V bit 为 1， 并将页表的物理地址写入
			temtbl[temvpn] = tempte; 
		}
		// 查询0级页表
		temtbl = (uint64*)(((tempte >> 10) << 12) + PA2VA_OFFSET); 
		temvpn = VPN0(va); // 取出vpn0
		tempte = ((pa >> 12) << 10) | perm | PTE_V; // 设置 V bit 为 1， 并将页表的物理地址写入
		temtbl[temvpn] = tempte; 

        va += PGSIZE;
        pa += PGSIZE;
    }
}

void create_mapping_2(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    uint64 *second_pgtbl, *third_pgtbl;
	uint64 va_end = va + sz;
    while (va < va_end) {
        int vpn2 = VPN2(va);
        int vpn1 = VPN1(va);
        int vpn0 = VPN0(va);

        // 顶级页表处理
        if (pgtbl[vpn2] & PTE_V) { 
			// 如果存在，物理地址转虚拟地址 这一点很重要，因为此时只能用虚拟地址来找页表实际的位置
            second_pgtbl = (uint64 *)((pgtbl[vpn2] >> 10 << 12) + PA2VA_OFFSET);
        } else {
            second_pgtbl = (uint64 *)kalloc(); //否则 直接申请，这里申请到的是虚拟地址，填入一个物理地址
            pgtbl[vpn2] = (((uint64)second_pgtbl - PA2VA_OFFSET) >> 12 << 10) | PTE_V;
        }

        // 二级页表处理
        if (second_pgtbl[vpn1] & PTE_V) {
			// 和上面同理
            third_pgtbl = (uint64 *)((second_pgtbl[vpn1] >> 10 << 12) + PA2VA_OFFSET);
        } else {
            third_pgtbl = (uint64 *)kalloc();
            second_pgtbl[vpn1] = (((uint64)third_pgtbl - PA2VA_OFFSET) >> 12 << 10) | PTE_V;
        }

        // 三级页表处理
        if (!(third_pgtbl[vpn0] & PTE_V)) {
            third_pgtbl[vpn0] = (pa >> 12 << 10) | perm | PTE_V;
        }

        // 移动到下一个页面
        va += PGSIZE;
        pa += PGSIZE;
    }
}



uint64 check_pte(uint64 *pgtbl, uint64 va ){
    uint64 *temtbl, temvpn, tempte;
        temtbl = pgtbl; // 根页表
		temvpn = VPN2(va); // 查询二级页表
		tempte = temtbl[temvpn]; // 取出根页表中的对应项
		
		temtbl = (uint64*)(((tempte >> 10) << 12) + PA2VA_OFFSET);
		temvpn = VPN1(va); // 查询一级页表
		tempte = temtbl[temvpn]; 

		// 查询0级页表
		temtbl = (uint64*)(((tempte >> 10) << 12) + PA2VA_OFFSET); // 页表的物理地址
		temvpn = VPN0(va); // 取出vpn0
		tempte = temtbl[temvpn] ; // 填入页表项
	return tempte;
}