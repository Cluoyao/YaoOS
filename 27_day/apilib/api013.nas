[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL	_api_closewin

[SECTION .text]

_api_closewin:	;void api_closewin(int win);
		PUSH	EBX
		MOV		EDX,14
		MOV 	EBX,[ESP+8] 	;WIN
		INT		0x40
		POP		EBX
		RET