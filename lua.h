#ifndef LUA_H
#define LUA_H


#define OP_MASK 0x0000003F // 0000.0000  0000.0000  0000.0000  0011.1111
#define A_MASK  0x00003FC0 // 0000.0000  0000.0000  0011.1111  1100.0000
#define B_MASK  0xFF800000 // 1111.1111  1000.0000  0000.0000  0000.0000
#define C_MASK  0x007FC000 // 0000.0000  0111.1111  1100.0000  0000.0000
#define Bx_MASK 0xFFFFC000 // 1111.1111  1111.1111  1100.0000  0000.0000
#define Ax_MASK 0XFFFFFFC0 // 1111.1111  1111.1111  1111.1111  1100.0000

#define ARGK(a) (a & (1 << 8))

#define GET_OPCODE(i) ((i & OP_MASK) >> 0)
#define GET_A(i)      ((i & A_MASK)  >> 6)
#define GET_B(i)      ((i & B_MASK)  >> 23)
#define GET_C(i)      ((i & C_MASK)  >> 14)
#define GET_Bx(i)     ((i & Bx_MASK) >> 14)
#define GET_Ax(i)     ((i & Ax_MASK) >> 6)

#define SET_OPCODE(i, op) i = ((i & ~OP_MASK) | op << 0)
#define SET_A(i, a)       i = ((i & ~A_MASK)  | a  << 6)
#define SET_B(i, b)       i = ((i & ~B_MASK)  | (b   < 0?(-b   - 1) | (1 << 8) :b)   << 23)
#define SET_C(i, c)       i = ((i & ~C_MASK)  | (c   < 0?(-c   - 1) | (1 << 8) :c)   << 14)
#define SET_Bx(i, bx)     i = ((i & ~Bx_MASK) | (bx  < 0?(-bx  - 1) | (1 << 18):bx)  << 14)
#define SET_sBx(i, sbx)   i = ((i & ~Bx_MASK) | (sbx > 0?-(-sbx + 0x20001)     :-sbx) << 14)
#define SET_Ax(i, ax)     i = ((i & ~Ax_MASK) | ax - 1 << 6)

#define sBC(i) ((i&0x0100)?-(i-0xFF):i)
#define sBx(i) ((i&0x020000)?-(i-0x01FFFF):i)
#define Bx(i)  (-(i+1))

typedef enum ConstType_ {
	LUA_NIL    = 0,
	LUA_BOOL   = 1,
	LUA_DOUBLE = 3,
	LUA_STRING = 4
} ConstType;

typedef enum InstrFormat_ {
	iABC, iABx, iAx, iAsBx
} InstrFormat;

typedef enum ArgType_ {
	Kst, Usg, Reg, Nil
} ArgType;

typedef struct __attribute__ ((packed)) FunctionHeader_ {
	unsigned long startLine;
	unsigned long endLine;
	unsigned char params;
	unsigned char vararg;
	unsigned char registers;
} FunctionHeader;

typedef struct LuaConstant_ {
	unsigned char type;
	unsigned long length;
	char *str;
	unsigned char boolean;
	double number;
} LuaConstant;

typedef struct LuaUpval_ {
	unsigned char val1;
	unsigned char val2;
} LuaUpval;

typedef struct LuaFunction_ {
	FunctionHeader header;
	
	unsigned long instrNum;
	unsigned long *instrTab;
	
	unsigned long constNum;
	LuaConstant *constTab;
	
	unsigned long functNum;
	struct LuaFunction_ *functTab;
	
	unsigned long upvalNum;
	LuaUpval *upvalTab;
	
	unsigned long fileOffset;

	char *functName;
} LuaFunction;


const char Lua52Header[] = {0x1B, 0x4C, 0x75, 0x61, 0x52, 0x00, 0x01, 0x04, 0x04, 0x04, 0x08, 0x00, 0x19, 0x93, 0x0D, 0x0A, 0x1A, 0x0A};

typedef enum LuaOpcode_ {
	MOVE, LOADK, LOADKX, LOADBOOL, LOADNIL, GETUPVAL, GETTABUP, GETTABLE, SETTABUP, SETUPVAL,
	SETTABLE, NEWTABLE, SELF, ADD, SUB, MUL, DIV, MOD, POW, UNM, NOT, LEN, CONCAT, JMP, EQ, LT, LE, TEST,
	TESTSET, CALL, TAILCALL, RETURN, FORLOOP, FORPREP, TFORCALL, TFORLOOP, SETLIST, CLOSURE, VARARG, EXTRAARG
} LuaOpcode;

const char *const OpName[] = {
	"MOVE", "LOADK", "LOADKX", "LOADBOOL", "LOADNIL", "GETUPVAL", "GETTABUP", "GETTABLE", "SETTABUP", "SETUPVAL",
	"SETTABLE", "NEWTABLE", "SELF", "ADD", "SUB", "MUL", "DIV", "MOD", "POW", "UNM", "NOT", "LEN", "CONCAT", "JMP", "EQ", "LT", "LE", "TEST",
	"TESTSET", "CALL", "TAILCALL", "RETURN", "FORLOOP", "FORPREP", "TFORCALL", "TFORLOOP", "SETLIST", "CLOSURE", "VARARG", "EXTRAARG"
};

const char OpFormat[] = {
	iABC, iABx, iABx, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC,
	iABC, iABC, iABC, iAsBx, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iABC, iAsBx, iAsBx, iABC, iAsBx, iABC, iABx, iABC, iAx
};

const ArgType BFormat[] = {
	Reg, Kst, Nil, Usg, Usg, Usg, Usg, Reg, Kst, Usg, Kst, Usg, Reg, Kst, Kst, Kst, Kst, Kst, Kst, Reg,
	Reg, Reg, Reg, Reg, Kst, Kst, Kst, Nil, Reg, Usg, Usg, Usg, Reg, Reg, Nil, Reg, Usg, Usg, Usg, Usg
};

const ArgType CFormat[] = {
	Nil, Nil, Nil, Usg, Nil, Nil, Kst, Kst, Kst, Nil, Kst, Usg, Kst, Kst, Kst, Kst, Kst, Kst, Kst, Nil, 
	Nil, Nil, Reg, Nil, Kst, Kst, Kst, Usg, Usg, Usg, Usg, Nil, Nil, Nil, Usg, Nil, Usg, Nil, Nil, Usg
};

LuaFunction readFunction(int fd, char *functName);	
LuaConstant readConstant(int fd);
void printFunction(LuaFunction f, char *indent);
void printConstant(LuaConstant k);


LuaFunction readFunctionASM (char *fi, long position);
void        writeFunctionASM(LuaFunction f, int fdout);

#endif