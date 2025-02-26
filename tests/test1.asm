;****************************************************************
;
;  DECLARATION OF AUTHORSHIP
;
;  I hereby declare that this source file is my own unaided work.
;
;  Tejas Tanmay Singh
;  2301AI30
;
;****************************************************************

; Computes nth Fibonacci number after initial sequence of 0, 1
; Result stored in 'b'

loop:
	ldl n
	ldl i
	sub
	brz halt

	ldl b
	stl tmp

	ldl b
	ldl a
	add
	stl b

	ldl tmp
	stl a

	ldl i
	adc 1
	stl i
	br loop

halt:
	HALT

n:	data 10
i:	data 0
tmp:	data 0
a:	data 0
b:	data 1
