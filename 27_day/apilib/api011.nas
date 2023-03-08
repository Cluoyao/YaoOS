[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL	_api_refreshwin

[SECTION .text]

_api_refreshwin:	; void api_refreshwin(int win, int x0, int y0, int x1, int y1);
		PUSH	EDI
		PUSH	ESI
		PUSH	EBX
		MOV		EDX,12
		MOV		EBX,[ESP+16]	;win
		MOV		EAX,[ESP+20]	;x0
		MOV		ECX,[ESP+24]	;Y0
		MOV		ESI,[ESP+28]	;X1
		MOV		EDI,[ESP+32]	;Y1
		INT		0x40
		POP		EBX
		POP		ESI
		POP		EDI
		RET