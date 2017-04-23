#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "lua.h"

int main(int argc, char **argv)
{
	int fd = 0;
	char header[18] = {0};
	LuaFunction f;
	
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s <input.lua>\n", argv[0]);
		return 1;
	}
		
	fd = open(argv[1], O_RDONLY | O_BINARY);
	if(fd == -1)
	{
		fprintf(stderr, "Unable to open %s.\n", argv[1]);
		return 1;
	}
	
	read(fd, header, 18);
	if(memcmp(Lua52Header, header, 18))
	{
		fprintf(stderr, "Header doesn't match. Make sure that the file is a compiled LUA 5.2 file.\n");
		return 1;
	}
	
	f = readFunction(fd, "main");
	printFunction(f, "");

	close(fd);
	
	return 0;
}

LuaFunction readFunction(int fd, char *functName)
{
	LuaFunction f;
	long i, tmp, tmp2;
	char *childName = NULL;
	size_t needed;

	f.fileOffset = lseek(fd, 0, SEEK_CUR);

	f.functName = strdup(functName);
	
	read(fd, &f.header, sizeof(f.header));
	read(fd, &f.instrNum, sizeof(long));
	
	f.instrTab = malloc(sizeof(long) * f.instrNum);
	if(!f.instrTab)
		exit(2);
	read(fd, f.instrTab, sizeof(long) * f.instrNum);
	
	read(fd, &f.constNum, sizeof(long));
	if(f.constNum)
	{
		f.constTab = malloc(f.constNum * sizeof(LuaConstant));
		if(!f.constTab)
			exit(3);
		for(i = 0; i < f.constNum; i++)
		f.constTab[i] = readConstant(fd);
	}
		
	read(fd, &f.functNum, sizeof(long));
		
	if(f.functNum)
		f.functTab = malloc(f.functNum * sizeof(LuaFunction));
	if(!f.functTab)
		exit(4);

	for(i = 0; i < f.functNum; i++)
	{
		needed  = snprintf(NULL, 0, "%s_%d", strcmp(f.functName, "main")?f.functName:"function", i + 1);
		childName = malloc(needed + 1);
		if(!childName)
			exit(5);
		snprintf(childName, needed + 1, "%s_%d", strcmp(f.functName, "main")?f.functName:"function", i + 1);
		fprintf(stderr, "\r"); //...For some reason it doesn't work without an instruction here...
		f.functTab[i] = readFunction(fd, childName);
		free(childName);
		childName = NULL;
	}
		
	read(fd, &f.upvalNum, sizeof(long));
	if(f.upvalNum)
		f.upvalTab = malloc(sizeof(LuaUpval)*f.upvalNum);
	if(!f.upvalTab)
		exit(6);
	
	for(i = 0; i < f.upvalNum; i++)
	{
		read(fd, &f.upvalTab[i], 2);
	}
	
	{ //Debug infos; useless here. Just making sure we're skipping the to the right position.
		read(fd, &tmp, sizeof(long));
		lseek(fd, tmp, SEEK_CUR);
		
		read(fd, &tmp, sizeof(long));
		lseek(fd, tmp * 4, SEEK_CUR);
		
		
		read(fd, &tmp, sizeof(long));
		for(i = 0; i < tmp; i++)
		{
			read(fd, &tmp2, sizeof(long));
			lseek(fd, tmp2 + 8, SEEK_CUR);
		}
		
		read(fd, &tmp, sizeof(long));
		for(i = 0; i < tmp; i++)
		{
			read(fd, &tmp2, sizeof(long));
			lseek(fd, tmp2, SEEK_CUR);
		}
	}
	
	return f;
}

