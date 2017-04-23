#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LUASM
#include "lua.h"

int main(int argc, char **argv)
{
	int i;
	int fdin = 0;
	int fdout = 0;
	size_t bufSize = 0;
	LuaFunction f;
	char *inBuf = NULL;
	
	if(argc < 3)
	{
		fprintf(stderr, "Usage: %s <input.asm> <output.lua>\n", argv[0]);
		return 1;
	}

	fdin = open(argv[1], O_RDONLY);
	if(fdin == -1)
	{
		fprintf(stderr, "Unable to open file %s.\n", argv[1]);
		return 1;
	}

	fdout = open(argv[2], O_CREAT | O_WRONLY | O_TRUNC | O_BINARY);
	if(fdout == -1)
	{
		fprintf(stderr, "Unable to open file %s.\n", argv[2]);
		return 1;
	}

	bufSize = lseek(fdin, 0, SEEK_END);
	lseek(fdin, 0, SEEK_SET);
	inBuf = malloc(bufSize);
	if(!inBuf)
		return 1;
	i = read(fdin, inBuf, bufSize);
	f = readFunctionASM(inBuf);

	write(fdout, Lua52Header, 18);
	writeFunctionASM(f, fdout);

	free(inBuf);
	close(fdin);
	close(fdout);
	return 0;
}

#define nextToken while(*line && (*line == ' ' || *line == '\t' || *line == '\n' || *line == ';')) \
		  { \
			if(*line == ';') while(*line && *line != '\n') line++; \
		  	*line?line++:line; \
		  }

#define scmp(s1, s2) memcmp(s1, s2, strlen(s2))

