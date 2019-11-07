/**********************************************************************
 * Copyright (c) 2019
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

 /*====================================================================*/
 /*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */

 /* To avoid security error on Visual Studio */
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)

#define MAX_NR_TOKENS	32	/* Maximum length of tokens in a command */
#define MAX_TOKEN_LEN	64	/* Maximum length of single token */
#define MAX_COMMAND	256 /* Maximum length of command string */

typedef unsigned char bool;
#define true	1
#define false	0


/**
 * memory[] emulates the memory of the machine
 */
static unsigned char memory[1 << 20] = {	/* 1MB memory at 0x0000 0000 -- 0x0100 0000 */
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0xde, 0xad, 0xbe, 0xef, 0x00, 0x00, 0x00, 0x00,
	'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',
	'r',  'l',  'd',  '!',  '!',  0x00, 0x00, 0x00,
	'a',  'w',  'e',  's',  'o',  'm',  'e',  ' ',
	'c',  'o',  'm',  'p',  'u',  't',  'e',  'r',
	' ',  'a',  'r',  'c',  'h',  'i',  't',  'e',
	'c',  't',  'u',  'r',  'e',  '!',  0x00, 0x00,
};

#define INITIAL_PC	0x1000	/* Initial value for PC register */
#define INITIAL_SP	0x8000	/* Initial location for stack pointer */

/**
 * Registers of the machine
 */
static unsigned int registers[32] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0x10, INITIAL_PC, 0x20, 3, 0xbadacafe, 0xcdcdcdcd, 0xffffffff, 7,
	0, 0, 0, 0, 0, INITIAL_SP, 0, 0,
};

/**
 * Names of the registers. Note that $zero is shorten to zr
 */
