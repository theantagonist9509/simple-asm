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

; Compute n!
; Result stored in 'prod'

loop:
	ldl i
	stl a
	ldl prod
	stl b
	call mul
	ldl retVal
	stl prod

	ldl i
	adc 1
	stl i

	ldl n
	ldl i
	sub
	brlz halt
	br loop

halt:
	HALT

n:	data 5
prod:	data 1
i:	data 1




mul:
	stl retAddr

	ldc 0
	stl retVal
	ldc 0
	stl cnt

mulLoop:
	ldl a
	ldl cnt
	sub
	brz return

	ldl b
	ldl retVal
	add
	stl retVal

	ldl cnt
	adc 1
	stl cnt
	br mulLoop

return:
	ldl retAddr
	return

retAddr:	data 0
retVal:		data 0
a:		data 0
b:		data 0
cnt:		data 0
