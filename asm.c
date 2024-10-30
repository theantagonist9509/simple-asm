#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COL_RED		"\033[1;31m"
#define COL_PUR		"\033[1;35m"
#define COL_WHITE	"\033[1;37m"
#define COL_END		"\033[0m"

char const	*src_name;
FILE		*src;
int		line_no;

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

Buf line	= { .cap = 1 };
Buf out_name	= { .cap = 1 };
Buf out_buf	= { .cap = 1 };

// Only do codegen for current file if no syntax error
bool syn_err;

typedef struct {
	char const	*mnem;
	bool		op;
} Ins;

Ins const ins[] = {
	(Ins) {
		.mnem	= "ldc",
		.op	= true,
	},
	(Ins) {
		.mnem	= "adc",
		.op	= true,
	},
	(Ins) {
		.mnem	= "ldl",
		.op	= true,
	},
	(Ins) {
		.mnem	= "stl",
		.op	= true,
	},
	(Ins) {
		.mnem	= "ldnl",
		.op	= true,
	},
	(Ins) {
		.mnem	= "stnl",
		.op	= true,
	},
	(Ins) {
		.mnem	= "add",
		.op	= false,
	},
	(Ins) {
		.mnem	= "sub",
		.op	= false,
	},
	(Ins) {
		.mnem	= "shl",
		.op	= false,
	},
	(Ins) {
		.mnem	= "shr",
		.op	= false,
	},
	(Ins) {
		.mnem	= "adj",
		.op	= true,
	},
	(Ins) {
		.mnem	= "a2sp",
		.op	= false,
	},
	(Ins) {
		.mnem	= "sp2a",
		.op	= false,
	},
	(Ins) {
		.mnem	= "call",
		.op	= true,
	},
	(Ins) {
		.mnem	= "return",
		.op	= false,
	},
	(Ins) {
		.mnem	= "brz",
		.op	= true,
	},
	(Ins) {
		.mnem	= "brlz",
		.op	= true,
	},
	(Ins) {
		.mnem	= "br",
		.op	= true,
	},
	(Ins) {
		.mnem	= "HALT",
		.op	= false,
	},
};
#define NUM_INS		(sizeof (ins) / sizeof (Ins))
#define BR_BEGIN_IDX	13
#define BR_END_IDX	17

Ins const ps_ins[] = {
	(Ins) {
		.mnem	= "data",
		.op	= true,
	},
	(Ins) {
		.mnem	= "SET",
		.op	= true,
	},
};
#define NUM_PS_INS	(sizeof (ps_ins) / sizeof (Ins))
#define DATA_IDX	0
#define SET_IDX		1

typedef struct {
	char	*name;
	int	name_len;
	int	line_no;

	// out_buf word index where label is defined/used
	int	word_idx;

	union {
		// Was this label definition used (for unused label warning)
		bool	used;

		// Was this label used in a branch instruction (if so, push PC displacement)
		bool	br;
	};
} Label;

typedef struct {
	Label	*data;
	int	cap;
	int	len;
} LabelBuf;

void pushLabel(LabelBuf *buf, Label lab)
{
	if (buf->len < buf->cap) {
		buf->data[buf->len] = lab;
		buf->len++;
		return;
	}

	buf->data = realloc(buf->data, 2 * buf->cap * sizeof (Label));
	if (buf->data == NULL) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "realloc() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	buf->data[buf->len] = lab;
	buf->cap *= 2;
	buf->len++;
}

LabelBuf defs = { .cap = 1 };
LabelBuf uses = { .cap = 1 };

// Whether the current line has a label (for SET pseudo instruction)
bool has_lab;

bool isStrzStrnEq(char const *str, char const *strn, int n)
{
	int i = 0;
	while (str[i] != 0 && i < n) {
		if (str[i] != strn[i]) {
			return false;
		}

		i++;
	}

	if (str[i] != 0 || i < n) {
		return false;
	}

	return true;
}

void parseSingle(char const *sym, int len)
{
	for (int i = 0; i < NUM_INS; i++) {
		if (isStrzStrnEq(ins[i].mnem, sym, len)) {
			if (ins[i].op) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "expected operand to instruction\n"
					"	%.*s\n",
					src_name,
					line_no,
					len,
					sym
				);
				syn_err = true;
				return;
			}

			push(&out_buf, i);
			push(&out_buf, 0);
			push(&out_buf, 0);
			push(&out_buf, 0);
			return;
		}
	}

	for (int i = 0; i < NUM_PS_INS; i++) {
		if (isStrzStrnEq(ps_ins[i].mnem, sym, len)) {
			if (ps_ins[i].op) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "expected operand to pseudo instruction\n"
					"	%.*s\n",
					src_name,
					line_no,
					len,
					sym
				);
				syn_err = true;
				return;
			}

			fprintf(stderr, COL_RED "bug: " COL_END "unimplemented pseudo instruction: idx: %d\n", i);
			exit(EXIT_FAILURE);

			return;
		}
	}

	fprintf(
		stderr,
		COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unknown instruction\n"
		"	%.*s\n",
		src_name,
		line_no,
		len,
		sym
	);
	syn_err = true;
}

