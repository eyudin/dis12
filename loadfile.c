/*
 * allocate storage and load a binary or S record file into it.
 *  return a pointer to the buffer
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dis12.h"

#define STATIC	static
u_int	first = 0xfffff, last = 0;
char	*lp;

STATIC int	get6h(void);
STATIC int	get4h(void);
STATIC int	get2h(void);
STATIC int	get1h(void);

extern int	debug, org, bbfsiz, binmode, xfer;
extern FILE	*dbgfp;

u_char	*binfile(char *);
u_char	*srecfile(char *);

u_char	*load_file(char *name)
	{
	char	buf[4];
	int		fd;

	if (-1 == (fd = open(name, O_RDONLY)))
		exit (_errmsg(1, "open error %d on %s\n", errno, name));

	read(fd, buf, 4);
	close (fd);
	if (('S' == buf[0]) && isxdigit(buf[1]) && isxdigit(buf[2]) && isxdigit(buf[3]))
		return (srecfile(name));
	else
		return (binfile(name));
	}


u_char	*binfile(char *name)
	{
	struct stat	sb;
	int			fd;
	u_char		*bp;					/* pointer to binary		*/

	if (-1 == stat(name, &sb))			/* find out how big it is	*/
		exit (_errmsg(1, "stat error %d on %s\n", errno, name));

	if (0 == (bp = (u_char *)malloc(sb.st_size)))
		exit (_errmsg(1, "binfile: no memory for code buffer\n"));

	if (-1 == (fd = open(name, O_RDONLY)))
		exit (_errmsg(1, "open error %d on %s\n", errno, name));

	read(fd, bp, sb.st_size);
	bbfsiz = sb.st_size;
	close (fd);
	return (bp);
	}


u_char	*srecfile(char *name)
	{
	char	buf[256];
	u_char	*bbp;						/* bin buf ptr				*/
	u_char	*bp;						/* pointer to binary		*/
	int		lineno, sraddr, srlen, curloc;
	FILE	*fp;

	/* first figure out address range */
	if (0 == (fp = fopen(name, "r")))
		exit (_errmsg(1, "open error %d on %s\n", errno, name));

	while (fgets(buf, 255, fp))
		{
		if (debug & 32) fprintf(dbgfp, "load_file: %s", buf);

		if (0 == strncmp(buf, "S1", 2))
			{
			lp = buf + 2;
			srlen = get2h() - 3;	/* less address and cs		*/
			sraddr = get4h();
			if (sraddr < first)
				first = sraddr;

			sraddr = sraddr + srlen - 1;
			if (sraddr > last)
				last = sraddr;
			}
		else if (0 == strncmp(buf, "S2", 2))
			{
			lp = buf + 2;
			srlen = get2h() - 4;
			sraddr = get6h();

			if (sraddr < first)
				first = sraddr;

			sraddr = sraddr + srlen - 1;
			if (sraddr > last)
				last = sraddr;
			}
		}

	org = first;
	bbfsiz = last - first + 1;
	if (debug & 32) fprintf(dbgfp, "load_file: first = $%04x  last = $%04x  bbfsiz = %d\n",
							first, last, bbfsiz);

	if (0 == (bp = (u_char *)malloc(bbfsiz)))
		exit (_errmsg(1, "srecfile: no memory for code buffer\n"));

	memset(bp, 0xa7, bbfsiz);			/* iniz to nop				*/
	rewind(fp);
	curloc = first;
	lineno = 0;
	bbp = bp;
	while (fgets(buf, 255, fp))
		{
		++lineno;
		if (debug & 32) fprintf(dbgfp, "load_file: %s", buf);

		if (0 == strncmp(buf, "S1", 2))
			{
			lp = buf + 2;
			srlen = get2h() - 3;
			sraddr = get4h();

			if (sraddr < curloc)
				exit (_errmsg(1, " out of range at %d (%04x)\n", lineno, sraddr));

			if (sraddr != curloc)
				{
				add_class('x', curloc, sraddr - 1);
				if (debug & 32) fprintf(dbgfp, "load_file: added x,%x,%x\n", curloc, sraddr - 1);

				curloc = sraddr;
				bbp = bp - first + curloc;
				}

			while (srlen--)
				{
				*bbp++ = get2h();
				++curloc;
				}
			}
		else
			if (0 == strncmp(buf, "S2", 2))
				{
				lp = buf + 2;
				srlen = get2h() - 4;
				sraddr = get6h();

				if (sraddr < curloc)
					exit (_errmsg(1, " out of range at %d (%04x)\n", lineno, sraddr));

				if (sraddr != curloc)
					{
					add_class('x', curloc, sraddr - 1);
					if (debug & 32) fprintf(dbgfp, "load_file: added x,%x,%x\n", curloc, sraddr - 1);

					curloc = sraddr;
					bbp = bp - first + curloc;
					}

				while (srlen--)
					{
					*bbp++ = get2h();
					++curloc;
					}
				}
		else
			if (0 == strncmp(buf, "S9", 2))
				{
				lp = buf + 2;
				srlen = get2h() - 3;
				sraddr = get4h();
				if ((sraddr < 65535) && (sraddr > 0))
					xfer = sraddr;
				}
		}

	fclose(fp);
	return (bp);
	}

STATIC
int		get6h(void)
	{
		int 	i, j, k;

		i = get2h();
		j = get2h();
		k = get2h();

		return (i << 16) + (j << 8) + k;

	}


STATIC
int		get4h(void)
	{
	int		i;

	i = get2h();
	i = (i << 8) + get2h();
	return (i);
	}


STATIC
int		get2h(void)
	{
	int		i;
	
	i = get1h();
	i = (i << 4) | get1h();
	return (i);
	}			


STATIC
int		get1h(void)
	{
	int		c;
	
	c = *lp++;
	return (toupper(c) - ((c > '9') ? '7' : '0'));
	}			