const char *register_names[] = {
	"zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

/**
 * Program counter register
 */
static unsigned int pc = INITIAL_PC;

/**
 * strmatch()
 *
 * DESCRIPTION
 *   Compare strings @str and @expect and return 1 if they are the same.
 *   You may use this function to simplify string matching :)
 *
 * RETURN
 *   1 if @str and @expect are the same
 *   0 otherwise
 */
static inline bool strmatch(char * const str, const char *expect)
{
	return (strlen(str) == strlen(expect)) && (strncmp(str, expect, strlen(expect)) == 0);
}

/*          ****** DO NOT MODIFY ANYTHING UP TO THIS LINE ******      */
/*====================================================================*/


/**********************************************************************
 * process_instruction
 *
 * DESCRIPTION
 *   Execute the machine code given through @instr. The following table lists
 *   up the instructions to support. Note that a pseudo instruction 'halt'
 *   (0xffffffff) is added for the testing purpose. Also '*' instrunctions are
 *   the ones that are newly added to PA2.
 *
 * | Name   | Format    | Opcode / opcode + funct |
 * | ------ | --------- | ----------------------- |
 * | `add`  | r-format  | 0 + 0x20                |
 * | `addi` | i-format  | 0x08                    |
 * | `sub`  | r-format  | 0 + 0x22                |
 * | `and`  | r-format  | 0 + 0x24                |
 * | `andi` | i-format  | 0x0c                    |
 * | `or`   | r-format  | 0 + 0x25                |
 * | `ori`  | i-format  | 0x0d                    |
 * | `nor`  | r-format  | 0 + 0x27                |
 * | `sll`  | r-format  | 0 + 0x00                |
 * | `srl`  | r-format  | 0 + 0x02                |
 * | `sra`  | r-format  | 0 + 0x03                |
 * | `lw`   | i-format  | 0x23                    |
 * | `sw`   | i-format  | 0x2b                    |
 * | `slt`  | r-format* | 0 + 0x2a                |
 * | `slti` | i-format* | 0x0a                    |
 * | `beq`  | i-format* | 0x04                    |
 * | `bne`  | i-format* | 0x05                    |
 * | `jr`   | r-format* | 0 + 0x08                |
 * | `j`    | j-format* | 0x02                    |
 * | `jal`  | j-format* | 0x03                    |
 * | `halt` | special*  | @instr == 0xffffffff    |
 *
 * RETURN VALUE
 *   1 if successfully processed the instruction.
 *   0 if @instr is 'halt' or unknown instructions
 */
static int process_instruction(unsigned int instr)
{
	int binary[32] = { 0, }; // instr를 2진수로 변환한 자리값들을 저장할 배열

	for (int i = 31; i >= 0; i--)
	{
		int mask = 1 << i;
		binary[31 - i] = instr & mask ? 1 : 0;
	}

	int OPcode = 0, rs = 0, rt = 0, rd = 0, shamt = 0, func = 0, imm = 0, address = 0;

	int position = 0;
	int decimal = 0;

	for (int i = 5; i >= 0; i--)
	{
		if (binary[i] == 1)
			decimal += 1 << position;

		position++;
	} // OPcode 판별

	/*int imm_position = 0;
	for (int i = 31; i >= 16; i--)
	{
		if (binary[i] == 1)
			imm += 1 << imm_position;

		imm_position++;
	} // imm 값*/
	int rsposition = 0, rtposition = 0;
	for (int i = 10; i >= 6; i--)
	{
		if (binary[i] == 1)
			rs += 1 << rsposition;

		rsposition++;
	} // rs 값
	for (int i = 15; i >= 11; i--)
	{
		if (binary[i] == 1)
			rt += 1 << rtposition;

		rtposition++;
	} // rt 값

	if (decimal == 0) // R포맷일 때
	{
		int R_func = 0;
		int R_position = 0;
		for (int i = 31; i >= 26; i--)
		{
			if (binary[i] == 1)
				R_func += 1 << R_position;

			R_position++;
		}
		int rsposition = 0, rtposition = 0, rdposition = 0, s_position = 0;
		for (int i = 20; i >= 16; i--)
		{
			if (binary[i] == 1)
				rd += 1 << rdposition;

			rdposition++;
		} // rd 값
		for (int i = 25; i >= 21; i--)
		{
			if (binary[i] == 1)
				shamt += 1 << s_position;

			s_position++;
		} // shamt 값

		if (R_func == 32)
		{
			registers[rd] = registers[rs] + registers[rt];
		} // add
		else if (R_func == 34)
		{
			registers[rd] = registers[rs] - registers[rt];
		} // sub
		else if (R_func == 36)
		{
			registers[rd] = registers[rs] & registers[rt];
		} // and
		else if (R_func == 37)
		{
			registers[rd] = registers[rs] | registers[rt];
		} // or
		else if (R_func == 39)
		{
			registers[rd] = ~(registers[rs] | registers[rt]);
		} // nor
		else if (R_func == 0)
		{
			registers[rd] = registers[rt] << shamt;
		} // sll
		else if (R_func == 2)
		{
			registers[rd] = registers[rt] >> shamt;
		} // srl
		else if (R_func == 3)
		{
			if (registers[rt] >= (1 << 31))
			{
				//printf("shamt : %d\n", shamt);
				//registers[rd] = (signed int)registers[rt] >> shamt;
				registers[rd] = registers[rt];
				for (int i = 0; i < shamt; i++)
				{
					registers[rd] = (registers[rd] >> 1) | (1 << 31);
					//printf("registerd[rd] = %08x\n", registers[rd]);
				}
			}
			else
			{
				registers[rd] = registers[rt] >> shamt;
			}
		} // sra
		else if (R_func == 42)
		{
			registers[rd] = (registers[rs] < registers[rt]) ? 1 : 0;
		} // slt
		else if (R_func == 8)
		{
			pc = registers[rs];
		} // jr

	} // R포맷일 때 종료

	else if (decimal == 8)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				if (i == 16 && binary[i] == 1)
				{
					imm += (1 << imm_position) * (-1);
				}
				else
				{
					imm += 1 << imm_position;
				}
			}
			imm_position++;
		}
		registers[rt] = registers[rs] + imm; // signextImm
	} // addi
	else if (decimal == 12)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				imm += 1 << imm_position;
			}
			imm_position++;
		}
		registers[rt] = registers[rs] & imm; // zeroextImm
	} // andi
	else if (decimal == 13)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				imm += 1 << imm_position;
			}
			imm_position++;
		}
		registers[rt] = registers[rs] | imm;
	} // ori
	else if (decimal == 35)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				imm += 1 << imm_position;
			}
			imm_position++;
		}
		registers[rt] = memory[registers[rs] + imm] << 24;
		registers[rt] += memory[registers[rs] + imm + 1] << 16;
		registers[rt] += memory[registers[rs] + imm + 2] << 8;
		registers[rt] += memory[registers[rs] + imm + 3];
	} // lw
	else if (decimal == 43)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				imm += 1 << imm_position;
			}
			imm_position++;
		}
		memory[registers[rs] + imm] = registers[rt] >> 24;
		memory[registers[rs] + imm + 1] = registers[rt] >> 16;
		memory[registers[rs] + imm + 2] = registers[rt] >> 8;
		memory[registers[rs] + imm + 3] = registers[rt];

	} // sw
	else if (decimal == 10)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				imm += 1 << imm_position;
			}
			imm_position++;
		}
		registers[rt] = (registers[rs] < imm ? 1 : 0);
	} // slti
	else if (decimal == 4)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				if (i == 16 && binary[i] == 1)
				{
					imm += (1 << imm_position) * (-1);
				}
				else
				{
					imm += 1 << imm_position;
				}
			}
			imm_position++;
		}
		//printf("pc is %d and imm is %d ha", pc, imm);
		if (registers[rs] == registers[rt])
		{
			pc = pc + (imm * 4);
		}
	} // beq
	else if (decimal == 5)
	{
		int imm_position = 0;
		for (int i = 31; i >= 16; i--)
		{
			if (binary[i] == 1)
			{
				if (i == 16 && binary[i] == 1)
				{
					imm += (1 << imm_position) * (-1);
				}
				else
				{
					imm += 1 << imm_position;
				}
			}
			imm_position++;
		}
		//printf("pc is %d imm is %d \n", pc, imm);
		if (registers[rs] != registers[rt])
		{
			pc = pc + (imm * 4);
		}

	} // bne
	else if (decimal == 2 || decimal == 3)
	{
		int j_position = 0;
		for (int i = 31; i >= 6; i--)
		{
			if (binary[i] == 1)
				address += 1 << j_position;

			j_position++;
		}
		address = (pc & 0xF0000000) | (address << 2);
		if (decimal == 2)
		{
			pc = address;
		} // j
		else if (decimal == 3)
		{
			registers[31] = pc;
			pc = address;
		} // jal
	} // j & jal


	return 0;
}