int parseUpToBaseTen(int base, char const *str, int len)
{
	int num = 0;
	int mul = 1;
	for (int i = len - 1; i >= 0; i--) {
		if (!(str[i] >= '0' && str[i] < '0' + base)) {
			fprintf(
				stderr,
				COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected character '%c'\n"
				"	%.*s\n",
				src_name,
				line_no,
				str[i],
				len,
				str
			);
			syn_err = true;
			return 0;
		}

		num += (str[i] - '0') * mul;
		mul *= base;
	}

	return num;
}

int parseHex(char const *str, int len)
{
	int num = 0;
	int mul = 1;
	for (int i = len - 1; i >= 0; i--) {
		if (!(str[i] >= '0' && str[i] <= '9' || str[i] >= 'A' && str[i] <= 'F' || str[i] >= 'a' && str[i] <= 'f')) {
			fprintf(
				stderr,
				COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected character '%c'\n"
				"	%.*s\n",
				src_name,
				line_no,
				str[i],
				len,
				str
			);
			syn_err = true;
			return 0;
		}

		int digit;
		if (str[i] <= '9') {
			digit = str[i] - '0';
		} else if (str[i] <= 'F') {
			digit = str[i] - 'A' + 10;
		} else {
			digit = str[i] - 'a' + 10;
		}

		num += digit * mul;
		mul *= 16;
	}

	return num;
}

int parseNum(char const *str, int len)
{
	int sign = 1;
	if (str[0] == '+') {
		str++;
		len--;
	} else if (str[0] == '-') {
		sign = -1;
		str++;
		len--;
	}

	if (len == 1) {
		if (!(str[0] >= '0' && str[0] <= '9')) {
			fprintf(
				stderr,
				COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected character '%c'\n"
				"	%.*s\n",
				src_name,
				line_no,
				str[0],
				len,
				str
			);
			syn_err = true;
			return 0;
		}

		return sign * (str[0] - '0');
	}

	if (str[0] == '0') {
		if (str[1] == 'x') {
			if (len == 2) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "expected hexadecimal number\n"
					"	0x\n",
					src_name,
					line_no
				);
				syn_err = true;
				return 0;
			}

			return sign * parseHex(str + 2, len - 2);
		}

		return sign * parseUpToBaseTen(8, str + 1, len - 1);
	}

	return sign * parseUpToBaseTen(10, str, len);
}

void *tryMalloc(int len)
{
	void *ret = malloc(len);
	if (ret == NULL) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "malloc() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	return ret;
}

// TODO LISTING FILE

void parseDouble(char const *sym1, int len1, char const *sym2, int len2)
{
	for (int i = 0; i < NUM_INS; i++) {
		if (isStrzStrnEq(ins[i].mnem, sym1, len1)) {
			if (!ins[i].op) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected operand to instruction\n"
					"	%.*s %.*s\n",
					src_name,
					line_no,
					len1,
					sym1,
					len2,
					sym2
				);
				syn_err = true;
				return;
			}

			push(&out_buf, i);

			if (sym2[0] == '+' || sym2[0] == '-' || (sym2[0] >= '0' && sym2[0] <= '9')) {
				int val = parseNum(sym2, len2);
				push(&out_buf, val & 0x0000ff);
				push(&out_buf, (val & 0x00ff00) >> 8);
				push(&out_buf, val >> 16);
				return;
			}

			// Filled in by fillLabels()
			char *name = tryMalloc(len2);
			memcpy(name, sym2, len2);
			pushLabel(&uses, (Label) {
				.name = name,
				.name_len = len2,
				.line_no = line_no,
				.word_idx = out_buf.len / 4,
				.br = i >= BR_BEGIN_IDX && i <= BR_END_IDX,
			});
			push(&out_buf, 0);
			push(&out_buf, 0);
			push(&out_buf, 0);
			return;
		}
	}

	for (int i = 0; i < NUM_PS_INS; i++) {
		if (isStrzStrnEq(ps_ins[i].mnem, sym1, len1)) {
			if (!ps_ins[i].op) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected operand to pseudo instruction\n"
					"	%.*s %.*s\n",
					src_name,
					line_no,
					len1,
					sym1,
					len2,
					sym2
				);
				syn_err = true;
				return;
			}

			if (!(sym2[0] == '+' || sym2[0] == '-' || (sym2[0] >= '0' && sym2[0] <= '9'))) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "expected number literal operand to pseudo instruction\n"
					"	%.*s %.*s\n",
					src_name,
					line_no,
					len1,
					sym1,
					len2,
					sym2
				);
				syn_err = true;
				return;
			}

			int num = parseNum(sym2, len2);

			switch (i) {
				case DATA_IDX:
					push(&out_buf, num & 0x000000ff);
					push(&out_buf, (num & 0x0000ff00) >> 8);
					push(&out_buf, (num & 0x00ff0000) >> 16);
					push(&out_buf, num >> 24);
					return;

				case SET_IDX:
					if (!has_lab) {
						fprintf(
							stderr,
							COL_WHITE "%s:%d: " COL_RED "error: " COL_END "expected label preceeding \'SET\'\n"
							"	%.*s %.*s\n",
							src_name,
							line_no,
							len1,
							sym1,
							len2,
							sym2
						);
						syn_err = true;
						return;
					}

					defs.data[defs.len - 1].word_idx = num;
					return;

				default:
					fprintf(stderr, COL_RED "bug: " COL_END "unimplemented pseudo instruction: idx: %d\n", i);
					exit(EXIT_FAILURE);
			}
		}
	}

	fprintf(
		stderr,
		COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unknown instruction\n"
		"	%.*s %.*s\n",
		src_name,
		line_no,
		len1,
		sym1,
		len2,
		sym2
	);
	syn_err = true;
}

