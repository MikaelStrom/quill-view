/*
	Copyright (c) 2008-2015 Mikael Strom

	This file is part of quill-view.

	quill-view-view is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	--------------------------------------------------------------------------------

	Quill-View - Translates Quill Documents to Text or Html.

	Note: Must be compiled with default char UNSIGNED,
		  or it will not work properly!
				Under MSC, use /J
				Under gcc, use -funsigned-char
				Under c68, use -uchar

	For proper display of this file, set tab-width to 4.

	Overview
	--------

	The Quill format and the way it is used in Quill is very
	clever, but also a pain to decode correctly.
	There are numerous 'fixes' throught the code to decode
	the file properly. This makes the code hard to read.
	In particular, the code for Right formated paras are
	hard to grasp. I just can't figure better way to do it.

	All versions translate to UTF-8 Text, except the QDOS
	version that keeps the native characters.

	Changes:
	--------
	- 0.6 Beta
		* Win32: Create file in %TEMP% rather than users home directory
        
    - 0.7 Mac OS X support added by Simon N Goodwin. Currently
          this is for Xcode 6, hence x86 architecture only ;-(
          Email simon@studio.woden.com if you want a PPC version

	Todo's:
	------

	- Still missing is Decimal and Right tabs, which currently
		is interpreted as Left tabs. Hard work!
	- Page numbering is always decimal, missing roman and
		character numbering (any one uses this?).
	- Page numbering always starts from 1 (should be easy to fix)
	- Would be nice with some sort of page delimiter. Not sure
		what to use though - can look messy...
	- Would be nice with a text only translation option.
		Not sure what to replace the non-printable characters
		with though.
	- Soft-hypen not yet done. Should be resonably easy.
	- Sngle line space only implemented.
*/

#ifdef _WIN32
#pragma warning(disable : 4996)
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/*------------------------------------------------------------------------------- */

#define ME				"Quill-View 0.7 Beta"

#define true			1
#define false			0

#define	MAX_ARGV		5
#define	END_TEXT		0x0e	/* End of text (EOF) */
#define	END_PARA		0x00	/* End of Paragraph (resets highlighting attributes) */
#define	SPACE			0x20	/* Space */
#define	TAB				0x09	/* Tab */
#define	FORM_FEED		0x0c	/* Form feed */
#define	BOLD			0x0f	/* Bold toggle */
#define	UNDELINE		0x10	/* Underline toggle */
#define	SUB_SCRIPT		0x11	/* Subscript toggle */
#define	SUPER_SCRIPT	0x12	/* Superscript toggle */
#define	SOFT_HYPEN		0x1e	/* Soft Hyphen */

#define JUST_LEFT		0
#define JUST_CENTRE		1
#define JUST_RIGHT		2

#define isPrintable(c)	(c >= 0x20 && c < 0xC0)

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#ifndef min
#define min(a,b)  (a < b ? a : b)
#endif

#ifndef max
#define max(a,b)  (a > b ? a : b)
#endif

#define HTML_HEAD "<html><head><title>Quill Document</title><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /></head>\n "\
	"<body><style type=\"text/css\">\n " \
	"p {font-family:monospace; padding-top:0px; padding-bottom:0px; margin-top:0px; margin-bottom:0px;}\n "	\
	"</style>\n"

#define HTML_TAIL "</body></html>"

/*------------------------------------------------------------------------------- */

#pragma pack(1)

typedef int bool;
typedef unsigned char byte;
typedef unsigned short ushort;

typedef enum { Html, Text } Format;

typedef struct {					/* File Header */
	ushort		len;				/* Header length, should be 20 */
	char		id[8];				/* Should be "vrm1qdf0" for Quill docs */
	unsigned	textLen;			/* Length of text area (including file header).   */
									/* Acts as a pointer to the start of the paragraph table. */
	ushort		paraLen;			/* Length of the Paragraph Table (including header).   */
									/* Acts as a pointer to the Free Space table. */
	ushort		freeLen;			/* Length of the Free Space Table (including header).	*/
									/* Acts as a pointer to the Layout Table. */
	ushort		layoutLen;			/* Length of the Layout table (junk after might be present).  */
} Header;
const int HeaderSize = sizeof(Header);

typedef struct {					/* PARAGRAPH TABLE */
	ushort		size;				/* Element size */
	ushort		gran;				/* Granularity */
	ushort		used;				/* Elements used */
	ushort		alloc;				/* Elements allocated */
} ParaTableHead;
const int ParaTableHeadSize = sizeof(ParaTableHead);

typedef struct {
	unsigned	offset;				/* Offset from start of file to the text making up this paragraph */
	ushort		paraLen;			/* The length of the paragraph. This length includes the NULL byte	*/
	byte		dummy;				/*	  ...that terminates the paragraph text. */
	byte		leftMarg;			/* Left Margin.  Starts at 0 */
	byte		indentMarg;			/* Indent Margin */
	byte		rightMarg;			/* Right Margin */
	byte		justif;				/* Justification: 0 = Left, 1 = Centre, 2 = Right */
	byte		tabTable;			/* Tab Table entry that applies to this paragraph */
	short		dummy2;
} ParaTable;
const int ParaTableSize = sizeof(ParaTable);

