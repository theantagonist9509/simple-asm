/*****************************************************************
*
*  DECLARATION OF AUTHORSHIP
*
*  I hereby declare that this source file is my own unaided work.
*
*  Tejas Tanmay Singh
*  2301AI30
*
*****************************************************************/

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

void exec(bool print)
{
	int a = 0;
	int b = 0;
	int pc = 0;
	int sp = 0;
	while (pc >= 0 && pc < mem.len / 4) {
		int word = *(int *) (mem.data + 4 * pc);
		int ins = word & 0xff;
		int op = word >> 8;
		switch (ins) {
			case 0:
				b = a;
				a = op;
				break;
			case 1:
				a += op;
				break;
			case 2:
				b = a;
				a = *(int *) (mem.data + 4 * (sp + op));
				break;
			case 3:
				*(int *) (mem.data + 4 * (sp + op)) = a;
				a = b;
				break;
			case 4:
				a = *(int *) (mem.data + 4 * (a + op));
				break;
			case 5:
				*(int *) (mem.data + 4 * (a + op)) = b;
				break;
			case 6:
				a += b;
				break;
			case 7:
				a = b - a;
				break;
			case 8:
				a = b << a;
				break;
			case 9:
				a = b >> a;
				break;
			case 10:
				sp += op;
				break;
			case 11:
				sp = a;
				a = b;
				break;
			case 12:
				b = a;
				a = sp;
				break;
			case 13:
				b = a;
				a = pc;
				pc += op;
				break;
			case 14:
				pc = a;
				a = b;
				break;
			case 15:
				pc += (a == 0) * op;
				break;
			case 16:
				pc += (a < 0) * op;
				break;
			case 17:
				pc += op;
				break;
			case 18:
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

	fprintf(stderr, COL_RED "error: " COL_END "pc=0x%08x is out of bounds\n", pc);
	exit(EXIT_FAILURE);
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
				fprintf(stderr, COL_RED "error: " COL_END "insufficient bytes at word address 0x%08x\n", mem.len / 4);
				return EXIT_FAILURE;
			}

			push(&mem, c);
		}
	}

	switch (opt) {
		case 0:
			exec(true);
			break;
		case 1:
			printMem();
			break;
		case 2:
			exec(false);
			printMem();
			break;
		default:
			fprintf(stderr, COL_RED "bug: " COL_END "unimplemented option: idx: %d\n", opt);
			return EXIT_FAILURE;
	}

	free(mem.data);
	if (fclose(file) != 0) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "failed to close file '%s': %s\n", argv[2], strerror(errno));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