// Handles labels recursively
void parseLine(char const *data, int len, bool parent_has_lab)
{
	if (len == 0) {
		return;
	}

	if (!(data[0] >= 'a' && data[0] <= 'z' || data[0] >= 'A' && data[0] <= 'Z')) {
		fprintf(
			stderr,
			COL_WHITE "%s:%d: " COL_RED "error: " COL_END "label names must begin with a letter\n"
			"	%.*s\n",
			src_name,
			line_no,
			len,
			data
		);
		syn_err = true;
		return;
	}

	int i = 1;
	while (i < len && (data[i] >= 'a' && data[i] <= 'z' || data[i] >= 'A' && data[i] <= 'Z' || data[i] >= '0' && data[i] <= '9')) {
		i++;
	}

	if (i == len) {
		has_lab = parent_has_lab;

		parseSingle(data, len);
		return;
	}

	if (data[i] == ':') {
		has_lab = true;

		for (int j = 0; j < defs.len; j++) {
			if (defs.data[j].name_len == i && strncmp(defs.data[j].name, data, i) == 0) {
				fprintf(
					stderr,
					COL_WHITE "%s:%d: " COL_RED "error: " COL_END "duplicate label definition\n"
					"	%.*s\n",
					src_name,
					line_no,
					len,
					data
				);
				syn_err = true;
				goto next_line;
			}
		}

		// Allocate separate buffers for storing label names (line buffer gets overwritten)
		char *name = tryMalloc(i);
		memcpy(name, data, i);
		pushLabel(&defs, (Label) {
			.name = name,
			.name_len = i,
			.line_no = line_no,
			.word_idx = out_buf.len / 4,
			.used = false,
		});

next_line:
		i++;
		if (i == len) {
			return;
		}

		while (data[i] == ' ' || data[i] == '\t') {
			i++;
		}

		parseLine(data + i, len - i, true);
		return;
	}

	has_lab = parent_has_lab;

	int j = i;
	while (data[j] == ' ' || data[j] == '\t') {
		j++;
	}

	int k = j;
	while (k < len && (data[k] == '+' || data[k] == '-' || data[k] >= 'a' && data[k] <= 'z' || data[k] >= 'A' && data[k] <= 'Z' || data[k] >= '0' && data[k] <= '9')) {
		k++;
	}

	if (k < len) {
		fprintf(
			stderr,
			COL_WHITE "%s:%d: " COL_RED "error: " COL_END "unexpected character '%c'\n"
			"	%.*s\n",
			src_name,
			line_no,
			data[k],
			len,
			data
		);
		syn_err = true;
		return;
	}

	parseDouble(data, i, data + j, len - j);
}