LuaFunction readFunctionASM(char *fi)
{
	LuaFunction f;

	unsigned long i = 0, l = 0;

	LuaOpcode op;
	unsigned long a;
	long b, bx, ax, c, s;
	long openBrackets = 0;
	double frac = 0;

	char *line = fi;
	char *instrStart = NULL, *constStart = NULL, *functStart = NULL, *upvalStart = NULL, *lineStart = NULL;

	f.instrNum = 0;
	f.constNum = 0;
	f.functNum = 0;
	f.upvalNum = 0;
	f.header.params = 0;
	f.header.vararg = 0;
	f.header.registers = 0;
	f.header.startLine = 0;
	f.header.endLine = 0;

	nextToken;
	if(*line != '{')
	{
		fprintf(stderr, "Unexpected token, expected '{', found '%c' (%.10s)\n", *line, line);
		exit(2);
	}
	line++;

	nextToken;
	while(*line && scmp(line, "SECTION"))
	{
		if(!scmp(line, ".params"))
		{
			line += strlen(".params");
			nextToken;
			while(*line && *line >= '0' && *line <= '9')
			{
				f.header.params *= 10;
				f.header.params += *line - '0';
				line++;
			}
			if(*line == '+')
			{
				f.header.vararg = 1;
				line++;
			}
			while(*line && *line != '\n')
			{
				while(*line == ' ' || *line == '\t') line++;
				if(*line == ';') while(*line && *line != '\n') line++;
				if(*line != '\n' && *line != ' ' && *line != '\t' && *line != ';')
				{
					fprintf(stderr, "Unexpected token, found '%c' (%.10s)\n", *line, line);
					exit(3);
				}
			}
		}
		else if(!scmp(line, ".slots"))
		{
			line += strlen(".slots");
			nextToken;
			if(*line < '0' || *line > '9')
			{
				fprintf(stderr, "Unexpected token %c, expected number (%.10s)\n", *line, line);
				exit(8);
			}
			while(*line && *line >= '0' && *line <= '9')
			{
				f.header.registers *= 10;
				f.header.registers += *line - '0';
				line++;
			}
			while(*line && *line != '\n')
			{
				while(*line == ' ' || *line == '\t') line++;
				if(*line == ';') while(*line && *line != '\n') line++;
				if(*line != '\n' && *line != ' ' && *line != '\t' && *line != ';')
				{
					fprintf(stderr, "Unexpected token, found '%c' (%.10s)\n", *line, line);
					exit(4);
				}
			}
		}
		else
		{
			fprintf(stderr, "Unexpected token, found '%c' (%.10s)\n", *line, line);
			exit(5);
		}

		line++;
		nextToken;
	}

	while(!scmp(line, "SECTION"))
	{
		if(!scmp(line, "SECTION TEXT::"))
		{
			line += strlen("SECTION TEXT::");
			nextToken;
			if(instrStart)
			{
				fprintf(stderr, "Found SECTION TEXT:: twice in one function at %ll and %ll.\n", fi - instrStart, fi - line);
				exit(6);
			}
			instrStart = line;

			while(scmp(line, "SECTION") && *line != '}' && *line)
			{
				f.instrNum++;
				while(*line && *line != '\n') line++;
				nextToken;
			}
			if(!*line)
			{
				fprintf(stderr, "Unexpected EOF\n");
				exit(33);
			}
			f.instrTab = malloc(sizeof(unsigned long) * f.instrNum);
			line = instrStart;
			for(i = 0; i < f.instrNum; i++)
			{
				nextToken;
				lineStart = line;
				while(*line && *line != '\n' && *line != ';')
				{
					if(*line == ':')
					{
						line++;
						nextToken;
						lineStart = line;
						break;
					}
					line++;
				}
				line = lineStart;
				
				for(op = 0; op <= EXTRAARG + 1; op++)
				{
					if(op == EXTRAARG + 1)
					{
						fprintf(stderr, "Unknown opcode (%.10s).\n", line);
						exit(7);
					}
					if(!scmp(line, OpName[op]) && (*(line + strlen(OpName[op])) == ' ' || *(line + strlen(OpName[op])) == '\t'))
						break;
				}
				line += strlen(OpName[op]);
				nextToken;

				a = 0;
				b = 0;
				c = 0;
				bx = 0;
				ax = 0;
				f.instrTab[i] = 0;
				SET_OPCODE(f.instrTab[i], op);

				switch(OpFormat[op])
				{
					case iABC:
						if(*line < '0' || *line > '9')
						{
							fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
							exit(9);
						}
						while(*line && *line >= '0' && *line <= '9')
						{
							a *= 10;
							a += *line - '0';
							line++;
						}
						SET_A(f.instrTab[i], a);
						if(BFormat[op] != Nil)
						{
							while(*line && (*line == ' ' || *line == '\t'))
								line++;
							if((*line < '0' || *line > '9') && *line != '-')
							{
								fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
								exit(10);
							}
							s = *line=='-'?-1:1;
							if(*line=='-') line++;
							while(*line && *line >= '0' && *line <= '9')
							{
								b *= 10;
								b += *line - '0';
								line++;
							}
							b *= s;
							SET_B(f.instrTab[i], b);
						}
							
						if(CFormat[op] != Nil)
						{
							while(*line && (*line == ' ' || *line == '\t'))
								line++;
							if((*line < '0' || *line > '9') && *line != '-')
							{
								fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
								exit(11);
							}
							s = *line=='-'?-1:1;
							if(*line=='-') line++;
							while(*line && *line >= '0' && *line <= '9')
							{
								c *= 10;
								c += *line - '0';
								line++;
							}
							c *= s;
							SET_C(f.instrTab[i], c);
						}


						break;
					

					case iABx:
						if(*line < '0' || *line > '9')
						{
							fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
							exit(12);
						}
						while(*line && *line >= '0' && *line <= '9')
						{
							a *= 10;
							a += *line - '0';
							line++;
						}
						SET_A(f.instrTab[i], a);
						if(BFormat[op] == Kst)
						{
							while(*line && (*line == ' ' || *line == '\t'))
								line++;
							if(*line != '-')
							{
								fprintf(stderr, "Unexpected token %c, expected negative number (%.10s).\n", *line, line);
								exit(13);
							}
							line++;
							while(*line && *line >= '0' && *line <= '9')
							{
								bx *= 10;
								bx += *line - '0';
								line++;
							}
							SET_Bx(f.instrTab[i], bx - 1);
						}
						if(BFormat[op] == Usg)
						{
							while(*line && (*line == ' ' || *line == '\t'))
								line++;
							if((*line < '0' || *line > '9') && *line != '-')
							{
								fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
								exit(14);
							}
							s = *line=='-'?-1:1;
							if(*line=='-') line++;
							while(*line && *line >= '0' && *line <= '9')
							{
								bx *= 10;
								bx += *line - '0';
								line++;
							}
							bx *= s;
							SET_Bx(f.instrTab[i], bx);
						}
						break;
					

					case iAsBx:
						if(*line < '0' || *line > '9')
						{
							fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
							exit(12);
						}
						while(*line && *line >= '0' && *line <= '9')
						{
							a *= 10;
							a += *line - '0';
							line++;
						}
						SET_A(f.instrTab[i], a);
						while(*line && (*line == ' ' || *line == '\t'))
							line++;
						if((*line < '0' || *line > '9') && *line != '-')
						{
							fprintf(stderr, "Unexpected token %c, expected number (%.10s).\n", *line, line);
							exit(14);
						}
						s = *line=='-'?-1:1;
						if(*line=='-') line++;
						while(*line && *line >= '0' && *line <= '9')
						{
							bx *= 10;
							bx += *line - '0';
							line++;
						}
						bx *= s;
						SET_sBx(f.instrTab[i], bx);
						break;
					

					case iAx:
						while(*line && (*line == ' ' || *line == '\t'))
							line++;
						if(*line != '-')
						{
							fprintf(stderr, "Unexpected token %c, expected negative number (%.10s).\n", *line, line);
							exit(15);
						}
						line++;
						while(*line && *line >= '0' && *line <= '9')
						{
							ax *= 10;
							ax += *line - '0';
							line++;
						}
						SET_Ax(f.instrTab[i], ax);
						break;
					

					default:
						break;
				}
				/*printf("%02X %02X %02X %02X\n", 
					(f.instrTab[i] >> 0 )&0xFF,
					(f.instrTab[i] >> 8 )&0xFF,
					(f.instrTab[i] >> 16)&0xFF,
					(f.instrTab[i] >> 24)&0xFF
				);*/
			}
		} else if(!scmp(line, "SECTION CONST::")) {
			line += strlen("SECTION CONST::");
			nextToken;
			if(constStart)
			{
				fprintf(stderr, "Found SECTION CONST:: twice in one function at %ll and %ll.\n", fi - instrStart, fi - line);
				exit(16);
			}
			constStart = line;

			while(scmp(line, "SECTION") && *line != '}' && *line)
			{
				f.constNum++;
				while(*line && *line != '\n') line++;
				nextToken;
			}

			if(!*line)
			{
				fprintf(stderr, "Unexpected EOF\n");
				exit(32);
			}

			f.constTab = malloc(sizeof(LuaConstant) * f.constNum);
			line = constStart;
			for(i = 0; i < f.constNum; i++)
			{
				nextToken;
				lineStart = line;
				while(*line && *line != '\n' && *line != ';')
				{
					if(*line == ':' || *line =='"')
					{
						line++;
						nextToken;
						lineStart = line;
						break;
					}
					line++;
				}
				line = lineStart;
				nextToken;

				if(!scmp(line, "nil"))
				{
					f.constTab[i].type = LUA_NIL;
					line += strlen("nil");
				} else if(!scmp(line, "true")) {
					f.constTab[i].type = LUA_BOOL;
					f.constTab[i].boolean = 1;
					line += strlen("true");
				} else if(!scmp(line, "false")) {
					f.constTab[i].type = LUA_BOOL;
					f.constTab[i].boolean = 0;
					line += strlen("false");
				} else if((*line >= '0' && *line <= '9') || *line == '-' || *line == '.') {
					f.constTab[i].type = LUA_DOUBLE;
					s = 1;
					if(*line == '-')
					{
						s = -1;
						line++;
					}
					f.constTab[i].number = 0;
					while(*line >= '0' && *line <= '9')
					{
						f.constTab[i].number *= 10;
						f.constTab[i].number += *line - '0';
						line++;
					}
					if(*line == '.')
					{
						frac = 0.1;
						line++;
						while(*line >= '0' && *line <= '9')
						{
							f.constTab[i].number += (*line - '0') * frac;
							frac /= 10;
							line++;
						}
					}
					f.constTab[i].number *= s;
					if(*line != ' ' && *line != '\t' && *line != '\n' && *line != ';')
					{
						fprintf(stderr, "Unexpected token (%.10s)\n", line);
						exit(17);
					}
				} else if(*line == '"') {
					f.constTab[i].type = LUA_STRING;
					lineStart = ++line;
					while(*line && *line != '"')
					{
						if(*line == '\n' || !*line)
						{
							fprintf(stderr, "Unterminated string (%.10s)\n", lineStart);
							exit(18);
						}
						if(*line == '\\')
						{
							line++;
						}
						line++;
					}
					l = 0;
					f.constTab[i].str = malloc(line - lineStart + 1);
					memset(f.constTab[i].str, 0, line - lineStart + 1);
					line = lineStart;
					while(*line != '"')
					{
						if(*line == '\\')
						{
							line++;
							switch(*line)
							{
								case '\\':
									f.constTab[i].str[l] = '\\';
									break;

								case '"':
									f.constTab[i].str[l] = '"';
									break;
								
								case 'n':
									f.constTab[i].str[l] = '\n';
									break;

								case 't':
									f.constTab[i].str[l] = '\t';
									break;

								case 'r':
									f.constTab[i].str[l] = '\r';
									break;
								case 'x':
									line++;
									if(*line >= '0' && *line <= '9')
										f.constTab[i].str[l] = *line - '0';
									else if(toupper(*line) >= 'A' && toupper(*line) <= 'F')
										f.constTab[i].str[l] = toupper(*line) - 'A' + 10;
									else {
										fprintf(stderr, "%c is not a hex number\n", *line);
										exit(19);
									}
									line++;
									f.constTab[i].str[l] *= 16;
									if(*line >= '0' && *line <= '9')
										f.constTab[i].str[l] += *line - '0';
									else if(toupper(*line) >= 'A' && toupper(*line) <= 'F')
										f.constTab[i].str[l] += toupper(*line) - 'A' + 10;
									else {
										fprintf(stderr, "%c is not a hex number\n", *line);
										exit(20);
									}
									break;

								default:
									fprintf(stderr, "\\%c isn't a correct escape character\n", *line);
									exit(21);
							}
						} else
							f.constTab[i].str[l] = *line;
						line++;
						l++;
					}
					line++;
					if(*line != ' ' && *line != '\t' && *line != '\n' && *line != ';')
					{
						fprintf(stderr, "Unexpected token (%.10s)\n", line);
						exit(22);
					}
				} else {
					fprintf(stderr, "Unknown data type (%.10s)\n", line);
					exit(23);
				}
			}
		} else if(!scmp(line, "SECTION FUNCTIONS::")) {
			line += strlen("SECTION FUNCTIONS::");

			nextToken;
			if(functStart)
			{
				fprintf(stderr, "Found SECTION FUNCTIONS:: twice in one function at %ll and %ll.\n", fi - functStart, fi - line);
				exit(24);
			}
			functStart = line;
			if(*line != '{')
			{
				fprintf(stderr, "Unexpected token, expected '{', found '%c' (%.10s)\n", *line, line);
				exit(25);
			}
			line++;
			openBrackets = 1;
			while((scmp(line, "SECTION") || openBrackets) && *line)
			{
				if(*line == '{')
					openBrackets++;
				if(*line == '}')
				{
					openBrackets--;
					if(!openBrackets)
						f.functNum++;
				}
				line++;
				nextToken;
				if(*line == '"')
				{
					line++;
					while(*line && *line != '"' && *line != '\n')
						line++;
				}
			}
			/*if(!*line)
			{
				fprintf(stderr, "Unexpected EOF\n");
				exit(31);
			}*/
			f.functTab = malloc(sizeof(LuaFunction) * f.functNum);
			line = functStart;
			for(i = 0; i < f.functNum; i++)
			{
				f.functTab[i] = readFunctionASM(line);
				openBrackets = 1;
				line++;
				while(openBrackets && *line)
				{
					if(*line == '{')
						openBrackets++;
					if(*line == '}')
						openBrackets--;
					line++;
					nextToken;
					if(*line == '"')
					{
						line++;
						while(*line && *line != '"' && *line != '\n')
							line++;
					}
				}
			}
		} else if(!scmp(line, "SECTION UPVALUES::")) {
			line += strlen("SECTION UPVALUES::");
			
			nextToken;
			if(upvalStart)
			{
				fprintf(stderr, "Found SECTION UPVALUES:: twice in one function at %ll and %ll.\n", fi - upvalStart, fi - line);
				exit(26);
			}
			upvalStart = line;

			while(scmp(line, "SECTION") && *line != '}' && *line)
			{
				f.upvalNum++;
				while(*line && *line != '\n') line++;
				nextToken;
			}
			if(!*line)
			{
				fprintf(stderr, "Unexpected EOF\n");
				exit(29);
			}

			f.upvalTab = malloc(sizeof(LuaConstant) * f.upvalNum);
			line = upvalStart;
			for(i = 0; i < f.upvalNum; i++)
			{
				nextToken;
				lineStart = line;
				while(*line && *line != '\n' && *line != ';')
				{
					if(*line == ':' || *line =='"')
					{
						line++;
						nextToken;
						lineStart = line;
						break;
					}
					line++;
				}
				line = lineStart;
				nextToken;

				f.upvalTab[i].val1 = 0;
				f.upvalTab[i].val2 = 0;

				if(*line < '0' || *line > '9')
				{
					fprintf(stderr, "Expected number (%.10s)", line);
					exit(27);
				}
				while(*line >= '0' && *line <= '9')
				{
					f.upvalTab[i].val1 *= 10;
					f.upvalTab[i].val1 += *line - '0';
					line++;
				}
				while((*line == ' ' || *line == '\t') && *line)
					line++;
				if(*line < '0' || *line > '9')
				{
					fprintf(stderr, "Expected number (%.10s)", line);
					exit(28);
				}
				while(*line >= '0' && *line <= '9')
				{
					f.upvalTab[i].val2 *= 10;
					f.upvalTab[i].val2 += *line - '0';
					line++;
				}
			}
		} else {
			fprintf(stderr, "SECTION name unknown (%s)\n", (line + strlen("SECTION")));
			exit(30);
		}
		line++;
		nextToken;
	}

	return f;
}

