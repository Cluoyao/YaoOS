[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL 	_api_malloc

[SECTION .text]

_api_malloc:      ; char* api_malloc(int size);
		PUSH	EBX
		MOV 	EDX, 9
		MOV		EBX,[CS:0x0020]	; malloc内存空间的地址
		MOV		ECX,[ESP+8] ; size
		INT		0x40
		POP		EBX
		RET
