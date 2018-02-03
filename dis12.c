/*
 *  6812 disassembler
 */

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dis12.h" 
//#include "dis12tbl.h" 

#define TEST						/* defined debug in usage()		*/


int		debug = 0;
char	*me;
int		org, pc, xfer, pass, eoflg; 
char	operbuf[64];
char	*lp, *label, *text, *operand, *hexdata; 
char	*symfile, *addrfile, *lstfile;
FILE	*outfp, *dbgfp;
u_char	*binbuf;						/* binary buffer			*/
u_char	*bbp;							/* working ptr to it		*/
int		bbfsiz;							/* and it's size			*/
int		behavior = 0x0077;
 
SymEnt	sym[SYMS];						/* the symbol table */ 
int		symcnt = 0;						/* number of symbols used */ 

MemTyp	memtyp[RANGES + 1] = {{'c', 0, 1}, {'c', 0xfffff, 0xfffff}};
 
void	process(char *);
void	disasm(void);
int		code(OpTblEnt *, int);
void	putline(int);
void	read_class(char *);
void	add_class(char, int, int);
int		find_type(int);
void	load_syms(char *);
int		insert_sym(char *, int);
void	dolabel(int);
int		findlabel(int);
void	cleansym(void);
void	doorg(int);
void	epilog(void);
char	*strsav(char *);
int		htoi(char *);
int		_errmsg(int, char *, ... );
void	usage();
 
extern u_char	*load_file(char *name);

int 	main(argc, argv, envp)
int		argc;
char	**argv;
char	**envp;
	{
	int		option;

	me = argv[0];
	outfp = stdout; 
	dbgfp = stderr; 
	while ((option = getopt(argc, argv, "a:b:dD:l:s:o:z?")) != EOF)
		switch (option)
			{
			case 'a' :					/* address file specified	*/
				addrfile = optarg;
				break;

			case 'b' :
				switch (optarg[0])
					{
					case '+' :
						behavior |= htoi(&optarg[1]);
						break;

					case '-' :
						behavior &= ~(htoi(&optarg[1]));
						break;

					default :
						behavior = htoi(optarg);
					}
				break;

			case 'd' :					/* turn on debug (level)	*/
				++debug;
				break;

			case 'D' :					/* turn on debug (level)	*/
				debug = htoi(optarg);
				break;

			case 'l' :					/* listing file				*/
				lstfile = optarg;
				break;

			case 's' :					/* symbol file specified	*/
				symfile = optarg;
				break;

			case 'o' :					/* set org					*/
				org = htoi(optarg);
				break;

			case 'z' :
				dbgfp = stdout;
				break;

			case '?' :
			default  :
				usage();
			}

if (debug) fprintf(dbgfp, "behavior = 0x%02x\n", behavior);
	if (optind == (argc - 1))
		process(argv[optind]);
	else
		usage();
	}

/*
 * perform actions named file or stdin if name is 0
 */
 
void	process(char *name)
	{
	binbuf = load_file(name);

	if (symfile)
		load_syms(symfile);

	if (addrfile)
		read_class(addrfile);

	if (lstfile)
		if (0 == (outfp = fopen(lstfile, "w")))
			exit (_errmsg(1, "open error %d on output file %s\n", errno, lstfile));

	pass = 0;
	while (pass++ < 2)
		{								/* do 2 passes				*/
		bbp = binbuf;					/* reset pointer			*/
		eoflg = 0;
		pc = org;
		doorg(pc);
		disasm();
		}

	epilog();                                  /* "END" statement */
	cleansym();
	}

int		opc_pc, gopc;

