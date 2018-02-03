#include <stdio.h>
#include <sys/types.h>
#include "dis12.h"
#include "dis12tbl.h"

extern int		org, pc, eoflg;
extern char		*text, *operand, *hexdata;
extern char		operbuf[];
extern SymEnt	sym[];					/* the symbol table */ 
extern u_int	last;

#define STATIC	static
STATIC char	*immed1(char *, int);
STATIC char	*immed2(char *, int);
STATIC char	*extended(char *, int);
STATIC char	*indexed(char *, int);

/*
 *  register text
 */
char	*regs[] = {
/* 0 */	"A",
/* 1 */	"B",
/* 2 */	"CCR",
/* 3 */	"?",
/* 4 */	"D",
/* 5 */	"X",
/* 6 */	"Y",
/* 7 */	"SP",
/* 8 */	"PC"};


/*
 * called to decode an opcode, and recursively to handle page 2 ops
 */
int		needpage;						/* global used by code callees	*/

int		code(OpTblEnt *tb, int opcode)
	{
	char	opbuf[32];
	int		addr, i;
	u_char	oper1, oper2, oper3;

	needpage = 0;
	tb += opcode;						/* point to table entry		*/
	text = tb->text;					/* get opcode text name		*/

	switch (tb->type)
		{
		case ILL :						/* illegal (unused)			*/
			sprintf(operand = operbuf, "$%02X", opcode);
			break;

		case INH :						/* inherent					*/
			if (tb->flags & TRAP)
				sprintf(operand = operbuf, "$%02X", opcode);
			break;

		case IMM1 :						/* immediate 8 bit			*/
			strcpy(operand = operbuf, immed1(opbuf, tb->flags));
			break;

		case IMM2 :						/* immediate 16 bit			*/
			strcpy(operand = operbuf, immed2(opbuf, tb->flags));
			break;

		case EXT :						/* extended					*/
			strcpy(operand = operbuf, extended(opbuf, tb->flags));
			break;

		case IND :						/* indexed					*/
			strcpy(operand = operbuf, indexed(opbuf, tb->flags));
			break;

		case DIR :						/* direct page				*/
			oper1 = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
			/* CRK all direct is memory access and therefore needs label */
			i = addlabel(oper1);
			sprintf(operand = operbuf, "%s", sym[i].name);
			break;

		case IMID :
			{
			char	arg1[32], arg2[32];

			indexed(arg2, tb->flags);
			if (opcode & 8)
				immed1(arg1, tb->flags);
			else
				immed2(arg1, tb->flags);

			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case EXID :
			{
			char	arg1[32], arg2[32];

			indexed(arg2, tb->flags);
			extended(arg1, tb->flags);
			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case IDID :
			{
			char	arg1[32], arg2[32];

			indexed(arg1, tb->flags);
			indexed(arg2, tb->flags);
			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case IMEX :
			{
			char	arg1[32], arg2[32];

			if (opcode & 8)
				immed1(arg1, tb->flags);
			else
				immed2(arg1, tb->flags);

			extended(arg2, tb->flags);
			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case EXEX :
			{
			char	arg1[32], arg2[32];

			extended(arg1, tb->flags);
			extended(arg2, tb->flags);
			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case IDEX :
			{
			char	arg1[32], arg2[32];

			indexed(arg1, tb->flags);
			extended(arg2, tb->flags);
			sprintf(operand = operbuf, "%s,%s", arg1, arg2);
			}
			break;

		case SPL1 :						/* loop opcodes				*/
			{
			char	*loops[] = {
						"dbeq", "dbne",
						"tbeq", "tbne",
						"ibeq", "ibne",
						"????", "????"};

			oper1 = get_byte();
			oper2 = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper1, oper2);
			text = loops[(oper1 >> 5) & 7];
			addr = oper2;
			if (oper1 & 0x10)
				addr = -((0 - addr) & 0xff);

			addr = pc + addr;
			i = addlabel(addr);
			sprintf(operand = operbuf, "%s,%s", regs[oper1 & 7], sym[i].name);
			}
			break;

		case SPL2 :						/* tfr/exg					*/
			oper1 = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
			if (oper1 & 0x80)
				text = "exg";
			else
				text = "tfr";

			sprintf(operand = operbuf, "%s,%s", regs[(oper1 >> 4) & 7], regs[oper1 & 7]);
			break;

		case REL1 :						/* relative 8 bit			*/
			oper1 = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
			addr = pc + (char) oper1;
			i = addlabel(addr);
			sprintf(operand = operbuf, "%s", sym[i].name);
			break;

		case REL2 :						/* relative 16 bit			*/
			oper1 = get_byte();
			oper2 = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper1, oper2);
			addr = ((char) oper1) << 8;
			addr = pc + (addr | oper2);
			i = addlabel(addr);
			sprintf(operand = operbuf, "%s", sym[i].name);
			break;

		case PG2 :						/* pre-byte 10				*/
			opcode = get_byte();
			sprintf(&hexdata[strlen(hexdata)], " $%02X", opcode);
			if (!eoflg)
				return (code(&pag2[0], opcode));
			break;
		}

	/* add mask for bset, bclr, brset, brclr */
	if (tb->flags & MASK)
		{
		oper1 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
		sprintf(&operbuf[strlen(operbuf)], ",$%02X", oper1);
		}

	/* add label for brset, brclr */
	if (tb->flags & LBL2)
		{
		oper1 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
		addr = pc + (char) oper1;
		i = addlabel(addr);
		sprintf(&operbuf[strlen(operbuf)], ",%s", sym[i].name);
		}

	if (needpage)
		{
		oper1 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
		sprintf(&operbuf[strlen(operbuf)], ",%d", oper1);
		}
	return (tb->newlines);
	}

STATIC
char	*immed1(char *buf, int flags)
	{
	u_char oper1 = get_byte();

	sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);
	if (FSI && ((0x20 <= oper1) && (oper1 <= 0x7e)))
		sprintf(buf, "#'%c'", oper1);
	else
		sprintf(buf, "#$%02X", oper1);

	return (buf);
	}


STATIC
char	*immed2(char *buf, int flags)
	{
	u_char	oper1 = get_byte();
	u_char	oper2 = get_byte();
	int		addr;

	addr = ((oper1 << 8) | oper2);
	sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper1, oper2);
	if (FLI && (LOAD & flags) && ((addr > org) && (addr < last)))
		sprintf(buf, "#%s", sym[addlabel(addr)].name);
	else
		sprintf(buf, "#$%04X", addr);

	return (buf);
	}


STATIC
char	*extended(char *buf, int flags)
	{
	u_char	oper1 = get_byte();
	u_char	oper2 = get_byte();
	int		addr;

	sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper1, oper2);
	/* CRK all extended is memory access and therefore needs label */
	addr = ((oper1 << 8) | oper2);
	sprintf(buf, "%s", sym[addlabel(addr)].name);
	if (flags & CALL)
		++needpage;

	return (buf);
	}


