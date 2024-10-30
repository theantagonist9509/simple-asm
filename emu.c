#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define COL_RED "\033[1;31m"
#define COL_END "\033[0m"

char const *const opts[] = {
	"-trace",
	"-before",
	"-after",
};
#define TRACE_IDX	0
#define BEFORE_IDX	1
#define AFTER_IDX	2

typedef struct {
	char	*data;
	int	cap;
	int	len;
} Buf;

void push(Buf *buf, char c)
{
	if (buf->len < buf->cap) {
		buf->data[buf->len] = c;
		buf->len++;
		return;
	}

	buf->data = realloc(buf->data, 2 * buf->cap);
	if (buf->data == NULL) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "realloc() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	buf->data[buf->len] = c;
	buf->cap *= 2;
	buf->len++;
}

Buf mem = { .cap = 1 };

void printMem()
{
	printf("(big endian)\n");

	int i = 0;
	for (; i < mem.len / 4; i++) {
		if (i % 4 == 0) {
			printf("%08x: ", i);
		}

		printf("%08x%c", *(int *) (mem.data + 4 * i), (i % 4 == 3 ? '\n' : ' '));
	}

	if (i % 4 != 0) {
		printf("\n");
	}
}

#define LDC_IDX		0
#define ADC_IDX		1
#define LDL_IDX		2
#define STL_IDX		3
#define LDNL_IDX	4
#define STNL_IDX	5
#define ADD_IDX		6
#define SUB_IDX		7
#define SHL_IDX		8
#define SHR_IDX		9
#define ADJ_IDX		10
#define A2SP_IDX	11
#define SP2A_IDX	12
#define CALL_IDX	13
#define RETURN_IDX	14
#define BRZ_IDX		15
#define BRLZ_IDX	16
#define BR_IDX		17
#define HALT		18

void exec(bool print)
{
	int a = 0;
	int b = 0;
	int pc = 0;
	int sp = 0;
	while (true) {
		int word = *(int *) (mem.data + 4 * pc);
		int ins = word & 0xff;
		int op = word >> 8;
		switch (ins) {
			case LDC_IDX:
				b = a;
				a = op;
				break;
			case ADC_IDX:
				a += op;
				break;
			case LDL_IDX:
				b = a;
				a = *(int *) (mem.data + 4 * (sp + op));
				break;
			case STL_IDX:
				*(int *) (mem.data + 4 * (sp + op)) = a;
				a = b;
				break;
			case LDNL_IDX:
				a = *(int *) (mem.data + 4 * (a + op));
				break;
			case STNL_IDX:
				*(int *) (mem.data + 4 * (a + op)) = b;
				break;
			case ADD_IDX:
				a += b;
				break;
			case SUB_IDX:
				a = b - a;
				break;
			case SHL_IDX:
				a = b << a;
				break;
			case SHR_IDX:
				a = b >> a;
				break;
			case ADJ_IDX:
				sp += op;
				break;
			case A2SP_IDX:
				sp = a;
				a = b;
				break;
			case SP2A_IDX:
				b = a;
				a = sp;
				break;
			case CALL_IDX:
				b = a;
				a = pc;
				pc += op;
				break;
			case RETURN_IDX:
				pc = a;
				a = b;
				break;
			case BRZ_IDX:
				pc += (a == 0) * op;
				break;
			case BRLZ_IDX:
				pc += (a < 0) * op;
				break;
			case BR_IDX:
				pc += op;
				break;
			case HALT:
				return;
			default:
				fprintf(stderr, COL_RED "error: " COL_END "unknown instruction with code 0x%02x at pc=0x%08x\n", ins, pc);
				exit(EXIT_FAILURE);
		}

		pc++;

		if (print) {
			printf(
				"a	: %d\n"
				"b	: %d\n"
				"pc	: %d\n"
				"sp	: %d\n"
				"\n",
				a,
				b,
				pc,
				sp
			);
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(
			stderr,
			COL_RED "fatal error: " COL_END "incorrect usage\n"
			"usage: %s <option> <file>\n"
			"options:\n"
			"	-trace	show instruction trace\n"
			"	-before	show memory dump before execution\n"
			"	-after	show memory dump after execution\n",
			argv[0]
		);
		return EXIT_FAILURE;
	}

	int opt = -1;
	for (int i = 0; i < sizeof (opts) / sizeof (char *); i++) {
		if (strcmp(argv[1], opts[i]) == 0) {
			opt = i;
			break;
		}
	}

	if (opt < 0) {
		fprintf(
			stderr,
			COL_RED "fatal error: " COL_END "unknown option '%s'\n"
			"options:\n"
			"	-trace	show instruction trace\n"
			"	-before	show memory dump before execution\n"
			"	-after	show memory dump after execution\n",
			argv[1]
		);
		return EXIT_FAILURE;
	}

	FILE *file = fopen(argv[2], "r");
	if (file == NULL) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "failed to open file '%s': %s\n", argv[2], strerror(errno));
		return EXIT_FAILURE;
	}

	mem.data = malloc(1);
	if (mem.data == NULL) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "malloc() failed: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	while (true) {
		int c = fgetc(file);
		if (c == EOF) {
			break;
		}

		push(&mem, c);

		for (int i = 0; i < 3; i++) {
			c = fgetc(file);
			if (c == EOF) {
				fprintf(stderr, COL_RED "error: " COL_END "insufficient bytes at word address %d\n", mem.len / 4);
				return EXIT_FAILURE;
			}

			push(&mem, c);
		}
	}

	switch (opt) {
		case BEFORE_IDX:
			printMem();
			break;
		case AFTER_IDX:
			exec(false);
			printMem();
			break;
		case TRACE_IDX:
			exec(true);
			break;
		default:
			fprintf(stderr, COL_RED "bug: " COL_END "unimplemented option: idx: %d\n", opt);
			return EXIT_FAILURE;
	}

	if (fclose(file) != 0) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "failed to close file '%s': %s\n", argv[2], strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
