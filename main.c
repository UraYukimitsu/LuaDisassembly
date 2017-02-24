#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "lua.h"

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    if(!result)
		exit(1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

int main(int argc, char **argv)
{
	int fd = 0;
	char header[18] = {0};
	LuaFunction f;
	
	if(argc == 1)
		return 1;
		
	fd = open(argv[1], O_RDONLY | O_BINARY);
	if(fd == -1)
	{
		fprintf(stderr, "Unable to open %s.", argv[1]);
		return 1;
	}
	
	read(fd, header, 18);
	if(memcmp(Lua52Header, header, 18))
	{
		fprintf(stderr, "Header doesn't match. Make sure that the file is a compiled LUA 5.2 file.\n");
		return 1;
	}
	
	f = readFunction(fd);
	printFunction(f, "", "main");
	
	return 0;
}

LuaFunction readFunction(int fd)
{
	LuaFunction f;
	long i, tmp, tmp2;
	f.fileOffset = lseek(fd, 0, SEEK_CUR);
	
	read(fd, &f.header, sizeof(f.header));
	read(fd, &f.instrNum, sizeof(long));
	
	f.instrTab = malloc(sizeof(long) * f.instrNum);
	if(!f.instrTab)
		exit(1);
	read(fd, f.instrTab, sizeof(long) * f.instrNum);
	
	read(fd, &f.constNum, sizeof(long));
	if(f.constNum)
	{
		f.constTab = malloc(f.constNum * sizeof(LuaConstant));
		if(!f.constTab)
			exit(1);
		for(i = 0; i < f.constNum; i++)
		f.constTab[i] = readConstant(fd);
	}
		
	read(fd, &f.functNum, sizeof(long));
		
	if(f.functNum)
		f.functTab = malloc(f.functNum * sizeof(LuaFunction));
	if(!f.functTab)
		exit(1);
	for(i = 0; i < f.functNum; i++)
		f.functTab[i] = readFunction(fd);
		
	read(fd, &f.upvalNum, sizeof(long));
	if(f.upvalNum)
		f.upvalTab = malloc(sizeof(LuaUpval)*f.upvalNum);
	if(!f.upvalTab)
		exit(1);
	
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
				exit(1);
			read(fd, ret.str, ret.length);
			break;
		
		default:
			break;
	}
	return ret;
}

void printFunction(LuaFunction f, char *indent, char *functName)
{
	long op, a, b, c, bx, ax;
	long i;
	
	size_t needed;
	
	char *childName = NULL;	
	char *indentpp = concat(indent, "\t");
	
	printf("%s%s(", indent, functName);
	for(i = 0; i < f.header.params; i++)
	{
		printf("A%d", i);
		if(i + 1 < f.header.params || f.header.vararg)
			printf(", ");
	}
	for(i = 0; i < f.header.vararg; i++)
	{
		printf("V%d", i);
		if(i + 1 < f.header.vararg)
			printf(", ");
	}
	printf(")\n");
	printf("%s{\n", indent);
	printf("%s;%s <%d,%d> (%d instructions at %08X)\n"
		"%s;%d%s params, %d constants, %d slots, %d upvalues\n"
		"%sSECTION TEXT::\n",
		indentpp, functName, f.header.startLine, f.header.endLine, f.instrNum, f.fileOffset,
		indentpp, (int)f.header.params, f.header.vararg?"+":"", f.constNum, (int)f.header.registers, f.upvalNum,
		indentpp
	);
	
	for(i = 0; i < f.instrNum; i++)
	{
		/*printf("%s\t%02X %02X %02X %02X", 
			indentpp,
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
				printf("%d %d", a, sBx(bx));
				break;
			
			case iAx:
				printf("%d", (-1-ax));
				break;
			
			default:
				break;
		}
		

	} 
	
	if(f.constNum)
		printf("%s;constants (%d) for %08X:\n%sSECTION CONST::", indentpp, f.constNum, f.fileOffset, indentpp);
	for(i = 0; i < f.constNum; i++)
	{
		printf("\n%s\t%-3d:\t", indentpp, i+1);
		printConstant(f.constTab[i]);
	}
	
	if(f.upvalNum)
		printf("\n%s;upvalues (%d) for %08X:\n%sSECTION UPVALUES::", indentpp, f.upvalNum, f.fileOffset, indentpp);
	for(i = 0; i < f.upvalNum; i++)
		printf("\n%s\t%d\t-\t%d\t%d", indentpp, i, (int)f.upvalTab[i].val1, (int)f.upvalTab[i].val2);
	
	if(f.functNum)
		printf("\n%s;functions (%d) for %08X:\n%sSECTION FUNCTIONS::\n", indentpp, f.functNum, f.fileOffset, indentpp);
	for(i = 0; i < f.functNum; i++)
	{
		needed  = snprintf(NULL, 0, "%s_%d", strcmp(functName, "main")?functName:"function", i + 1);
		childName = malloc(needed + 1);
		if(!childName)
			exit(1);
		snprintf(childName, needed + 1, "%s_%d", strcmp(functName, "main")?functName:"function", i + 1);
		printFunction(f.functTab[i], indentpp, childName);
		free(childName);
		childName = NULL;
	}
		
	
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
}

void printConstant(LuaConstant k)
{
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
			printf("\"%s\"", k.str);
			break;
		
		default:
			printf("Unknown type (%d)", k.type);
	}
}