/**********************************************************************
 * load_program
 *
 * DESCRIPTION
 *   Load the instructions in the file @filename onto the memory starting at
 *   @INITIAL_PC. Each line in the program file looks like;
 *
 *	 [MIPS instruction started with 0x prefix]  // optional comments
 *
 *   For example,
 *
 *   0x8c090008
 *   0xac090020	// sw t1, zero + 32
 *   0x8c080000
 *
 *   implies three MIPS instructions to load. Each machine instruction may
 *   be followed by comments like the second instruction. However you can simply
 *   call strtoimax(linebuffer, NULL, 0) to read the machine code while
 *   ignoring the comment parts.
 *
 *	 The program DOES NOT include the 'halt' instruction. Thus, make sure the
 *	 'halt' instruction is appended to the loaded instructions to terminate
 *	 your program properly.
 *
 *	 Refer to the @main() for reading data from files. (fopen, fgets, fclose).
 *
 * RETURN
 *	 0 on successfully load the program
 *	 any other value otherwise
 */

static int load_program(char * const filename)
{
	return -EINVAL;
}


/**********************************************************************
 * run_program
 *
 * DESCRIPTION
 *   Start running the program that is loaded by @load_program function above.
 *   If you implement the @load_program() properly, the first instruction is
 *   at @INITIAL_PC. Using @pc, which is the program counter of this processor,
 *   you can emulate the MIPS processor by
 *
 *   1. Read instruction from @pc
 *   2. Increment @pc by 4
 *   3. @process_instruction(instruction)
 *   4. Repeat until @process_instruction() returns 0
 *
 * RETURN
 *   0
 */
static int run_program(void)
{
	pc = INITIAL_PC;

	return 0;
}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING FROM THIS LINE ******       */
static void __show_registers(char * const register_name)
{
	int from = 0, to = 0;
	bool include_pc = false;

	if (strmatch(register_name, "all")) {
		from = 0;
		to = 32;
		include_pc = true;
	}
	else if (strmatch(register_name, "pc")) {
		include_pc = true;
	}
	else {
		for (int i = 0; i < sizeof(register_names) / sizeof(*register_names); i++) {
			if (strmatch(register_name, register_names[i])) {
				from = i;
				to = i + 1;
			}
		}
	}

	for (int i = from; i < to; i++) {
		fprintf(stderr, "[%02d:%2s] 0x%08x    %u\n", i, register_names[i], registers[i], registers[i]);
	}
	if (include_pc) {
		fprintf(stderr, "[  pc ] 0x%08x\n", pc);
	}
}

