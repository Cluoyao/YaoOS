[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL  _api_alloctimer

[SECTION .text]

_api_alloctimer:	; int api_alloctimer(void)
		MOV		EDX,16
		INT		0x40   ;系统函数调用
		RET