void	disasm(void)
	{
	char	hexbuf[64];
	int		useful, opcode, oper1;
	int		i, word, nl;
	char	mem_type, last_mt = 'c';

	do	{
		opc_pc = pc;
		opcode = get_byte();
		if (eoflg)
			break;

		gopc = opcode;			/* FIXME for debug in memtype */
		nl = 0;
		useful = 1;						/* assume success			*/
		label = "";
		operand = "";
		sprintf(hexdata = hexbuf, ";$%04X $%02X", opc_pc, opcode);

		last_mt = mem_type;
		mem_type = find_type(opc_pc);
		switch (mem_type)
			{
			case 'x' :
				while ('x' == find_type(pc))
					get_byte();			/* just eat them			*/
				
				opc_pc = pc;
				useful = 0;
				doorg(pc);
				break;

			case 'a' :					/* address (label)			*/
				text = "fdb";
				oper1 = get_byte();
				word = (opcode << 8) | oper1;
				if (0 == eoflg)
					{
					sprintf(&hexbuf[strlen(hexbuf)], " $%02X", oper1);
					i = addlabel(word);
					sprintf(operand = operbuf, "%s", sym[i].name);
					}
				break;

			case 'b' :					/* bytes					*/
				text = "fcb";
				sprintf(operand = operbuf, "$%02X", opcode);
				for (i = 1; ('b' == find_type(pc)); ++i)
					{
					opcode = get_byte();
					if (eoflg)
						break;

					sprintf(&hexbuf[strlen(hexbuf)], " $%02X", opcode);
					sprintf(&operbuf[strlen(operbuf)], ",$%02X", opcode);
					if (i > 2)
						break;
					}
				break;

			case 'c' :					/* code						*/
				if ((2 == pass) && (mem_type != last_mt))
					fprintf(outfp, "\n\n");

				nl = code(&dis12[0], opcode);
				break;

			case 't' :					/* text						*/
				{
				int		state = 0;

				text = "fcc";
				i = 0;
				do	{
					if (i < 6)
						sprintf(&hexbuf[strlen(hexbuf)], " $%02X", opcode);

					if (isprint(opcode) && ('\'' != opcode))
						{	/* printable - do ascii */
						switch (state)
							{
							case 0 :	/* first time	*/
								sprintf(operand = operbuf, "'%c", opcode);
								state = 1;	/* now doing ascii		*/
								break;

							case 1 :	/* was doing ascii			*/
								sprintf(&operbuf[strlen(operbuf)], "%c", opcode);
								break;

							case 2 :	/* was doing hex			*/
								sprintf(&operbuf[strlen(operbuf)], ",'%c", opcode);
								state = 1;	/* now doing ascii		*/
								break;
							}
						}
					else
						{	/* non-printable - do hex */
						switch (state)
							{
							case 0 :	/* first time				*/
								sprintf(operand = operbuf, "$%02X", opcode);
								state = 2;	/* now doing hex		*/
								break;

							case 1 :	/* was doing ascii			*/
								sprintf(&operbuf[strlen(operbuf)], "',$%02X", opcode);
								state = 2;	/* now doing hex		*/
								break;

							case 2 :	/* was doing hex			*/
								sprintf(&operbuf[strlen(operbuf)], ",$%02X", opcode);
								break;
							}
						}

					if (++i >= 32)
						break;

					} while (('t' == find_type(pc)) && (opcode = get_byte()));

				switch (state)			/* ascii needs cleanup		*/
					{
					case 1 :
						strcat(operbuf, "'");	/* got to have this	*/
						/* fall through to maybe add null			*/

					case 2 :
						if (opcode == 0)
							strcat(operbuf, ",0");	/* need this too	*/
						break;
					}
				}
				break;

			case 'w' :					/* words					*/
				text = "fdb";
				oper1 = get_byte();
				word = (opcode << 8) | oper1;
				if (eoflg)
					break;

				sprintf(&hexbuf[strlen(hexbuf)], " $%02X", oper1);
				sprintf(operand = operbuf, "$%04X", word);
				for (i = 1; ('w' == find_type(pc)); ++i)
					{
					opcode = get_byte();
					oper1 = get_byte();
					word = (opcode << 8) | oper1;
					if (eoflg)
						break;

					sprintf(&hexbuf[strlen(hexbuf)], " $%02X $%02X", opcode, oper1);
					sprintf(&operbuf[strlen(operbuf)], ",$%04X", word);
					if (i > 2)
						break;
					}
				break;
			}

		if ((pass == 2) && useful)
			{
			if (findlabel(opc_pc) >= 0)
				dolabel(opc_pc);

			putline(nl);
			}
		else
			if (debug & 1) putline(nl);

		} while (eoflg == 0);
	}