typedef struct {
	byte		bottomMarg;			/* Bottom margin */
	byte		dispMode;			/* Display Mode */
	byte		lineGap;			/* Line Gap */
	byte		pageLen;			/* Page length */
	byte		startPage;			/* Start page */
	byte		color;				/* Colour of type 0 = Green, 1 = White */
	byte		topMargin;			/* Top (Upper) Margint */
	byte		dummy1;
	ushort		wordCount;			/* Word count */
	ushort		maxTabSize;			/* Max size of tab area */
	ushort		tabSize;			/* Size of tab area used */
	byte		headerF;			/* Header flag: 0 = None, 1 = Left, 2 = Centre, 3 = Right */
	byte		footerF;			/* Footer flag: -"- */
	byte		headerMarg;			/* Header Margin */
	byte		footerMarg;			/* Footer Margin */
	byte		headerBold;			/* Header bold flag: 0 = Normal, 1 = Bold */
	byte		footerBold;			/* Footer bold flag: -"- */
} LayoutTable;
const int LayoutTableSize = sizeof(LayoutTable);

typedef struct {
	byte	entry;					/* Tab Entry Number }	zero if end */
	byte	length;					/* Tab Entry Length }	of tab entries */
} TabHeader;
const int TabHeaderSize = sizeof(TabHeader);

typedef struct {
	byte	pos;					/* Tab position */
	byte	type;					/* Tab type: 0 = Left, 1 = Centre, 2 = Right */
} TabEntry;
const int TabEntrySize = sizeof(TabEntry);

/*------------------------------------------------------------------------------- */

Format			format;
unsigned		offset;				/* offset in file (number of bytes read so far) */
Header			header;
char			*textBuffer;
ParaTableHead	parTableHead;
ParaTable		*parTable;
LayoutTable		layoutTable;
TabHeader		*tabTable;
char			headerPara[128];
char			footerPara[128];
int				lineNo;
int				pageNo;
int				minLmarg;
int				maxRmarg;
int				maxLines;
int				paraCount;
bool			bold = false;
bool			sub = false;
bool			super = false;
bool			underline = false;
char			*renderNewLine;
char			*renderSpace;
char			*renderParaStart;
char			*renderParaEnd;

/* NOTE: THIS WILL ONLY WORK WITH DEFAULT CHAR UNSIGNED. */

unsigned int xlate_utf_8[0x100] = {
/*	0		 1		2	   3	  4		 5		6	   7		  8		 9		a	   b	  c		 d		e	   f	 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',				/* 00 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   '-',	 ' ',				/* 10 */
	' ',   '!',   '"',	 '#',	'$',   '%',   '&',	 '\'',		'(',   ')',   '*',	 '+',	',',   '-',   '.',	 '/',				/* 20 */
	'0',   '1',   '2',	 '3',	'4',   '5',   '6',	 '7',		'8',   '9',   ':',	 ';',	'<',   '=',   '>',	 '?',				/* 30 */
	'@',   'A',   'B',	 'C',	'D',   'E',   'F',	 'G',		'H',   'I',   'J',	 'K',	'L',   'M',   'N',	 'O',				/* 40 */
	'P',   'Q',   'R',	 'S',	'T',   'U',   'V',	 'W',		'X',   'Y',   'Z',	 '[',	'\\',  ']',   '^',	 '_',				/* 50 */
	0xA3,  'a',   'b',	 'c',	'd',   'e',   'f',	 'g',		'h',   'i',   'j',	 'k',	'l',   'm',   'n',	 'o',				/* 60 */
	'p',   'q',   'r',	 's',	't',   'u',   'v',	 'w',		'x',   'y',   'z',	 '{',	'|',   '}',   '~',0x0A9C2,				/* 70 */
	0xA4C3,0xA3C3,0xA5C3,0xA9C3,0xB6C3,0xB5C3,0xB8C3,0xBCC3,	0xA7C3,0xB1C3,0xBDC7,0x93C5,0xA1C3,0xA0C3,0xA2C3,0xABC3,			/* 80 */
	0xA8C3,0xAAC3,0xAFC3,0xADC3,0xACC3,0xAEC3,0xB3C3,0xB2C3,	0xB4C3,0xBAC3,0xB9C3,0xBBC3,0x9FC3,0xA2C2,0xA5C2,0x60,				/* 90 */
	0x84C3,0x83C3,0x85C3,0x89C3,0x96C3,0x95C3,0x98C3,0x9CC3,	0x87C3,0x91C3,0x86C3,0x92C5,0xB1CE,0xB4CE,0xB8CE,0xBBCE,			/* a0 */
	0xB5C2,0xA0CE,0xA6CE,0xA1C2,0xBFC2,0xAC82E2,0xA7C2,0xA4C2,	0xABC2,0xBBC2,0xBAC2,0xB7C3,0x9086E2,0x9286E2,0x9186E2,0x9386E2,	/* b0 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',				/* c0 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',				/* d0 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',				/* e0 */
	' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' ',		' ',   ' ',   ' ',	 ' ',	' ',   ' ',   ' ',	 ' '				/* f0 */
};

