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

; Computes a / b and a % b through repeated subtraction
; Quotient stored in 'quo'
; Remainder stored in 'a'

ldc 71
stl a
ldc 13
stl b
call div
HALT

div:
	stl retAddr

	ldc 0
	stl quo

divLoop:
	ldl a
	ldl b
	sub
	brlz return
	stl a

	ldl quo
	adc 1
	stl quo
	br divLoop

return:
	ldl retAddr
	return

retAddr:	data 0
quo:		data 0
a:		data 0
b:		data 0
