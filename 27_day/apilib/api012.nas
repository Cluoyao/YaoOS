[FORMAT "WCOFF"]			; 生成对象文件的模式
[INSTRSET "i486p"]		; 表示使用486兼容指令集
[BITS 32]							; 生成32位模式机器语言
[FILE "a_nask.nas"]		; 源文件名信息

		GLOBAL  _api_linewin

[SECTION .text]

_api_linewin:		; void api_linewin(int win, int x0, int y0, int x1, int y1, int col);
		PUSH	EDI
		PUSH	ESI
		PUSH	EBP
		PUSH	EBX
		MOV		EDX,13
		MOV		EBX,[ESP+20]	;WIN
		MOV		EAX,[ESP+24]	;X0
		MOV		ECX,[ESP+28]	;Y0
		MOV		ESI,[ESP+32]	;X1
		MOV		EDI,[ESP+36]	;Y1
		MOV		EBP,[ESP+40]	;COL
		INT		0x40
		POP		EBX
		POP		EBP
		POP		ESI
		POP		EDI
		RET