/*------------------------------------------------------------------------------- */
#ifndef _QDOS_
ushort BEword(ushort be)
{
	ushort le;

	le =  ((0x00ff & be) << 8);
	le |= ((0xff00 & be) >> 8);

	return le;
}

/*------------------------------------------------------------------------------- */
unsigned BElong(unsigned be)
{
	unsigned le;

	le =  ((0x000000ff & be) << 24);
	le |= ((0x0000ff00 & be) << 8);
	le |= ((0x00ff0000 & be) >> 8);
	le |= ((0xff000000 & be) >> 24);

	return le;
}
#endif	/* not _QDOS_ */

/*------------------------------------------------------------------------------- */
void io_error(char *format, char *arg)
{
	char msg[512];

	sprintf(msg, format, arg, strerror(errno));

#ifdef _WIN32
	MessageBox(NULL, msg, ME, MB_OK | MB_ICONERROR);
#else
	fprintf(stderr, msg);
#endif
	exit(1);
}

/*------------------------------------------------------------------------------- */
void error(char *msg)
{
#ifdef _WIN32
	MessageBox(NULL, msg, ME, MB_OK | MB_ICONERROR);
#else
	fprintf(stderr, msg);
#endif
	exit(1);
}

/*------------------------------------------------------------------------------- */
void usage()
{
#if defined(_WIN32)
	MessageBox(NULL, "usage:\n\nquill-view filename\nor\nquill-view [-t|-m] source-file target-file\n\nTip 1: Drag and drop a file to the program icon.\nTip 2: Hold down Alt-Key while dropping to show file in notepead.\n\n", ME, MB_OK | MB_ICONINFORMATION);
#elif defined(_QDOS_)
	fprintf(stderr, "quill-view [-t|-m] [source-file [target-file]]\n");
	fprintf(stderr, "			-t translates to text (QDOS ASCII) format (default)\n");
	fprintf(stderr, "			-m translates to HTML format\n");
#else
	fprintf(stderr, "quill-view [-t|-m] [source-file [target-file]]\n");
	fprintf(stderr, "			-t translates to UTF-8 text format (default)\n");
	fprintf(stderr, "			-m translates to HTML format\n");
#endif
	exit(1);
}

/*------------------------------------------------------------------------------- */
int getByte()
{
	if(offset >= (header.textLen - HeaderSize))
		return EOF;
	else
		return textBuffer[offset++];
}

/*------------------------------------------------------------------------------- */
ParaTable *getPara(unsigned offset)
{
	int i;

	for(i = 1; i < parTableHead.used; ++i)		/* skip first entry (set i = 1), always garbage? */
	{
		if(parTable[i].offset == offset + 20)	/* add 20, we don't have a header here */
			return &parTable[i];
	}

	return NULL;	/* not there! */
}

/*------------------------------------------------------------------------------- */
int getNextTab(int table, int column)
{
	TabHeader *pt;
	TabEntry *pe;
	int i;

	/* find the table */
	for(pt = tabTable; (pt->entry != 0) && (pt->entry != table); pt += pt->length / 2)
		;

	/* found, search for next tab given current column */
	if(pt->entry == table)
	{
		pe = (TabEntry*) pt + 1;
		for(i = 0; i < (pt->length / 2) - 1; ++i)
			if(pe[i].pos >= column)
				return pe[i].pos;
	}

	return -1;
}

/*------------------------------------------------------------------------------- */
void renderLine(char *line) // SNG, suppress sprintf warning (was byte *)
{
	unsigned c;

	++lineNo;

	if(format == Text)
	{
		while(*line)
		{
			switch(*line)
			{
			case BOLD:
			case UNDELINE:
			case SUB_SCRIPT:
			case SUPER_SCRIPT:
			case FORM_FEED:
				break;
			default:
#ifdef _QDOS_
				c = *line;
				if(c == TAB)
					c = ' ';
				putchar(c);
#else
				c = xlate_utf_8[*line];
				if(c > 0xff)
				{
					putchar(c & 0x0000ff);
					if((c & 0x0000ff) == 0xE2)
					{
						putchar((c & 0x00ff00) >> 8);
						putchar((c & 0xff0000) >> 16);
					}
					else
					{
						putchar((c & 0x00ff00) >> 8);
					}
				}
				else
				{
					putchar(c);
				}
#endif
				break;
			}
			++line;
		}
	}
	else
	{
		while(*line)
		{
			switch(*line)
			{
			case BOLD:
				printf(bold ? "</b>" : "<b>");
				bold = ! bold;
				break;
			case UNDELINE:
				printf(underline ? "</u>" : "<u>");
				underline = ! underline;
				break;
			case SUB_SCRIPT:
				printf(sub ? "</sub>" : "<sub>");
				sub = ! sub;
				break;
			case SUPER_SCRIPT:
				printf(super ? "</sup>" : "<sup>");
				super = ! super;
				break;
			case FORM_FEED:
				break;
			case SOFT_HYPEN:
				putchar('-');
				break;
			case '<':
				printf("&lt;");
				break;
			case '>':
				printf("&gt;");
				break;
			case SPACE:
			case TAB:
				printf("&nbsp;"); /* &nbsp	*/
				break;
			default:
				c = xlate_utf_8[*line];
				if(c > 0x100)
				{
					putchar(c & 0x0000ff);
					if((c & 0x0000ff) == 0xE2)
					{
						putchar((c & 0x00ff00) >> 8);
						putchar((c & 0xff0000) >> 16);
					}
					else
					{
						putchar((c & 0x00ff00) >> 8);
					}
				}
				else
					putchar(c);
				break;

			}
			++line;
		}
	}
	printf(renderNewLine);
}