void writeFunctionASM(LuaFunction f, int fdout)
{
	long i, l;
	char padding0[16] = {0};
	write(fdout, &f.header, sizeof(FunctionHeader));
	write(fdout, &f.instrNum, sizeof(unsigned long));
	write(fdout, f.instrTab, f.instrNum * sizeof(unsigned long));
	write(fdout, &f.constNum, sizeof(unsigned long));
	for(i = 0; i < f.constNum; i++)
	{
		write(fdout, &f.constTab[i].type, 1);
		switch(f.constTab[i].type)
		{
			case LUA_BOOL:
				write(fdout, &f.constTab[i].boolean, 1);
				break;
			
			case LUA_DOUBLE:
				write(fdout, &f.constTab[i].number, sizeof(double));
				break;

			case LUA_STRING:
				l = strlen(f.constTab[i].str) + 1;
				write(fdout, &l, sizeof(long));
				write(fdout, f.constTab[i].str, l);
				break;

			default:
				break;
		}
	}
	write(fdout, &f.functNum, sizeof(unsigned long));
	for(i = 0; i < f.functNum; i++)
		writeFunctionASM(f.functTab[i], fdout);
	write(fdout, &f.upvalNum, sizeof(unsigned long));
	write(fdout, f.upvalTab, f.upvalNum * sizeof(LuaUpval));
	write(fdout, padding0, 16);
}
