[BITS 32]
		MOV     AL, 'A'
		CALL    2*8:0xacb
fin:
		HLT
		JMP		fin