/*------------------------------------------------------------------------------- */
void renderMargin(int leftPad)
{
	if(format == Text)
	{
		while(leftPad-- > 0)
			printf(renderSpace);
	}
	else
	{
		if(bold)	printf("</b>");
		if(underline)	printf("</u>");
		if(sub)		printf("</sub>");
		if(super)	printf("</sup>");

		while(leftPad-- > 0)
			printf(renderSpace);

		if(bold)	printf("<b>");
		if(underline)	printf("<u>");
		if(sub)		printf("<sub>");
		if(super)	printf("<sup>");
	}
}

/*------------------------------------------------------------------------------- */
void renderHeaderFooter(char *str, bool head)
{
	char line[128];     // SNG, suppress sprintf warning (was byte *)
	char *pLine = line; // SNG, was byte *
	bool useBold = false;
	int width, length;

	if((head && layoutTable.headerF == 0) || (!head && layoutTable.footerF == 0))
		return;

	*pLine = 0;

	width = maxRmarg - minLmarg;

	if((head && layoutTable.headerBold) || (! head && layoutTable.footerBold))
	{
		*pLine++ = BOLD;
		useBold = true;
	}

	while(*str)
	{
		if(str[0] == 'n' && str[1] == 'n' && str[2] == 'n')			/* digit page number? */
		{
			sprintf(pLine, "%d", pageNo);
			while(*pLine)
				++pLine;

			str = &str[3];																					/* advance source pointer after 'nnn' */
		}
		else if(str[0] == 'a' && str[1] == 'a' && str[2] == 'a')	/* alpha page number? */
		{
			sprintf(pLine, "%d", pageNo);
			while(*pLine)
				++pLine;

			str = &str[3];																					/* advance source pointer after 'nnn' */
		}
		else if(str[0] == 'r' && str[1] == 'r' && str[2] == 'r')	/* roman page number? */
		{
			sprintf(pLine, "%d", pageNo);
			while(*pLine)
				++pLine;

			str = &str[3];																					/* advance source pointer after 'nnn' */
		}
		else
			*pLine++ = *str++;
	}

	if(useBold)
		*pLine++ = BOLD;

	*pLine = 0;

	length = (int) strlen(line);

	if(useBold)
		length -= 2;

	switch(head ? layoutTable.headerF : layoutTable.footerF)
	{
	case 0:					/* None */
	case 1:					/* Left Justified */
		renderMargin(minLmarg);
		break;
	case 2:					/* Centre justified */
		renderMargin(minLmarg + (width / 2) - (length / 2));
		break;
	case 3:					/* Right justified */
		renderMargin(width - length);
		break;
	}

	renderLine(line);
}

/*------------------------------------------------------------------------------- */
void newPage()
{
	if(format == Html)
	{
		if(bold)	printf("</b>");
		if(underline)	printf("</u>");
		if(sub)		printf("</sub>");
		if(super)	printf("</sup>");
	}

	if(maxLines && lineNo < maxLines)
		while(lineNo++ <= maxLines)
			renderLine("");

/*	renderLine(""); */
	renderHeaderFooter(footerPara, false);
/*	renderLine(""); */
	++pageNo;
	lineNo = 2;
	renderHeaderFooter(headerPara, true);
	/*renderLine(""); */

	if(format == Html)
	{
		if(bold)	printf("<b>");
		if(underline)	printf("<u>");
		if(sub)		printf("<sub>");
		if(super)	printf("<sup>");
	}
}