void fillLabels()
{
	for (int i = 0; i < uses.len; i++) {
		for (int j = 0; j < defs.len; j++) {
			if (!(defs.data[j].name_len == uses.data[i].name_len && strncmp(defs.data[j].name, uses.data[i].name, defs.data[j].name_len) == 0)) {
				continue;
			}

			defs.data[j].used = true;

			int use_word_idx = uses.data[i].word_idx;
			int def_word_idx = defs.data[j].word_idx;

			int write;
			if (uses.data[i].br) {
				write = def_word_idx - (use_word_idx + 1);
			} else {
				write = def_word_idx;
			}

			out_buf.data[4 * use_word_idx + 1] = write & 0x0000ff;
			out_buf.data[4 * use_word_idx + 2] = (write & 0x00ff00) >> 8;
			out_buf.data[4 * use_word_idx + 3] = write >> 16;
			goto next_use;
		}

		fprintf(
			stderr,
			COL_WHITE "%s:%d: " COL_RED "error: " COL_END "undefined label\n"
			"	%.*s\n",
			src_name,
			uses.data[i].line_no,
			uses.data[i].name_len,
			uses.data[i].name
		);
		syn_err = true;

next_use:	{}
	}

	for (int i = 0; i < defs.len; i++) {
		if (!defs.data[i].used) {
			fprintf(
				stderr,
				COL_WHITE "%s:%d: " COL_PUR "warning: " COL_END "unused label\n"
				"	%.*s\n",
				src_name,
				defs.data[i].line_no,
				defs.data[i].name_len,
				defs.data[i].name
			);
		}
	}
}

void writeAll(int fd, char const *data, int len)
{
	int written = 0;
	int tot_written = 0;
	while (tot_written < len && written >= 0) {
		tot_written += written;
		written = write(fd, data + tot_written, len - tot_written);
	}

	if (written < 0) {
		fprintf(stderr, COL_RED "fatal error: " COL_END "failed to write to output file '%.*s': %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	if (argc == 1) {
		fprintf(
			stderr,
			COL_RED "fatal error: " COL_END "no input files\n"
			"usage: %s <files>\n",
			argv[0]
		);
		return EXIT_FAILURE;
	}

	line.data = tryMalloc(1);
	out_name.data = tryMalloc(1);
	out_buf.data = tryMalloc(1);
	defs.data = tryMalloc(sizeof (Label));
	uses.data = tryMalloc(sizeof (Label));

	int exit_code = EXIT_SUCCESS;

	for (int i = 1; i < argc; i++) {
		src_name = argv[i];
		src = fopen(src_name, "r");
		if (src == NULL) {
			fprintf(stderr, COL_RED "fatal error: " COL_END "failed to open file '%s': %s\n", src_name, strerror(errno));
			return EXIT_FAILURE;
		}

		line_no = 1;
		out_buf.len = 0;
		defs.len = 0;
		uses.len = 0;
		while (true) {
			// Push stripped line into buffer
			line.len = 0;

			int c;
			do {
				c = fgetc(src);
			} while (c == ' ' || c == '\t');

			while (!(c == EOF || c == '\n' || c == ';')) {
				push(&line, c);
				c = fgetc(src);
			}

			while (line.len > 0 && (line.data[line.len - 1] == ' ' || line.data[line.len - 1] == '\t')) {
				line.len--;
			}

			parseLine(line.data, line.len, false);

			// Go to next file/line
			if (c == EOF) {
				goto eof;
			}

			if (c == ';') {
				while (true) {
					c = fgetc(src);
					if (c == EOF) {
						goto eof;
					}

					if (c == '\n') {
						break;
					}
				}
			}

			line_no++;
		}

eof:
		fillLabels();

		if (syn_err) {
			exit_code = EXIT_FAILURE;
		} else {
			out_name.len = 0;
			while (src_name[out_name.len] != 0 && src_name[out_name.len] != '.') {
				push(&out_name, src_name[out_name.len]);
			}
			push(&out_name, '.');
			push(&out_name, 'o');

			int out = creat(out_name.data, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (out < 0) {
				fprintf(stderr, COL_RED "fatal error: " COL_END "failed to create output file '%.*s': %s\n", out_name.len, out_name.data, strerror(errno));
				return EXIT_FAILURE;
			}

			writeAll(out, out_buf.data, out_buf.len);

			if (close(out) < 0) {
				fprintf(stderr, COL_RED "fatal error: " COL_END "failed to close output file '%.*s': %s\n", out_name.len, out_name.data, strerror(errno));
				return EXIT_FAILURE;
			}
		}

		if (fclose(src) != 0) {
			fprintf(stderr, COL_RED "fatal error: " COL_END "failed to close file '%s': %s\n", src_name, strerror(errno));
			return EXIT_FAILURE;
		}

		for (int j = 0; j < defs.len; j++) {
			free(defs.data[j].name);
		}
		for (int j = 0; j < uses.len; j++) {
			free(uses.data[j].name);
		}
	}

	free(line.data);
	free(out_name.data);
	free(out_buf.data);
	free(defs.data);
	free(uses.data);
	return exit_code;
}