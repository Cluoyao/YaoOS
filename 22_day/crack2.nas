[INSTRSET "i486p"]				; 使用到486为止的指令
[BITS 32]						; 3制作32位模式用的机器语言

		MOV		EAX,1*8
        MOV     DS,AX
        MOV     BYTE [0X102600],0
        MOV     EDX,4
        INT     0x40