/*------------------------------------------------------------------------------- */
void printLeftPara(ParaTable *parTab)
{
	char	lineBuf[512];
	char	*lineBufPtr;
	bool	indentLine;
	int	col;
	int	lastSpace;
	char	*lastSpacePtr;
	int		lastCol = 0;  // SNG, suppress spurious warning
	/*int	maxWidth; */
	int	lMarg;
	int	rMarg;
	bool	newPageFlag;

	indentLine = true;			/* first line is indent line */

	while(textBuffer[offset] != 0)		/* until end of paragraph, for each line */
	{
		if(maxLines && lineNo >= maxLines)
			newPage();

		newPageFlag = false;
		col = 0;
		lineBufPtr = lineBuf;
		lastSpace = 0;
		lastSpacePtr = NULL;

		/* calculate effective margins */

		if(paraCount < 3)			/* Header/footer margins seems to be garbage */
		{
			lMarg = 9;
			rMarg = 79;
		}
		else
		{
			lMarg = indentLine ?  parTab->indentMarg : parTab->leftMarg;
			rMarg = parTab->rightMarg;
		}

		indentLine = false;			/* only valid for first line */
		col = lMarg;
		newPageFlag = false;

		/* while within max line width, collect words and build line */
		/* remember last space so we can backout to fit last word within rMarg */

		do {
			if(textBuffer[offset] == FORM_FEED)
				newPageFlag = true;
			if(isPrintable(textBuffer[offset]))
				++col;					/* advance column  */
			if(textBuffer[offset] == SPACE)
			{
				lastSpace = offset;			/* remember last space in case we need to break line */
				lastSpacePtr = lineBufPtr;
				lastCol = col;
			}

			if(textBuffer[offset] == TAB)			/* expand tabs */
			{
				int nextTab = getNextTab(parTab->tabTable, col + 1);

				if(nextTab < 0) {
					++offset;
					break;				/* if no more tabs, finnish line here */
				}

				while(col < nextTab)
				{
					*lineBufPtr++ = SPACE;
					++col;
				}
			}
			else
				*lineBufPtr++ = textBuffer[offset];
			++offset;
		} while(textBuffer[offset] != END_PARA && col < rMarg);

		*lineBufPtr = 0;

		if(col >= rMarg && lastSpacePtr != NULL)	/* break line */
		{
			*lastSpacePtr = 0;						/* back up to last space */
			offset = lastSpace;						/* advance on after last space, so we don't loop forever */
			col = lastCol;
			while(textBuffer[offset] == SPACE)		/* skip initial spaces on line */
				++offset;
		}

		renderMargin(lMarg);
		renderLine(lineBuf);

		if(newPageFlag)
			newPage();
	}
}

/*------------------------------------------------------------------------------- */
void printRightPara(ParaTable *parTab)
{
	char	lineBuf[512];
	char	resultBuf[512];
	char	*resultPtr;
	char	*lineBufPtr;
	bool	indentLine;
	int		col;
	int		lastSpace;
	char	*lastSpacePtr;
	int	lastCol;
	int	lMarg;
	int	rMarg;
	int	i, j, pads, spaces, padSpace, lastTab;
	bool	newPageFlag;
	bool	tabWrapFlag;

	indentLine = true;								/* first line is indent line */
	while(textBuffer[offset] != 0)					/* until end of paragraph, for each line */
	{
		newPageFlag = false;
		tabWrapFlag = false;
		col = 0;
		lastCol = 0;
		lineBufPtr = lineBuf;
		lastSpace = 0;
		lastSpacePtr = NULL;

		if(maxLines && lineNo >= maxLines)
			newPage();

		/* calculate effective margins */

		lMarg = indentLine ?  parTab->indentMarg : parTab->leftMarg;
		rMarg = parTab->rightMarg;

		indentLine = false;							/* only valid for first line */
		col = lMarg;

		/* while within right margin, collect words and build line */
		/* remember last space so we can back out to fit last word within rMarg */

		do {
			/*if(textBuffer[offset] == FORM_FEED) */
			/*	newPageFlag = true; */

			if(textBuffer[offset] == SPACE || textBuffer[offset] == TAB || textBuffer[offset] == SOFT_HYPEN)
			{
				lastSpace = offset;					/* remember last space in case we need to break line */
				lastSpacePtr = lineBufPtr;
				lastCol = col;
			}

			if(textBuffer[offset] == TAB)			/* expand tabs */
			{
				int nextTab = getNextTab(parTab->tabTable, col + 1);

				if(nextTab < 0) {
					tabWrapFlag = true;				/* wraps to next line */
					++offset;						/* tab consumed */
					break;							/* no more tabs, wrap line here */
				}

				while(col < nextTab && col < rMarg)
				{
					*lineBufPtr++ = TAB;			/* we write tabs to local buffer, to distinuish from normal space later on */
					++col;
				}
			}
			else
			{
				*lineBufPtr++ = textBuffer[offset];	/* copy to local buffer */

				if(isPrintable(textBuffer[offset]))
					++col;							/* advance column  */
			}
			++offset;
		} while(textBuffer[offset] != END_PARA && col < rMarg);

		*lineBufPtr = 0;

		if(textBuffer[offset] == END_PARA || tabWrapFlag)	/* if end of para reached, or line ended with tab */
		{
			strcpy(resultBuf, lineBuf);				/* ...don't right justify */
		}
		else										/* ...else, right justify */
		{
			if(col >= rMarg && lastSpacePtr != NULL)	/* break line */
			{
				*lastSpacePtr = 0;					/* strip of trailing spaces			 */
				--lastSpacePtr;
				while(*lastSpacePtr == SPACE && lastSpacePtr > lineBuf)
				{
					*lastSpacePtr = 0;
					--lastSpacePtr;
					--lastCol;
				}

				offset = lastSpace;
				col = lastCol;
				while(textBuffer[offset] == SPACE)	/*	 || textBuffer[offset] == FORM_FEED */
					++offset;						/* skip initial spaces on line */
			}

			/* right justify line */

			pads = rMarg - col;

			/* count no of spaces in line, and figure how many padding spaces to add per space. */
			/* if we find a tab, reset counter (as we cant pad before a tab, only after) and also */
			/* remember where the last tab was found, so we can start from there */

			spaces = 0;
			lastTab = 0;
			for(i = 0; lineBuf[i] != 0; ++i)
			{
				if(lineBuf[i] == TAB)
				{
					spaces = 0;
					lastTab = i;
				}
				if(lineBuf[i] == SPACE)
					++spaces;
			}

			if(pads <= spaces)
				padSpace = 1;
			else if(spaces > 0)
				padSpace = pads / spaces;
			else
				padSpace = 1;

			if(lastTab > 0)
			{
				memcpy(resultBuf, lineBuf, lastTab);		/* copy up to last tab */
				resultPtr = &resultBuf[lastTab];			/* and set start pinter to the last tab */
			} else
				resultPtr = resultBuf;

			/* pad with spaces to make line fit exaclty between margins */

			for(i = lastTab; lineBuf[i]; ++i)
			{
				if(lineBuf[i] == SPACE)
				{
					*resultPtr++ = SPACE;
					if(--spaces == 0)						/* if last space, add all remaining pads */
						while(pads--)
							*resultPtr++ = ' ';
					else									/* else add 'padSpace' pads */
						for(j = 0; j < padSpace; ++j)
							if(pads > 0)
							{
								*resultPtr++ = ' ';
								--pads;
							}
				}
				else
					*resultPtr++ = lineBuf[i];
			}

			*resultPtr = 0;
		}

		renderMargin(lMarg);
		renderLine(resultBuf);

		for(i = 0; resultBuf[i]; ++i)
			if(resultBuf[i] == FORM_FEED)
			{
				newPage();
				break;
			}
	}
}