LuaConstant readConstant(int fd)
{
	LuaConstant ret = {0, 0, NULL, 0, 0};
	read(fd, &ret.type, sizeof(char));
	switch(ret.type)
	{
		case LUA_BOOL:
			read(fd, &ret.boolean, sizeof(char));
			break;
		
		case LUA_DOUBLE:
			read(fd, &ret.number, sizeof(double));
			break;
			
		case LUA_STRING:
			read(fd, &ret.length, sizeof(long));
			ret.str = malloc(ret.length * sizeof(char));
			if(!ret.str)
				exit(7);
			read(fd, ret.str, ret.length);
			break;
		
		default:
			break;
	}
	return ret;
}

void printFunction(LuaFunction f, char *indent)
{
	unsigned long op, a, b, c, bx, ax;
	unsigned long i;
	int ind = strlen(indent);
	
	char *indentpp = malloc(ind + 2);
	if(!indentpp)
		exit(8);
	strcpy(indentpp, indent);
	indentpp[ind] = '\t';
	indentpp[ind + 1] = '\0';
	
	printf("%s;%s(", indent, f.functName);
	for(i = 0; i < f.header.params; i++)
	{
		printf("A%d", i);
		if(i + 1 < f.header.params || f.header.vararg)
			printf(", ");
	}
	if(f.header.vararg)
		printf("...");
	printf(")\n");
	printf("%s{\n", indent);

	printf("%s.params %d%c\n", indentpp, f.header.params, f.header.vararg?'+':' ');
	printf("%s.slots  %d\n", indentpp, (int)f.header.registers);
	//printf("%s.start  %d\n", indentpp, f.header.startLine);
	//printf("%s.end    %d\n", indentpp, f.header.endLine);

	printf("%s;%s <%d,%d> (%d instructions at %08X)\n"
		"%s;%d%s params, %d constants, %d slots, %d upvalues\n"
		"%sSECTION TEXT::\n",
		indentpp, f.functName, f.header.startLine, f.header.endLine, f.instrNum, f.fileOffset,
		indentpp, (int)f.header.params, f.header.vararg?"+":"", f.constNum, (int)f.header.registers, f.upvalNum,
		indentpp
	);
	
	for(i = 0; i < f.instrNum; i++)
	{
		/*printf("%02X %02X %02X %02X", 
			(f.instrTab[i] >> 0 )&0xFF,
			(f.instrTab[i] >> 8 )&0xFF,
			(f.instrTab[i] >> 16)&0xFF,
			(f.instrTab[i] >> 24)&0xFF
		);*/
		op = GET_OPCODE(f.instrTab[i]);
		a  = GET_A(f.instrTab[i]);
		b  = GET_B(f.instrTab[i]);
		c  = GET_C(f.instrTab[i]);
		ax = GET_Ax(f.instrTab[i]);
		bx = GET_Bx(f.instrTab[i]);
		printf("%s\t%-3d:\t%-9s\t", indentpp, i + 1, OpName[op]);
		switch(OpFormat[op])
		{
			case iABC:
				printf("%d", a);
				if(BFormat[op] != Nil)
					printf(" %d", sBC(b));
				if(CFormat[op] != Nil)
					printf(" %d", sBC(c));
				break;
			
			case iABx:
				if(BFormat[op] == Kst)
					printf("%d %d", a, Bx(bx));
				if(BFormat[op] == Usg)
					printf("%d %d", a, sBx(bx));
				break;
			
			case iAsBx:
				bx -= 0x01FFFF;
				printf("%d %d", a, bx);
				break;
			
			case iAx:
				printf("%d", (-1-ax));
				break;
			
			default:
				break;
		}

		switch (op)
		{
			case LOADK:
				printf("\t; ");
				printConstant(f.constTab[bx]);
				break;

			case GETTABUP:
				if(ARGK(c))
				{
					printf("\t; ");
					printConstant(f.constTab[-sBC(c) - 1]);
				}
				break;

			case SETTABUP:
				printf("\t; ");
				if(ARGK(b))
				{
					printConstant(f.constTab[-sBC(b) - 1]);
				}
				if(ARGK(c))
				{
					printf(" ");
					printConstant(f.constTab[-sBC(c) - 1]);
				}
				break;

			case GETTABLE:
			case SELF:
				if(ARGK(c))
				{
					printf("\t; ");
					printConstant(f.constTab[-sBC(c) - 1]);
				}
				break;

			case SETTABLE:
			case ADD:
			case SUB:
			case MUL:
			case DIV:
			case POW:
			case EQ:
			case LT:
			case LE:
				if(ARGK(b) || ARGK(c))
				{
					printf("\t; ");
					if(ARGK(b))
						printConstant(f.constTab[-sBC(b) - 1]);
					else
						printf("-");
					printf(" ");
					if(ARGK(c))
						printConstant(f.constTab[-sBC(c) - 1]);
					else
						printf("-");
				}
				break;

			case JMP:
			case FORLOOP:
			case FORPREP:
			case TFORLOOP:
				printf("\t; to %d", bx + i + 2);
				break;

			case CLOSURE:
				printf("\t; %s", f.functTab[bx].functName);
				break;

			case SETLIST:
				if(c != 0)
					printf("\t; %d", c);
				else
					printf("\t; %d", (int)f.instrTab[i + 1]); //No idea what this means.
				break;

			case EXTRAARG:
				printf("\t; ");
				printConstant(f.constTab[ax]);
				break;

			default:
				break;

		}

		printf("\n");
	} 
	
	if(f.constNum)
		printf("%s;constants (%d) for %s:\n%sSECTION CONST::", indentpp, f.constNum, f.functName, indentpp);
	for(i = 0; i < f.constNum; i++)
	{
		printf("\n%s\t%-3d:\t", indentpp, i+1);
		printConstant(f.constTab[i]);
	}
	
	if(f.upvalNum)
		printf("\n%s;upvalues (%d) for %s:\n%sSECTION UPVALUES::", indentpp, f.upvalNum, f.functName, indentpp);
	for(i = 0; i < f.upvalNum; i++)
		printf("\n%s\t%-3d:\t%d\t%d", indentpp, i, (int)f.upvalTab[i].val1, (int)f.upvalTab[i].val2);
	
	if(f.functNum)
		printf("\n%s;functions (%d) for %s:\n%sSECTION FUNCTIONS::\n", indentpp, f.functNum, f.functName, indentpp);
	for(i = 0; i < f.functNum; i++)
		printFunction(f.functTab[i], indentpp);
		
	
	printf("\n%s}\n\n", indent);
	free(indentpp);
	
	if(f.instrNum)
		free(f.instrTab);
		
	if(f.constNum)
		free(f.constTab);
		
	if(f.upvalNum)
		free(f.upvalTab);
		
	if(f.functNum)
		free(f.functTab);

	free(f.functName);
}

void printConstant(LuaConstant k)
{
	char *str;
	switch(k.type)
	{
		case LUA_NIL:
			printf("nil");
			break;
		
		case LUA_BOOL:
			printf("%s", k.boolean?"true":"false");
			break;
			
		case LUA_DOUBLE:
			printf("%.14g", k.number);
			break;
			
		case LUA_STRING:
			putc('"', stdout);
			for(str = k.str; *str; str++)
			{
				switch(*str)
				{
					case '\t':
						putc('\\', stdout);
						putc('t', stdout);
						break;
					case '\n':
						putc('\\', stdout);
						putc('n', stdout);
						break;
					case '\r':
						putc('\\', stdout);
						putc('r', stdout);
						break;
					case '\\':
						putc('\\', stdout);
						putc('\\', stdout);
						break;
					case '"':
						putc('\\', stdout);
						putc('"', stdout);
						break;
					default:
						if(!isprint(*str))
							printf("\\x%02x", (long)*str);
						else
							putc(*str, stdout);
				}
			}
			putc('"', stdout);
			break;
		
		default:
			printf("Unknown type (%d)", k.type);
	}
}
