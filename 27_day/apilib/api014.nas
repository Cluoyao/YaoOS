[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL  _api_getkey

[SECTION .text]

_api_getkey:	; int api_getkey(int mode)
		MOV		EDX,15
		MOV		EAX,[ESP+4]	;mode
		INT		0x40   
		RET