static void __dump_memory(unsigned int addr, size_t length)
{
	for (size_t i = 0; i < length; i += 4) {
		fprintf(stderr, "0x%08lx:  %02x %02x %02x %02x    %c %c %c %c\n",
			addr + i,
			memory[addr + i], memory[addr + i + 1],
			memory[addr + i + 2], memory[addr + i + 3],
			isprint(memory[addr + i]) ? memory[addr + i] : '.',
			isprint(memory[addr + i + 1]) ? memory[addr + i + 1] : '.',
			isprint(memory[addr + i + 2]) ? memory[addr + i + 2] : '.',
			isprint(memory[addr + i + 3]) ? memory[addr + i + 3] : '.');
	}
}

static void __process_command(int argc, char *argv[])
{
	if (argc == 0) return;

	if (strmatch(argv[0], "load")) {
		if (argc == 2) {
			load_program(argv[1]);
		}
		else {
			printf("Usage: load [program filename]\n");
		}
	}
	else if (strmatch(argv[0], "run")) {
		if (argc == 1) {
			run_program();
		}
		else {
			printf("Usage: run\n");
		}
	}
	else if (strmatch(argv[0], "show")) {
		if (argc == 1) {
			__show_registers("all");
		}
		else if (argc == 2) {
			__show_registers(argv[1]);
		}
		else {
			printf("Usage: show { [register name] }\n");
		}
	}
	else if (strmatch(argv[0], "dump")) {
		if (argc == 3) {
			__dump_memory(strtoimax(argv[1], NULL, 0), strtoimax(argv[2], NULL, 0));
		}
		else {
			printf("Usage: dump [start address] [length]\n");
		}
	}
	else {
		/**
		 * You may hook up @translate() from pa1 here to allow assembly input!
		 */
#ifdef INPUT_ASSEMBLY
		unsigned int instr = translate(argc, argv);
		process_instruction(instr);
#else
		process_instruction(strtoimax(argv[0], NULL, 0));
#endif
	}
}

static int __parse_command(char *command, int *nr_tokens, char *tokens[])
{
	char *curr = command;
	int token_started = false;
	*nr_tokens = 0;

	while (*curr != '\0') {
		if (isspace(*curr)) {
			*curr = '\0';
			token_started = false;
		}
		else {
			if (!token_started) {
				tokens[*nr_tokens] = curr;
				*nr_tokens += 1;
				token_started = true;
			}
		}
		curr++;
	}

	/* Exclude comments from tokens */
	for (int i = 0; i < *nr_tokens; i++) {
		if (strmatch(tokens[i], "//") || strmatch(tokens[i], "#")) {
			*nr_tokens = i;
			tokens[i] = NULL;
		}
	}

	return 0;
}

int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND] = { '\0' };
	FILE *input = stdin;

	if (argc > 1) {
		input = fopen(argv[1], "r");
		if (!input) {
			fprintf(stderr, "No input file %s\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (input == stdin) {
		printf("*********************************************************\n");
		printf("*          >> SCE212 MIPS Simulator v0.01 <<            *\n");
		printf("*                                                       *\n");
		printf("*                                       .---.           *\n");
		printf("*                           .--------.  |___|           *\n");
		printf("*                           |.------.|  | =.|           *\n");
		printf("*                           || >>_  ||  |---|           *\n");
		printf("*                           |'------'|  |   |           *\n");
		printf("*                           ')______('~~|___|           *\n");
		printf("*                                                       *\n");
		printf("*                                   Fall 2019           *\n");
		printf("*********************************************************\n\n");
		printf(">> ");
	}

	while (fgets(command, sizeof(command), input)) {
		char *tokens[MAX_NR_TOKENS] = { NULL };
		int nr_tokens = 0;

		for (size_t i = 0; i < strlen(command); i++) {
			command[i] = tolower(command[i]);
		}

		if (__parse_command(command, &nr_tokens, tokens) < 0)
			continue;

		__process_command(nr_tokens, tokens);

		if (input == stdin) printf(">> ");
	}

	if (input != stdin) fclose(input);

	return EXIT_SUCCESS;
}