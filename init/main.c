#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();
extern void schedule();
extern uint64 _stext;
extern uint64 _srodata;

int start_kernel() {
    printk("2022");
    printk(" Hello RISC-V\n");
	//启动内核之后立刻调度
	schedule();
	
	// printk("text rodata 测试\n");
	// uint64 *_stextptr = &_stext;
	// uint64 *_srodataptr = &_srodata;
	// printk("_stext = %x\n", *_stextptr);       
	// printk("_srodata = %x\n", *_srodataptr);
	// *_stextptr = 0x12345678;                            
	// *_srodataptr = 0x12345678;
	// printk("_stext = %x\n", *_stextptr);
	// printk("_srodata = %x\n", *_srodataptr);


	// uint64 value = csr_read(sstatus);
	// printk("before write value: 0x%lx\n", value);
	// uint64 written_value = 0xffffffffffffffff ;
	// csr_write(sstatus,written_value);
	// value = csr_read(sstatus);
	// printk("sstatus=0x%lx\n", value);
	// printk("after write value 0x%lx: 0x%lx\n",written_value, value);
    
	test(); // DO NOT DELETE !!!

	return 0;
}
