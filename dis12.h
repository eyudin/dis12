/*
 * behavior control stuff
 */
extern int	behavior;
#define FSI		(behavior & 0x0001)	/* force some 8 bit immed to char	*/
#define FLI		(behavior & 0x0002)	/* force long immed to symbol	*/
#define F9BIP	(behavior & 0x0004)	/* force pc 9 bit idx. to symbol	*/
#define F9BIR	(behavior & 0x0008)	/* force other 9 bit idx. to symbol	*/
#define F16BIP	(behavior & 0x0010)	/* force pc 16 bit idx. to symbol	*/
#define F16BIR	(behavior & 0x0020)	/* force other 16 bit idx. to symbol	*/
#define F16BIIP	(behavior & 0x0040)	/* force pc 16 bit indidx. to symbol	*/
#define F16BIIR	(behavior & 0x0080)	/* force other 16 bit indidx. to symbol	*/

/* 
 *	symbol table entry structure 
 */ 
typedef struct { 
	int		flags;		/* symbol table flags */ 
	int		address;	/* address of this symbol */ 
	char	*name;
	} SymEnt;
 
/* 
 *	defines for flag field 
 */ 
#define	USED	1		/* entry used flag */ 
#define	PRINT	2		/* label printed */ 
 
#define	SYMS	2400		/* number of symbols (max) in table */ 

/*
 * memory specifier structure
 */
typedef struct {
	int		type;
	int		from, to;
	} MemTyp;

#define RANGES	1000					/* number of ranges allowed	*/

/* 
 *	table entry for each opcode 
 */ 
typedef struct { 
	int		type;				/* opcode type			*/ 
	int		flags;				/* flags for processing	*/
	int		newlines;			/* how many newlines	*/
	char	*text;				/* text to insert		*/ 
	} OpTblEnt;

extern OpTblEnt dis12[];
extern OpTblEnt pag2[];


/* 
 *	type field defines 
 */ 
#define ILL		0				/* illegal (unused)		*/
#define INH		(1 <<  0)		/* inherent - no args	*/
#define	IMM1	(1 <<  1)		/* immediate (#nn)		*/
#define	IMM2	(1 <<  2)		/* immediate (#nn)		*/
#define EXT		(1 <<  3)		/* extended (16 bit)	*/
#define	DIR		(1 <<  4)		/* direct page (8 bit)	*/
#define	IND		(1 <<  5)		/* indexed				*/
#define REL1	(1 <<  6)		/* relative				*/
#define REL2	(1 <<  7)		/* relative				*/
#define	IMID	(1 <<  8)		/* immediate-indexed	*/
#define	EXID	(1 <<  9)		/* extended-indexed		*/
#define	IDID	(1 << 10)		/* indexed-indexed		*/
#define	IMEX	(1 << 11)		/* immediate-extended	*/
#define	EXEX	(1 << 12)		/* extended-extended	*/
#define	IDEX	(1 << 13)		/* indexed-extended		*/
#define SPL1	(1 << 14)		/* loops				*/
#define SPL2	(1 << 15)		/* tfr/exg				*/
#define PG2		(1 << 16)		/* page 2				*/
 

/*
 * flag field defines
 */
#define NADA	0				/* no special handling		*/
#define LBL1	(1 << 0)		/* control xfer needs label	*/
#define MASK	(1 << 1)		/* has a mask too			*/
#define LBL2	(1 << 2)		/* brclr/brset needs label	*/
#define CALL	(1 << 3)		/* call needs a page number	*/
#define TRAP	(1 << 4)		/* trap needs trap number	*/
#define LOAD	(1 << 5)		/* load immediate			*/

