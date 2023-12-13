#include "types.h"
#include "sbi.h"

struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
    struct  sbiret ret;
	//将 arg0 ~ arg5 放入寄存器 a0 ~ a5 中
	register uint64 a0 asm("a0") = (uint64)(arg0);
	register uint64 a1 asm("a1") = (uint64)(arg1);
	register uint64 a2 asm("a2") = (uint64)(arg2);
	register uint64 a3 asm("a3") = (uint64)(arg3);
	register uint64 a4 asm("a4") = (uint64)(arg4);
	register uint64 a5 asm("a5") = (uint64)(arg5);
	register uint64 a6 asm("a6") = (uint64)(fid);
	//fid (Function ID) 放入寄存器 a6 中
	register uint64 a7 asm("a7") = (uint64)(ext);
	// ext (Extension ID) 放入寄存器 a7 中

	//内联汇编语句，用于触发 SBI 调用
	//volatile 关键字告诉编译器不要对这段代码进行优化，确保其按照指定的顺序执行
	asm volatile(
		"ecall"//执行一个 SBI 调用
		
		: "+r" (a0) , "+r" (a1) //授权读写
		: "r" (a2) , "r" (a3) , "r" (a4) ,"r" (a5) , "r" (a6) ,"r" (a7) // 授权读
		: "memory" // 刷新内存中的变化
	);
	// OpenSBI 的返回结果会存放在寄存器 a0 ， a1 中，其中 a0 为 error
	// code， a1 为返回值
	ret.error = a0;
	ret.value = a1;
	return ret;
	          
}



