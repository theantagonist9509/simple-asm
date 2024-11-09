loopI:
	ldl i
	ldl len
	sub
	brz halt

	ldc 0
	stl j

loopJ:
	ldl j
	ldl len
	adc -1
	sub
	brz nextI

	ldl j
	ldnl arr
	ldl j
	adc 1
	ldnl arr
	sub
	brlz nextJ

	ldl j
	ldnl arr
	stl tmp

	ldl j
	adc 1
	ldnl arr
	ldl j
	stnl arr

	ldl tmp
	ldl j
	adc 1
	stnl arr

nextJ:
	ldl j
	adc 1
	stl j
	br loopJ

nextI:
	ldl i
	adc 1
	stl i
	br loopI

halt:
	HALT

len:	data 10
arr:
	data 10
	data 9
	data 8
	data 7
	data 6
	data 5
	data 4
	data 3
	data 2
	data 1

i:	data 0
j:	data 0
tmp:	data 0 ; for swapping