void	putline(int nl)
	{
	fprintf(outfp, "%-11s %-7s %-31s %s\n", label, text, operand, hexdata);
	while (nl--)
		fprintf(outfp, "\n");

	if (debug & 1) fflush(outfp);
	}

/*
 * memmory classes are a=addr, b=byte, c=code, t=text, w=word
 */

void	read_class(char *name)
	{
	char	buf[256];
	char	*p = buf;
	int		from, to, i;
	char	type;
	FILE	*fp;

	if ((fp = fopen(name, "r")) == 0)
		exit (_errmsg(1, "open error %d on %s\n", errno, name));


	while (fgets(buf, 255, fp))
		{
		if (0 == strchr("#;*", buf[0]))
			{
			if (3 != sscanf(buf, "%c %x %x", &type, &from, &to))
				exit (_errmsg(1, "read_class: bad format - %s\n", buf));

			if (from > to)
				exit (_errmsg(1, "read_class: from > to - %s\n", buf));

			add_class(buf[0], from, to);
			}
		}

	fclose(fp);
	if (debug & 2) for (i = 0; memtyp[i].type; ++i)
			fprintf(dbgfp, "%2d: %c 0x%04x - 0x%04x\n",
							i, memtyp[i].type, memtyp[i].from, memtyp[i].to);
	}


void	add_class(char type, int from, int to)
	{
	int		i, j;

	for (i = 0; i < RANGES; ++i)
		if (from < memtyp[i].from)
			break;

	if (i == RANGES)
		exit (_errmsg(1, "memory classification table overflow\n"));

	if ((to >= memtyp[i].from) || (from <= memtyp[i - 1].to))
		exit (_errmsg(1, "memory classification overlap %c %4x %4x\n", type, to, from));

	for (j = RANGES; j >= i; --j)
		memtyp[j + 1] = memtyp[j];

	memtyp[i].type = type;
	memtyp[i].from = from;
	memtyp[i].to = to;
	}



int		find_type(int addr)
	{
	int		i, type;

	for (i = 0; i <= RANGES; ++i)
		if (addr >= memtyp[i].from && addr <= memtyp[i].to)
			break;

	if (debug & 4)
		if (i > RANGES)
			fprintf(dbgfp, "find_type: 0x%02x not at 0x%04x, using 'c'\n", gopc, addr);
		else
			fprintf(dbgfp, "find_type: 0x%02x at 0x%04x => %c 0x%04x - 0x%04x\n",
							gopc, i, memtyp[i].type, memtyp[i].from, memtyp[i].to);


	if (i > RANGES)
		type = 'c';						/* not found => 'c'			*/
	else
		type = memtyp[i].type;

	return (type);
	}


void	load_syms(char *name)
	{
	char	buf[256];
	char	*p = buf;
	int		addr;
	FILE	*fp;

	if ((fp = fopen(name, "r")) == 0)
		exit (_errmsg(1, "open error %d on %s\n", errno, name));

	while (fgets(p = buf, 255, fp))
		{
		if (0 == strchr("#;*", *p))
			{
			while (!isspace(*p))
				++p;

			*p++ = 0;					/* truncate name			*/
			addr = htoi(p);
			if (-1 == findlabel(addr))
				insert_sym(buf, addr);
			}
		}

	fclose(fp);
	}


int		addlabel(int address)
	{
	int		i;

	if (debug & 8) fprintf(dbgfp, "addlabel: $%04X at pc=$%04X\n", address, opc_pc);

	if (-1 == (i = findlabel(address)))
		{
		char	buf[32];

		sprintf(buf, "L%04X", address);
		i = insert_sym(buf, address);
		}

	return (i);
	}


int		insert_sym(char *s, int addr)
	{
	int		i;

	if ((i = symcnt++) == SYMS)
		{
		printf("Symbol table overflow!\n");
		exit(1);
		}

	sym[i].name = strsav(s);
	sym[i].flags |= USED;
	sym[i].address = addr;
	return (i);
	}


