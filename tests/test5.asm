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

; Demonstrates the 'SET' instruction

symbol: SET 0x1234

ldc symbol
stl value
HALT

value: data 0