STATIC
char	*indexed(char *buf, int flags)
	{
	int		addr;
	u_char	oper1 = get_byte();
	u_char	oper2, oper3;

	sprintf(&hexdata[strlen(hexdata)], " $%02X", oper1);

	/* half: 0/1, 4/5, 8/9, c/d	*/
	if (0 == (oper1 & 0x20))	/* rr0nnnnn	5 bit			*/
		{
		addr = oper1 & 0x1f;
		if (0x10 & addr)		/* have to sign extend		*/
			addr = -((0 - addr) & 0x1f);

		if (0xc0 != (oper1 & 0xc0))
			sprintf(buf, "%d,%s", addr, regs[(oper1 >> 6) + 5]);
		else   /* needs label */
			sprintf(buf, "%d,%s", addr, regs[(oper1 >> 6) + 5]);

		if (flags & CALL)
			++needpage;
		}
	/* 3/8s: 2/3, 6/7, a/b	*/
	else if (0xe0 != (oper1 & 0xe0))/* rr1pnnnn pre/post	*/
		{
		u_char	n = (oper1 & 7);

		switch (0x18 & oper1)
			{
			case 0x00:
				sprintf(buf, "%d,+%s", 1 + n, regs[(oper1 >> 6) + 5]);
				break;

			case 0x08:
				sprintf(buf, "%d,-%s", 8 - n, regs[(oper1 >> 6) + 5]);
				break;

			case 0x10:
				sprintf(buf, "%d,%s+", 1 + n, regs[(oper1 >> 6) + 5]);
				break;

			case 0x18:
				sprintf(buf, "%d,%s-", 8 - n, regs[(oper1 >> 6) + 5]);
				break;
			}

		if (flags & CALL)
			++needpage;
		}
	/* all the rest are e/f	*/
	/* [ef][0189] */
	else if (0 == (oper1 & 0x06))	/* 111rr0zs 9 bit		*/
		{
		int		ispc = (0xf8 == (0xf8 & oper1));

		addr = oper2 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X", oper2);
		if (oper1 & 1)
			addr = -((0 - addr) & 0xff);

		//if ((0 == F9BI) || (0xf8 != (0xf8 & oper1)))
		if ((!ispc && !F9BIR) || (ispc && !F9BIP))	/* do neither */
			sprintf(buf, "%d,%s", addr, regs[((oper1 >> 3) & 3) + 5]);
		else
			if (!ispc)
				{
				int		i = addlabel(0xffff & addr);

				sprintf(buf, "%s,%s", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}
			else
				{
				int		i;

				addr = ((addr + pc) & 0xffff);
				i = addlabel(addr);
				sprintf(buf, "%s-*-3,%s", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}

		if (flags & CALL)
			++needpage;
		}
	/* [ef][2a] */
	else if (2 == (oper1 & 0x07))	/* 111rr010 16 bit		*/
		{
		short	addr;
		int		ispc = (0xfa == oper1);

		oper2 = get_byte();
		oper3 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper2, oper3);
		addr = (oper2 << 8) | oper3;
		if ((!ispc && !F16BIR) || (ispc && !F16BIP))	/* do neither */
			sprintf(buf, "$%04X,%s", 0xffff & addr, regs[((oper1 >> 3) & 3) + 5]);
		else
			if (!ispc)
				{
				int		i = addlabel(0xffff & addr);

				sprintf(buf, "%s,%s", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}
			else
				{
				int		i;

				addr = ((addr + pc) & 0xffff);
				i = addlabel(addr);
				sprintf(buf, "%s-*-4,%s", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}

		if (flags & CALL)
			++needpage;
		}
	/* [ef][3b] */
	else if (3 == (oper1 & 0x07))	/* 111rr011 16 bit indir	*/
		{
		int		ispc = (0xfb == oper1);
		short	addr;
		
		oper2 = get_byte();
		oper3 = get_byte();
		sprintf(&hexdata[strlen(hexdata)], " $%02X $%02X", oper2, oper3);
		addr = (oper2 << 8) | oper3;
		if ((!ispc && !F16BIIR) || (ispc && !F16BIIP))	/* do neither */
			sprintf(buf, "[%d,%s]", addr, regs[((oper1 >> 3) & 3) + 5]);
		else
			if (!ispc)
				{
				int		i = addlabel(0xffff & addr);

				sprintf(buf, "[%s,%s]", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}
			else
				{
				int		i;

				addr = ((addr + pc) & 0xffff);
				i = addlabel(0xffff & addr);
				sprintf(buf, "[%s-*-4,%s]", sym[i].name, regs[((oper1 >> 3) & 3) + 5]);
				}
		}
	/* [ef][456cde] */
	else if (7 != (oper1 & 0x07))	/* 111rr1aa accum		*/
		{
		sprintf(buf, "%c,%s", "ABD"[oper1 & 3], regs[((oper1 >> 3) & 3) + 5]);
		if (flags & CALL)
			++needpage;
		}
	/* [ef][7f] */
	else							/* 111rr111 D indir		*/
		{
		sprintf(buf, "[D,%s]", regs[((oper1 >> 3) & 3) + 5]);
		}

	return (buf);
	}