/*------------------------------------------------------------------------------- */
void printCenterPara(ParaTable *parTab)
{
	char lineBuf[512];
	char *lineBufPtr;
	int col;
	int lastSpace;
	char *lastSpacePtr;
	int lastCol;
	int maxWidth;
	int lMarg;
	int rMarg;
	int leftPad;
	bool newPageFlag;

	/* calculate effective margins */

	if(paraCount < 3)										/* header/footer margins seems to be garbage */
	{
		lMarg = 9;
		rMarg = 79;
		maxWidth = rMarg - lMarg;
	}
	else
	{
		lMarg = parTab->leftMarg;
		rMarg = parTab->rightMarg;
		maxWidth = rMarg - lMarg;
	}

	while(textBuffer[offset] != 0)							/* until end of paragraph, for each line */
	{
		if(maxLines && lineNo >= maxLines)
			newPage();

		newPageFlag = false;
		col = 0;
		lastCol = 0;
		lineBufPtr = lineBuf;
		lastSpace = 0;
		lastSpacePtr = NULL;

		/* while within max line width, collect words and build line */

		do {
			if(textBuffer[offset] == FORM_FEED)
				newPageFlag = true;

			if(isPrintable(textBuffer[offset]) || textBuffer[offset] == TAB)
				++col;					/* advance column  */
			if(textBuffer[offset] == SPACE)
			{
				lastSpace = offset;							/* remember last space in case we need to break line */
				lastSpacePtr = lineBufPtr;
				lastCol = col;
			}

			if(textBuffer[offset] == TAB)
				*lineBufPtr++ = SPACE;						/* convert TAB to space in centered paras */
			else
				*lineBufPtr++ = textBuffer[offset];
			++offset;
		} while(textBuffer[offset] != END_PARA && col < maxWidth);

		*lineBufPtr = 0;

		if(col >= maxWidth && lastSpacePtr != NULL)			/* break line */
		{
			*lastSpacePtr = 0;								/* back up to last space */
			offset = lastSpace;
			col = lastCol;
		}

		/* Center line */
		leftPad = lMarg + (maxWidth / 2) - (col / 2);
		renderMargin(leftPad);
		renderLine(lineBuf);

		if(newPageFlag)
			newPage();
	}
}

/*------------------------------------------------------------------------------- */
void printPara(ParaTable *parTab)
{
	++paraCount;

	printf(renderParaStart);

	if(textBuffer[offset] == 0)
	{
		if(paraCount > 2)
			renderLine("");			/* empty paragaph needs a new line */
	}
	else
	{
		switch(parTab->justif)
		{
		case JUST_LEFT:
			printLeftPara(parTab);
			break;
		case JUST_CENTRE:
			printCenterPara(parTab);
			break;
		case JUST_RIGHT:
			printRightPara(parTab);
			break;
		}
		if(format == Html)
		{
			if(bold)	printf("</b>");
			if(underline)	printf("</u>");
			if(sub)		printf("</sub>");
			if(super)	printf("</sup>");
			bold = false;
			sub = false;
			super = false;
			underline = false;
		}
	}
	printf(renderParaEnd);
}

