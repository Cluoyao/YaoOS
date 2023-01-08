[FORMAT "WCOFF"]				; 制作目标文件的模式	
[INSTRSET "i486p"]				; 使用到486为止的指令
[BITS 32]						; 3制作32位模式用的机器语言
[FILE "a_nask.nas"]			    ; 文件名

		GLOBAL	_api_putchar
        GLOBAL  _api_end

[SECTION .text]

_api_putchar:                   ;void api_putchar(int c);
        MOV    EDX, 1
        MOV    AL, [ESP + 4]
        INT    0x40
        RET

_api_end:                   ;void api_end();
        MOV    EDX,4
        INT    0x40