[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL	_api_initmalloc

[SECTION .text]

_api_initmalloc:      ; void api_initmalloc(void);
		PUSH	EBX
		MOV 	EDX, 8
		MOV		EBX,[CS:0x0020]	; malloc内存空间的地址
		MOV		EAX,EBX
		ADD		EAX,32*1024		; 加上32kb
		MOV		ECX,[CS:0x0000] ; 数据段的大小
		SUB		ECX,EAX
		INT		0x40
		POP		EBX
		RET