/*------------------------------------------------------------------------------- */
void *safe_malloc(int size)
{
	void *p;
	char msg[64];

	p = malloc(size);

	if(p == NULL)
	{
		sprintf(msg, "quill-view: cant allocate %d bytes of memory\n", size);
		error(msg);
	}
	return p;
}

/*------------------------------------------------------------------------------- */
void translate(char *srcfile)
{
	ParaTable	*currPara;
	ParaTable	defaultPara = { 0, 0, 0, 9, 14, 69, 0, 0, 0 };
	size_t	bytes;
	int			i, ch, done;

	/* Read 20 bytes header and make sure it's a Quill file  */

	bytes = fread(&header, 1, HeaderSize, stdin);
#ifndef _QDOS_
	header.len = BEword(header.len);
	header.textLen = BElong(header.textLen);
	header.paraLen = BEword(header.paraLen);
	header.freeLen = BEword(header.freeLen);
	header.layoutLen = BEword(header.layoutLen);
#endif

	if(memcmp(header.id, "vrm1qdf0", sizeof(header.id)) != 0)
		error("Not a valid Quill Document\n");

	/* Read text buffer */

	textBuffer = safe_malloc(header.textLen);
	bytes = fread(textBuffer, 1, header.textLen, stdin);

	/* goto paragraph table head and read it */

	fseek(stdin, header.textLen, SEEK_SET);
	bytes = fread(&parTableHead, 1, ParaTableHeadSize, stdin);
#ifndef _QDOS_
	parTableHead.size = BEword(parTableHead.size);
	parTableHead.gran = BEword(parTableHead.gran);
	parTableHead.used = BEword(parTableHead.used);
	parTableHead.alloc = BEword(parTableHead.alloc);
#endif

	/* allocate memory and read the paragraph table (or, used parts actually), */
	/* then translate from big to little endian */

	parTable = safe_malloc(parTableHead.size * parTableHead.used);
	bytes = fread(parTable, 1, parTableHead.size * parTableHead.used, stdin);

#ifndef _QDOS_
	for(i = 0; i < parTableHead.used; ++i)
	{
		parTable[i].offset = BElong(parTable[i].offset);
		parTable[i].paraLen = BEword(parTable[i].paraLen);
	}
#endif

	/* goto layout table head and read it */

	fseek(stdin, header.textLen + header.freeLen + header.paraLen, SEEK_SET);
	bytes = fread(&layoutTable, 1, LayoutTableSize, stdin);
#ifndef _QDOS_
	layoutTable.wordCount = BEword(layoutTable.wordCount);
	layoutTable.maxTabSize = BEword(layoutTable.maxTabSize);
	layoutTable.tabSize = BEword(layoutTable.tabSize);
#endif

	/* allocate memory and read the tab entries table */

	tabTable = safe_malloc(layoutTable.tabSize);
	bytes = fread(tabTable, 1, layoutTable.tabSize, stdin);

	/* rewind to start of text area, just after the 20 byte header */

	fseek(stdin, header.len, SEEK_SET);
	offset = 0;

	/* and, finally, decode the actual text */

	lineNo = 2;
	pageNo = 1;
	done = 0;
	paraCount = 0;

	maxLines = layoutTable.pageLen - layoutTable.topMargin - layoutTable.bottomMarg;
	if(layoutTable.pageLen == 0 || maxLines < 1)
		maxLines = 0;		/* disable automatic page breaks */
	else if(layoutTable.footerF)
		maxLines -= 1;

	currPara = getPara(offset);

	/* get header  */

	i = 0;
	headerPara[0] = 0;
	while(textBuffer[offset] != END_PARA)
		headerPara[i++] = textBuffer[offset++];
	++paraCount;
	++offset;

	/* get footer */

	i = 0;
	footerPara[0] = 0;
	while(textBuffer[offset] != END_PARA)
		footerPara[i++] = textBuffer[offset++];
	++offset;
	++paraCount;

	/* find out the smallest and largest margins in document, used for header/footer */

	minLmarg = 100;
	maxRmarg = 0;

	for(i = 3; i < parTableHead.used; ++i)					/* skip first entry (set *i = 1), always garbage? */
	{
		minLmarg = min(parTable[i].leftMarg, minLmarg);
		maxRmarg = max(parTable[i].rightMarg, maxRmarg);
	}

	if(format == Text)	/* Show text version */
	{
#ifndef _QDOS_
		putchar(0xEF);
		putchar(0xBB);
		putchar(0xBF);
#endif
		renderNewLine = "\n";
		renderSpace = " ";
		renderParaStart = "";
		renderParaEnd = "";
	}
	else
	{
		renderNewLine = "<br>\n";
		renderSpace = "&nbsp;";
		renderParaStart = "<p>";
		renderParaEnd = "</p>";
		printf(HTML_HEAD);
	}

	currPara = getPara(offset);

	if(currPara == NULL)
		currPara = &defaultPara;

	while(! done)
	{
		printPara(currPara);

		ch = getByte();

		switch(ch)
		{
		case EOF:
			done = true;
			break;
		case END_PARA:
			{
				ParaTable *newPara = getPara(offset);
				currPara = newPara ? newPara : currPara;
				break;
			}
		case END_TEXT:
			done = 1;
			break;
		}
	}

	if(format == Text)
	{
		fprintf(stdout, "\n\n____________________________________________________________________\n");
		fprintf(stdout, "File: %s\nTranslated by %s (compiled %s)\n", srcfile, ME, __DATE__);
	}
	else
	{
		char tmp[MAX_PATH + 64];

		printf(renderParaStart);
		renderLine("_____________________________________________________________________________");
		sprintf(tmp, "File: %s", srcfile);
		renderLine(tmp);
		sprintf(tmp, "Translated by %s (compiled %s)", ME, __DATE__);
		printf(renderParaEnd);

		printf(HTML_TAIL);
	}
}