void	dolabel(int address)
	{
	int		i;

	i = findlabel(address);

	label = sym[i].name;
	sym[i].flags |= PRINT;
	}


int		findlabel(int address)
	{
	int		i;

	for (i = 0; i < symcnt; i++)
		if (address == sym[i].address)
			return (i);

	return (-1);
	}


void	cleansym()
	{
	int		i, k;

	for (i = 0, k = 0; i < symcnt; i++)
		if ((sym[i].flags & PRINT) == 0)
			k++;

	if (k != 0)
		{
		fprintf(outfp, "\n");
		for (i = 0; i < symcnt; i++)
			if ((sym[i].flags & PRINT) == 0)
				fprintf(outfp, "%-11s equ     $%04X\n", sym[i].name, sym[i].address);
		}
	}


void	doorg(int address)
	{
	if (2 == pass)
		fprintf(outfp, "\n\n            org     $%04X\n\n", address);
	}


void	epilog()
	{
	if (xfer)
		fprintf(outfp, "            end     $%04x\n", xfer);
	else
		fprintf(outfp, "            end\n");
	}



/*
 * strsav - allocate space for a string and copy it there
 * die if error
 */
char	*strsav(char *s)
	{
	char	*p;

	if (0 == (p = (char *)malloc(1 + strlen(s))))
		exit (_errmsg(1, "out of memory\n"));

	return (strcpy(p, s));
	}


/* 
 * read the next byte from the input file 
 */ 

u_char	get_byte(void) 
	{
	u_char	byte = 0;

	if (bbp >= (bbfsiz + binbuf))
		eoflg = 1;
	else
		{
		pc++;
		byte = *bbp++;
		}

	return (byte);
	}

/*
 * Get a hex digit from 's' into 'x'.
 */

int				htoi(char *s)
	{
	char	ch;
	int		x = 0;

	while (*s == ' ' || *s == '\t')
		++s;

	while (isxdigit(ch = *s++))
		x = (x << 4) + toupper(ch) - (ch > '9' ? '7' : '0');

	return (x);
	}


/*
 * generic error message handler
 */

int		_errmsg(int code, char *string, ...)
	{
	va_list	ap;

	va_start(ap, string);
	vfprintf(stderr, string, ap);
	va_end(ap);

	return (code);
	}


/*
 * provide usage info for this command
 */

static char	*hlpmsg[] = {
			"Usage: %s [options] file\n",
			"   -a file     address classification file\n",
			"   -b [+-]hhhh set the behavior of the disassembler\n",
			"               'hhhh' is a bit mask composed of:\n",
			"    0x0001     force 8 bit immed in 32..126 to char\n",
			"    0x0002     force long immediate to symbol\n",
			"    0x0004     force 9 bit pc indexed to symbol\n",
			"    0x0008     force other 9 bit indexed to symbol\n",
			"    0x0010     force 16 bit pc indexed to symbol\n",
			"    0x0020     force other 16 bit indexed to symbol\n",
			"    0x0040     force 16 bit pc indirect indexed to symbol\n",
			"    0x0080     force other 16 bit indirect indexed to symbol\n",
			"               a '-' prefix removes a bit and\n",
			"               a '+' prefix adds a bit and\n",
			"               no prefix sets the mask to the value\n",
			"               the default is 0x0077\n",
			"   -d          turn debug on\n",
#ifdef TEST
			"   -D hhhh     set debug to 'hhhh', where\n",
			"         1     show pass 1 disassembly\n",
			"         2     dump memtype struct\n",
			"         4     show find_type()\n",
			"         8     show addlabel()\n",
			"        10     show get_byte()\n",
			"        20     show load_file()\n",
			"        40     \n",
#else
			"   -D hhhh     set debug to 'hhhh'\n",
#endif
			"   -l file     write listing to 'file'\n",
			"   -o hhhh     set org to 'hhhh'\n",
			"   -s file     symbol mapping file\n",
			0};

void	usage()
	{
	register char	**p;

	fprintf(stderr, *(p = hlpmsg), me);
	for (++p; *p; ++p)
		fputs(*p, stderr);

	exit (1);
	}
