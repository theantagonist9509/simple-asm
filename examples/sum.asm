loop:
	ldl i
	ldl len
	sub
	brz halt

	ldl sum
	ldl i
	ldnl arr
	add
	stl sum

	ldl i
	adc 1
	stl i
	br loop

halt:
	HALT

len:	data 10
arr:
	data 1
	data 1
	data 2
	data 2
	data 3
	data 3
	data 4
	data 4
	data 5
	data 5

i:	data 0
sum:	data 0
