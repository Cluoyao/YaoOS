[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL  _api_settimer

[SECTION .text]

_api_settimer:		;void api_settimer(int timer, int time);
		PUSH	EBX
		MOV		EDX,18
		MOV		EBX,[ESP+8]
		MOV		EAX,[ESP+12]
		INT		0x40
		POP		EBX
		RET