/*------------------------------------------------------------------------------- */
#ifdef _WIN32
void fixFileName(char *fname)
{
	int i;

	if(*fname == '"')			/* if file name within quotes, remove them */
	{
		for(i = 1; fname[i] && fname[i] != '"'; ++i)
			fname[i-1] = fname[i];

		fname[i-1] = 0;
	}
}
/*------------------------------------------------------------------------------- */
__stdcall WinMain(HINSTANCE inst, HINSTANCE pInst, LPSTR cmdLn, int show)
{
	int			argc;
	char		*argv[MAX_ARGV];
	char		*cmdLine;
	char		*targetFile = "";
	char		*sourceFile = "";
	FILE		*fpin, *fpout;
	int			i;


	/* build argc and argv manually - Win32 sucks... */

	cmdLine = GetCommandLine();
	argc = 0;

	for(i = 0; i < MAX_ARGV; ++i)
	{
		while(*cmdLine && isspace(*cmdLine))
			++cmdLine;

		if(*cmdLine != 0)
		{
			argv[i] = cmdLine;
			++argc;
		} else
			argv[i] = NULL;

		while(*cmdLine && ! isspace(*cmdLine))
		{
			if(*cmdLine == '"')
			{
				do
					++cmdLine;
				while(*cmdLine && *cmdLine != '"');
				++cmdLine;
			}
			else
				++cmdLine;
		}

		if(*cmdLine != 0)
		{
			*cmdLine = 0;
			++cmdLine;
		}
	}

	if(argc == 2)								/* drag and drop mode, only file name on command line */
	{
		if(GetKeyState(VK_MENU) & 0xff00)
			format = Text;						/* If Alt-Key down, show in notepad.exe, otherwise in browser */
		else
			format = Html;

		fixFileName(argv[1]);
		sourceFile = argv[1];

		if(format == Text)
			targetFile = "quill-viev.txt";
		else
			targetFile = "quill-viev.html";
	}
	else if(argc == 4)							/* format is "quill-view [-t|-m] infile outfile" */
	{
		if(stricmp(argv[1], "-t") == 0)
			format = Text;
		else if(stricmp(argv[1], "-h") == 0)
			format = Html;
		else
			usage();

		fixFileName(argv[2]);
		fixFileName(argv[3]);

		sourceFile = argv[2];
		targetFile = argv[3];
	}
	else										/* insufficient user IQ, help the usr... */
	{
		usage();
	}

	fpin = freopen(sourceFile, "rb+", stdin);

	if(fpin == NULL)
		io_error("quil-view: can't open file %s, '%s'", sourceFile);

	fpout = freopen(targetFile, "w", stdout);

	if(fpout == NULL)
		io_error("quil-view: can't open file %s, '%s'", targetFile);

	translate(sourceFile);

	fclose(fpout);

	if(argc == 2)
		ShellExecute(NULL, "open", targetFile, NULL, NULL, SW_SHOWNORMAL);
}

#else	/* if not _WIN32 */

int main(int argc, char *argv[])
{
	char		*sourceFile = "(stdin)";
	char		*targetFile = "";
	FILE		*fpin, *fpout;
	int			i;

	format = Text;
	i = 1;

	if(i < argc)
	{
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
				usage();
		}
		else if(strcmp(argv[i], "-t") == 0)
		{
				format = Text;
				++i;
		}
		else if(strcmp(argv[i], "-m") == 0)
		{
				format = Html;
				++i;
		}
	}

	if(i < argc)
	{
		sourceFile = argv[i];

		fpin = freopen(sourceFile, "rb+", stdin);
		if(fpin == NULL)
			io_error("quill-view: can't open file %s, '%s'\n", sourceFile);

		++i;
	}

	if(i < argc)
	{
		targetFile = argv[i];
		fpout = freopen(targetFile, "w", stdout);
		if(fpout == NULL)
			io_error("quill-view: can't open file %s, '%s'\n", targetFile);

		++i;
	}

	translate(sourceFile);

	return 0;
}
#endif
