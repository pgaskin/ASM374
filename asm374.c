/* Assembler for the ELEC374 W23 CPU
 * Copyright 2023 Patrick Gaskin
 *
 * Features:
 * - Easy-to-use command-line interactive and scriptable interface.
 * - Can be used as a GTKWave "Transform Filter Program".
 * - Cross-platform, portable, no stdlib deps.
 * - JavaScript WebAssembly bindings.
 * - Flexible assembly syntax.
 * - Modular and easy to extend.
 * - Comprehensive error checking.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Convert the top 4 bits of x to a hex digit.
 */
static char u4_tohex(uint8_t x) {
    return "0123456789ABCDEF"[x&15];
}

/**
 * Convert the the hex digit c to a 4-bit number, returning 0xFF on error.
 */
static uint8_t u4_fromhex(char c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    else if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    else if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    return 0xFF;
}

/**
 * Append w binary digits and a null terminator to str (min length w+1).
 */
static char *u32be_tobin(char *str, uint32_t n, int w) {
    if (!str) return NULL;
    str += w;
    for (char *s = str; w > 0; w--) {
        *--s = '0' + (n&1);
        n >>= 1;
    }
    *str = '\0';
    return str;
}

/**
 * Append 8 hex digits and a null terminator to str, big-endian.
 */
static char *u32be_tohex(char *str, uint32_t n) {
    if (!str) return NULL;
    *str++ = u4_tohex(n>>28);
    *str++ = u4_tohex(n>>24);
    *str++ = u4_tohex(n>>20);
    *str++ = u4_tohex(n>>16);
    *str++ = u4_tohex(n>>12);
    *str++ = u4_tohex(n>>8);
    *str++ = u4_tohex(n>>4);
    *str++ = u4_tohex(n>>0);
    *str = '\0';
    return str;
}

/**
 * Convert 8 hex digits into a uint32, big-endian, returning false if the string
 * is too short/long, null, or contains invalid digits.
 */
static bool u32be_fromhex(uint32_t *n, const char *str) {
    uint32_t x, r = 0;
    if (!str) return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 28; else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 24; else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 20; else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 16; else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 12; else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 8;  else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 4;  else return false;
    if ((x = u4_fromhex(*str++)) != 0xFF) r |= x << 0;  else return false;
    if (*str) return false;
    *n = r;
    return true;
}

/**
 * Convert c to ASCII lowercase.
 */
static char chr_tolower(char c) {
    if ('a' <= c && c <= 'z')
        c -= 'a' - 'A';
    return c;
}

/**
 * Check if c is ASCII whitespace or CR.
 */
static bool chr_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/**
 * Get the length of s.
 */
static size_t str_len(const char *s) {
    const char *a = s;
    while (s && *s) s++;
	return s-a;
}

/**
 * Trim a string in-place, replacing the first triling whitespace with a null
 * terminator, and returning a pointer to the last leading whitespace.
 */
static char *str_trim(char *s) {
    if (!s) return NULL;
    char *a = s, *b = s + str_len(s);
    while (a < b && chr_isspace(*a))
        a++;
    while (b > a && chr_isspace(*(b-1)))
        b--;
    *b = '\0';
    return a;
}

/**
 * Replace the first instance of one of the provided characters with a null
 * byte, returning a pointer to the next character, or NULL if no match was
 * found.
 */
static char *str_spl(char *s, char *x) {
    for (char *c = s; c && *c; c++) {
        for (char *y = x; y && *y; y++) {
            if (*c == *y) {
                *c++ = '\0';
                return c;
            }
        }
    }
    return NULL;
}

/**
 * Compare a and b, optionally case-insensitively.
 */
static bool str_eq(const char *a, const char *b, bool i) {
    if (a && b) {
        while (*a && *b && (*a == *b || (i ? chr_tolower(*a) : *a) == (i ? chr_tolower(*b) : *b))) {
            a++;
            b++;
        }
        return !*a && !*b;
    }
    return false;
}

/**
 * Unsafely copy b onto a, returning a pointer to the new end of a.
 */
static char *str_ecpy(char *a, const char *b) {
    if (a) {
        char c;
        while (b && (c = *b++))
            *a++ = c;
        *a = '\0';
    }
    return a;
}

/**
 * Copy up to n-1 bytes of b into a, set the following one to null, and return a
 * pointer to the null, or NULL if b was too large.
 */
static char *str_ecpyn(char *a, const char *b, size_t n) {
    if (a) {
        char c;
        while (b && (c = *b++) && n > 1) {
            *a++ = c;
            n--;
        }
        if (n) {
            *a = '\0';
        }
    }
    return n ? a : NULL;
}

/**
 * Error codes.
 */
typedef enum Error {
    NoError,
    Error_Parse_EmptyArgument,
    Error_Parse_LongArgument,
    Error_Parse_InvalidArgument,
    Error_Parse_Imm18s_InvalidDigit,
    Error_Parse_Imm18s_OutOfRange,
    Error_Parse_Reg_Unknown,
    Error_Parse_Cond_Unknown,
    Error_Parse_Op_Unknown,
    Error_Parse_Op_MissingCond,
    Error_Parse_RegImm18s_R0,
    Error_Parse_OpArgs_TooMany,
    Error_Parse_OpArgs_NotEnough,
    Error_Inst_Op,
    Error_Inst_Reg,
    Error_Inst_Cond,
    Error_Disassemble_Hex,
} Error;

/**
 * Gets the description for err, or NULL on success.
 */
static const char *GetError(Error err) {
    switch (err) {
    case NoError:
        return NULL;
    case Error_Parse_EmptyArgument:
        return "empty argument";
    case Error_Parse_LongArgument:
        return "argument too long";
    case Error_Parse_InvalidArgument:
        return "invalid argument";
    case Error_Parse_Imm18s_InvalidDigit:
        return "unexpected non-digit in immediate";
    case Error_Parse_Imm18s_OutOfRange:
        return "immediate value out of range";
    case Error_Parse_Reg_Unknown:
        return "unknown register";
    case Error_Parse_Op_Unknown:
        return "unknown op";
    case Error_Parse_Op_MissingCond:
        return "missing condition code";
    case Error_Parse_Cond_Unknown:
        return "unknown condition code";
    case Error_Parse_RegImm18s_R0:
        return "register r0 is forbidden";
    case Error_Parse_OpArgs_TooMany:
        return "too many arguments";
    case Error_Parse_OpArgs_NotEnough:
        return "not enough arguments";
    case Error_Inst_Op:
        return "unknown opcode";
    case Error_Inst_Reg:
        return "unknown register";
    case Error_Inst_Cond:
        return "unknown condition code";
    case Error_Disassemble_Hex:
        return "invalid hexadecimal input (expected 8 hex digits)";
    }
    return "unknown error";
}

/**
 * 18-bit 2's complement signed immediate value.
 */
typedef uint32_t Imm18s; // uint18

/**
 * Writes imm to str, returning a pointer to the end of the updated string.
 */
static char *FormatImm18s(char *str, Imm18s imm) {
    imm &= ((1<<18) - 1);
    if (str) {
        if (!imm) {
            *str++ = '0';
            *str = '\0';
            return str;
        }
        if (imm & (1<<(18-1))) {
            imm = (1<<18) - imm;
            *str++ = '-';
        }
        for (Imm18s tmp = imm; tmp; tmp /= 10)
            str++;
        for (char *ptr = str; imm; imm /= 10)
            *--ptr = imm%10 + '0';
        *str = '\0';
    }
    return str;
}

/**
 * Attempts to parse str as an Imm18s, writing it to imm (if non-null) on
 * success.
 *
 * A sign (+ or -) may be specified at the start of str. The default base is
 * decimal, but str can be prefixed with 0x for hex, 0b for binary, or 0o for
 * octal.
 *
 * Alternatively, if str if prefixed with $, it is parsed as unsigned hex.
 *
 * By default, an error is returned if the provided signed value is out of
 * range. If a sign is not specified, and a base is, the value is treated as an
 * unsigned value (which can include the sign bit).
 */
static Error ParseImm18s(Imm18s *imm, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;

    int base = 10;
    bool neg = false, pos = false;
    if (*str == '$') {
        str++;
        base = 16;
    } else {
        switch (*str) {
        case '+':
            str++;
            pos = true;
            break;
        case '-':
            str++;
            neg = true;
            break;
        }
        if (*str == '0') {
            switch (*++str) {
            case 'x':
                str++;
                base = 16;
                break;
            case 'o':
                str++;
                base = 8;
                break;
            case 'b':
                str++;
                base = 2;
                break;
            }
        }
    }

    uint32_t tmp = 0;
    for (char c = *str++; c; c = *str++) {
        if ('0' <= c && c <= '9')
            c -= '0';
        else if ('a' <= c && c <= 'z')
            c = c - 'a' + 10;
        else if ('A' <= c && c <= 'Z')
            c = c - 'A' + 10;
        else
            return Error_Parse_Imm18s_InvalidDigit;

        if (c >= base)
            return Error_Parse_Imm18s_InvalidDigit;
        tmp *= base;
        tmp += c;

        if (tmp >= 1<<18)
            return Error_Parse_Imm18s_OutOfRange;
    }

    // check signed limits unless we're parsing an explicit base without a sign
    if (neg && tmp > 1<<(18-1))
        return Error_Parse_Imm18s_OutOfRange;
    if (!neg && (pos || base == 10) && tmp >= 1<<(18-1))
        return Error_Parse_Imm18s_OutOfRange;

    if (neg)
        tmp = (1<<18) - tmp; // 2's complement
    if (imm)
        *imm = tmp;
    return NoError;
}

/**
 * General-purpose 32-bit register.
 */
typedef enum Reg {
    Reg_R0  = 0,
    Reg_R1  = 1,
    Reg_R2  = 2,
    Reg_R3  = 3,
    Reg_R4  = 4,
    Reg_R5  = 5,
    Reg_R6  = 6,
    Reg_R7  = 7,
    Reg_R8  = 8,
    Reg_R9  = 9,
    Reg_R10 = 10,
    Reg_R11 = 11,
    Reg_R12 = 12,
    Reg_R13 = 13,
    Reg_R14 = 14,
    Reg_R15 = 15,
    RegCount,
} Reg;

/**
 * GetReg gets the register name, returning NULL if the register does not exist.
 */
static const char *GetReg(Reg reg) {
    if (reg < RegCount) {
        switch (reg) {
        case Reg_R0:   return "r0";
        case Reg_R1:   return "r1";
        case Reg_R2:   return "r2";
        case Reg_R3:   return "r3";
        case Reg_R4:   return "r4";
        case Reg_R5:   return "r5";
        case Reg_R6:   return "r6";
        case Reg_R7:   return "r7";
        case Reg_R8:   return "r8";
        case Reg_R9:   return "r9";
        case Reg_R10:  return "r10";
        case Reg_R11:  return "r11";
        case Reg_R12:  return "r12";
        case Reg_R13:  return "r13";
        case Reg_R14:  return "r14";
        case Reg_R15:  return "r15";
        case RegCount: break;
        }
    }
    return NULL;
}

/**
 * Writes reg to str, returning a pointer to the end of the updated string.
 */
static char *FormatReg(char *str, Reg reg) {
    const char *r = GetReg(reg);
    return str_ecpy(str, r ? r : "?");
}

/**
 * Attempts to parse str as a Reg, writing it to reg (if non-null) on success.
 */
static Error ParseReg(Reg *reg, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;
    for (Reg x = 0; x < RegCount; x++) {
        if (str_eq(GetReg(x), str, true)) {
            if (reg)
                *reg = x;
            return NoError;
        }
    }
    return Error_Parse_Reg_Unknown;
}

/**
 * Condition code.
 */
typedef enum Cond {
    Cond_ZR  = 0,
    Cond_NZ  = 1,
    Cond_PL  = 2,
    Cond_MI  = 3,
    CondCount,
} Cond;

/**
 * GetCond gets the condition code, returning NULL if the code does not exist.
 */
static const char *GetCond(Cond cond) {
    if (cond < CondCount) {
        switch (cond) {
        case Cond_ZR:   return "zr";
        case Cond_NZ:   return "nz";
        case Cond_PL:   return "pl";
        case Cond_MI:   return "mi";
        case CondCount: break;
        }
    }
    return NULL;
}

/**
 * Writes cond to str, returning a pointer to the end of the updated string.
 */
static char *FormatCond(char *str, Cond cond) {
    const char *r = GetCond(cond);
    return str_ecpy(str, r ? r : "?");
}

/**
 * Writes an indexed register to str, returning a pointer to the end of the
 * updated string.
 */
static char *FormatRegImm18s(char *str, Reg reg, Imm18s imm) {
    if (str) {
        str = FormatImm18s(str, imm);
        if (reg != 0) {
            *str++ = '(';
            str = FormatReg(str, reg);
            *str++ = ')';
        }
        *str = '\0';
    }
    return str;
}

/**
 * Attempts to parse str as an indexed register, writing it to reg (if non-null)
 * on success.
 */
static Error ParseRegImm18s(Reg *reg, Imm18s *imm, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;

    char s_imm[256];
    if (!str_ecpyn(s_imm, str, sizeof(s_imm)))
        return Error_Parse_LongArgument;

    char *s_reg = NULL;
    for (char *a = s_imm; *a; a++) {
        if (*a == '(') {
            bool ok = false;
            for (char *b = a; *b; b++) {
                if (*b == ')') {
                    if (!b[1]) {
                        s_reg = a+1;
                        *a = '\0';
                        *b = '\0';
                        ok = true;
                    }
                    break;
                }
            }
            if (ok)
                break;
            return Error_Parse_InvalidArgument;
        }
    }

    Error err;
    if ((err = ParseImm18s(imm, s_imm)))
        return err;
    if (s_reg && (err = ParseReg(reg, s_reg)))
        return err;
    if (s_reg && *reg == 0)
        return Error_Parse_RegImm18s_R0;
    return NoError;
}

/**
 * Instruction opcode.
 */
typedef uint8_t Opcode; // uint5

/**
 * Decoded instruction fields.
 *
 * This contains all possible information about an instruction.
 */
typedef struct Inst {
    Opcode Opcode;
    Cond   C2;
    Reg    Ra;
    Reg    Rb;
    Reg    Rc;
    Imm18s C;
} Inst;

static const Inst INST_ZERO; // would use {}, but that's a GNU extension

/**
 * Instruction argument syntax.
 *
 * This controls how instruction arguments are parsed and formatted from the
 * instruction fields.
 */
typedef enum InstArg {
    InstArg__,
    InstArg_Ra,
    InstArg_Rb,
    InstArg_Rc,
    InstArg_C,
    InstArg_RbC,
} InstArg;

/**
 * Instruction encoding.
 *
 * This controls how the instruction fields are encoded.
 */
typedef enum InstEnc {
    InstEnc_R = 'R',
    InstEnc_I = 'I',
    InstEnc_B = 'B',
    InstEnc_J = 'J',
    InstEnc_M = 'M',
} InstEnc;

/**
 * Instruction specification.
 *
 * This contains the name, encoding, and arguments of an opcode.
 */
typedef struct InstSpec {
    InstEnc Format;
    char    Op[10];
    bool    Cond;
    InstArg Arg[3];
} InstSpec;

/**
 * Instruction table.
 *
 * This contains the mappings of opcodes to instruction specifications.
 */
static const InstSpec InstData[1<<5] = {
    [ 0] = {InstEnc_I, "ld",   false, {InstArg_Ra,  InstArg_RbC, InstArg__ }},
    [ 1] = {InstEnc_I, "ldi",  false, {InstArg_Ra,  InstArg_RbC, InstArg__ }},
    [ 2] = {InstEnc_I, "st",   false, {InstArg_RbC, InstArg_Ra,  InstArg__ }},
    [ 3] = {InstEnc_R, "add",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 4] = {InstEnc_R, "sub",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 5] = {InstEnc_R, "and",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 6] = {InstEnc_R, "or",   false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 7] = {InstEnc_R, "shr",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 8] = {InstEnc_R, "shra", false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [ 9] = {InstEnc_R, "shl",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [10] = {InstEnc_R, "ror",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [11] = {InstEnc_R, "rol",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [12] = {InstEnc_I, "addi", false, {InstArg_Ra,  InstArg_Rb,  InstArg_C }},
    [13] = {InstEnc_I, "andi", false, {InstArg_Ra,  InstArg_Rb,  InstArg_C }},
    [14] = {InstEnc_I, "ori",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_C }},
    [15] = {InstEnc_I, "mul",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [16] = {InstEnc_I, "div",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [17] = {InstEnc_I, "neg",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [18] = {InstEnc_I, "not",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [19] = {InstEnc_B, "br",   true,  {InstArg_Ra,  InstArg_C,   InstArg__ }},
    [20] = {InstEnc_J, "jr",   false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [21] = {InstEnc_J, "jal",  false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [22] = {InstEnc_J, "in",   false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [23] = {InstEnc_J, "out",  false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [24] = {InstEnc_J, "mfhi", false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [25] = {InstEnc_J, "mflo", false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [26] = {InstEnc_M, "nop",  false, {InstArg__,   InstArg__,   InstArg__ }},
    [27] = {InstEnc_M, "halt", false, {InstArg__,   InstArg__,   InstArg__ }},
};

/**
 * Gets the instruction specification for op. Valid if Format is non-zero.
 */
static InstSpec LookupOpcode(Opcode op) {
    return InstData[op & ((1<<5)-1)];
}

/**
 * DecodeInst decodes inst.
 */
static Inst DecodeInst(uint32_t b) {
    Inst i = INST_ZERO;
    switch (LookupOpcode((i.Opcode = (Opcode) ((b >> 27) & ((1<<5)-1)))).Format) {
    case InstEnc_R:
        i.Ra = (Reg)((b >> 23) & ((1<<4)-1));
        i.Rb = (Reg)((b >> 19) & ((1<<4)-1));
        i.Rc = (Reg)((b >> 15) & ((1<<4)-1));
        break;
    case InstEnc_I:
        i.Ra = (Reg)   ((b >> 23) & ((1<<4)-1));
        i.Rb = (Reg)   ((b >> 19) & ((1<<4)-1));
        i.C  = (Imm18s)((b >> 00) & ((1<<18)-1));
        break;
    case InstEnc_B:
        i.Ra = (Reg)   ((b >> 23) & ((1<<4)-1));
        i.C2 = (Cond)  ((b >> 19) & ((1<<4)-1));
        i.C  = (Imm18s)((b >> 00) & ((1<<18)-1));
        break;
    case InstEnc_J:
        i.Ra = (Reg)((b >> 23) & ((1<<4)-1));
        break;
    case InstEnc_M:
        break;
    }
    return i;
}

/**
 * EncodeInst encodes inst.
 */
static uint32_t EncodeInst(Inst i) {
    uint32_t b = ((uint32_t)(i.Opcode) & ((1<<5)-1)) << 27;
    switch (LookupOpcode(i.Opcode).Format) {
    case InstEnc_R:
        b |= ((uint32_t)(i.Ra) & ((1<<4)-1)) << 23;
        b |= ((uint32_t)(i.Rb) & ((1<<4)-1)) << 19;
        b |= ((uint32_t)(i.Rc) & ((1<<4)-1)) << 15;
        break;
    case InstEnc_I:
        b |= ((uint32_t)(i.Ra) & ((1<<4)-1))  << 23;
        b |= ((uint32_t)(i.Rb) & ((1<<4)-1))  << 19;
        b |= ((uint32_t)(i.C)  & ((1<<18)-1)) << 0;
        break;
    case InstEnc_B:
        b |= ((uint32_t)(i.Ra) & ((1<<4)-1))  << 23;
        b |= ((uint32_t)(i.C2) & ((1<<4)-1))  << 19;
        b |= ((uint32_t)(i.C)  & ((1<<18)-1)) << 0;
        break;
    case InstEnc_J:
        b |= ((uint32_t)(i.Ra) & ((1<<4)-1)) << 23;
        break;
    case InstEnc_M:
        break;
    }
    return b;
}

/**
 * CheckInst checks whether inst is valid.
 */
static Error CheckInst(Inst i) {
    switch (LookupOpcode(i.Opcode).Format) {
    case InstEnc_R:
        if (!GetReg(i.Ra)) return Error_Inst_Reg;
        if (!GetReg(i.Rb)) return Error_Inst_Reg;
        if (!GetReg(i.Rc)) return Error_Inst_Reg;
        return NoError;
    case InstEnc_I:
        if (!GetReg(i.Ra)) return Error_Inst_Reg;
        if (!GetReg(i.Rb)) return Error_Inst_Reg;
        return NoError;
    case InstEnc_B:
        if (!GetReg(i.Ra))   return Error_Inst_Reg;
        if (!GetCond(i.C2)) return Error_Inst_Cond;
        return NoError;
    case InstEnc_J:
        if (!GetReg(i.Ra)) return Error_Inst_Reg;
        return NoError;
    case InstEnc_M:
        return NoError;
    }
    return Error_Inst_Op;
}

/**
 * Writes inst to str, returning a pointer to the end of the updated string.
 */
static char *FormatInst(char *str, Inst inst) {
    if (str) {
        InstSpec spec = LookupOpcode(inst.Opcode);
        if (spec.Format) {
            str = str_ecpy(str, spec.Op);
            if (spec.Cond)
                str = FormatCond(str, inst.C2);
            for (size_t i = 0; i < sizeof(spec.Arg)/sizeof(*spec.Arg) && spec.Arg[i]; i++) {
                if (i)
                    *str++ = ',';
                *str++ = ' ';
                switch (spec.Arg[i]) {
                case InstArg__:   __builtin_unreachable();
                case InstArg_Ra:  str = FormatReg(str, inst.Ra); break;
                case InstArg_Rb:  str = FormatReg(str, inst.Rb); break;
                case InstArg_Rc:  str = FormatReg(str, inst.Rc); break;
                case InstArg_C:   str = FormatImm18s(str, inst.C); break;
                case InstArg_RbC: str = FormatRegImm18s(str, inst.Rb, inst.C); break;
                }
            }
        } else {
            *str++ = '?';
        }
        *str = '\0';
    }
    return str;
}

/**
 * ParseInst parses an instruction in assembly syntax.
 *
 * On success, the parsed instruction will always be valid (i.e, it will pass
 * CheckInst).
 */
static Error ParseInst(Inst *inst, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;

    char buf[4096];
    if (!str_ecpyn(buf, str, sizeof(buf)))
        return Error_Parse_LongArgument;

    char *s_op = str_trim(buf);
    char *s_args = str_trim(str_spl(s_op, " \t"));

    Inst tmp = INST_ZERO;
    for (tmp.Opcode = 0; tmp.Opcode < 1<<5; tmp.Opcode++) {
        InstSpec spec = LookupOpcode(tmp.Opcode);
        if (!spec.Format)
            continue;

        if (spec.Cond) {
            bool m_op = false;
            char opcond[sizeof(spec.Op) + 2];
            char *opcond_c = str_ecpy(opcond, spec.Op);
            for (tmp.C2 = 0; tmp.C2 < CondCount; tmp.C2++) {
                const char *cond = GetCond(tmp.C2);
                if (!cond)
                    continue;
                str_ecpy(opcond_c, cond);
                if (str_eq(s_op, opcond, true)) {
                    m_op = true;
                    break;
                }
            }
            if (!m_op) {
                *opcond_c = '\0';
                if (str_eq(s_op, opcond, true))
                    return Error_Parse_Op_MissingCond;
                continue;
            }
        } else if (!str_eq(s_op, spec.Op, true)) {
            continue;
        }

        char *s_arg_next = s_args;
        for (size_t i = 0; i < sizeof(spec.Arg)/sizeof(*spec.Arg) && spec.Arg[i]; i++) {
            char *s_arg_cur = s_arg_next;
            s_arg_next = str_trim(str_spl(s_arg_cur, ","));

            if (!s_arg_cur || !*s_arg_cur)
                return Error_Parse_OpArgs_NotEnough;

            Error err;
            switch (spec.Arg[i]) {
            case InstArg__:   __builtin_unreachable();
            case InstArg_Ra:  err = ParseReg(&tmp.Ra, s_arg_cur); break;
            case InstArg_Rb:  err = ParseReg(&tmp.Rb, s_arg_cur); break;
            case InstArg_Rc:  err = ParseReg(&tmp.Rc, s_arg_cur); break;
            case InstArg_C:   err = ParseImm18s(&tmp.C, s_arg_cur); break;
            case InstArg_RbC: err = ParseRegImm18s(&tmp.Rb, &tmp.C, s_arg_cur); break;
            }
            if (err) {
                return err;
            }
        }
        if (s_arg_next && *s_arg_next)
            return Error_Parse_OpArgs_TooMany;

        if (inst)
            *inst = tmp;
        return NoError;
    }

    return Error_Parse_Op_Unknown;
}

/**
 * Explains the binary encoding of an instruction in a human-readable manner.
 */
static char *ExplainInst(char *str, Inst i) {
    if (str) {
        InstSpec spec = LookupOpcode(i.Opcode);

        int bits = 32;
        str = u32be_tobin(str_ecpy(str, "Op:"), i.Opcode, 5); bits -= 5;
        switch (spec.Format) {
        case InstEnc_R:
            str = u32be_tobin(str_ecpy(str, "|Ra:"), i.Ra, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|Rb:"), i.Rb, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|Rc:"), i.Rc, 4); bits -= 4;
            break;
        case InstEnc_I:
            str = u32be_tobin(str_ecpy(str, "|Ra:"), i.Ra, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|Rb:"), i.Rb, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|C:"),  i.C, 18); bits -= 18;
            break;
        case InstEnc_B:
            str = u32be_tobin(str_ecpy(str, "|Ra:"), i.Ra, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|C2:"), i.C2, 4); bits -= 4;
            str = u32be_tobin(str_ecpy(str, "|C:"),  i.C, 18); bits -= 18;
            break;
        case InstEnc_J:
            str = u32be_tobin(str_ecpy(str, "|Ra:"), i.Ra, 4); bits -= 4;
            break;
        case InstEnc_M:
            break;
        }
        if (--bits) {
            str = str_ecpy(str, "|Unk:");
            while (bits--) *str++ = '?';
        }

        *str++ = '\n';
        *str++ = spec.Format ? spec.Format : '?';
        if (spec.Format)
            str = str_ecpy(str_ecpy(str, " Op="), spec.Op);
        switch (spec.Format) {
        case InstEnc_R:
            str = FormatReg(str_ecpy(str, " Ra="), i.Ra);
            str = FormatReg(str_ecpy(str, " Rb="), i.Rb);
            str = FormatReg(str_ecpy(str, " Rc="), i.Rc);
            break;
        case InstEnc_I:
            str = FormatReg(str_ecpy(str, " Ra="), i.Ra);
            str = FormatReg(str_ecpy(str, " Rb="), i.Rb);
            str = FormatImm18s(str_ecpy(str, " C="), i.C);
            break;
        case InstEnc_B:
            str = FormatReg(str_ecpy(str, " Ra="), i.Ra);
            str = FormatCond(str_ecpy(str, " C2="), i.C2);
            str = FormatImm18s(str_ecpy(str, " C="), i.C);
            break;
        case InstEnc_J:
            str = FormatReg(str_ecpy(str, " Ra="), i.Ra);
            break;
        case InstEnc_M:
            break;
        }

        *str = '\0';
    }
    return str;
}

/**
 * High-level wrapper to disassemble and check a hex instruction.
 *
 * The input and output buffers may overlap. The input buffer must contain
 * exactly 8 hex digits, otherwise Error_Disassemble_Hex will be returned. The
 * output buffer is not bounds-checked, and must be long enough to hold all
 * possible disassembled instructions (allocate at _least_ 10 chars for opcode,
 * plus 2 for the condition code, 4 spaces, 2 commas, 3*(1 sign + 6 digits + 2
 * parenthesis + 3 register) characters for 18-bit signed immediates with
 * registers, and a null terminator).
 *
 * Note that the output buffer will _always_ be set to either an empty string or
 * a best-effort disassembly of the instruction, even if an error occurs. If no
 * error is returned, the disassembled instruction is in canonical form (and
 * thus should round-trip though another assembly/disassembly and have the same
 * result).
 */
static Error Disassemble(char *asmb, const char *hex) {
    uint32_t b;
    if (!u32be_fromhex(&b, hex)) {
        if (asmb)
            *asmb = '\0';
        return Error_Disassemble_Hex;
    }

    Inst i = DecodeInst(b);
    FormatInst(asmb, i);
    return CheckInst(i);
}

/**
 * High-level wrapper to assemble an instruction to hex.
 *
 * The input and output buffers may overlap. The output buffer must be 9 bytes
 * long (8 hex, and a null terminator).
 *
 * On failure, the output buffer will be set to an empty string.
 */
static Error Assemble(char *hex, const char *asmb) {
    Inst i;
    Error e;
    if ((e = ParseInst(&i, asmb))) {
        if (hex)
            *hex = '\0';
        return e;
    }

    uint32_t b = EncodeInst(i);
    u32be_tohex(hex, b);
    return NoError;
}

/**
 * High-level wrapper to check and explain the encoding of an instruction.
 * Similar to Disassemble.
 *
 * The output buffer should be at least 64 bytes long to be safe.
 */
static Error Explain(char *exp, const char *hex) {
    uint32_t b;
    if (!u32be_fromhex(&b, hex)) {
        if (exp)
            *exp = '\0';
        return Error_Disassemble_Hex;
    }

    Inst i = DecodeInst(b);
    ExplainInst(exp, i);
    return CheckInst(i);
}

#if defined(__wasm__)
#define export __attribute__((visibility("default")))

// https://webassembly.org/roadmap/
// https://clang.llvm.org/doxygen/Basic_2Targets_2WebAssembly_8cpp_source.html

export char buf[512];

export size_t bufsz(void) {
    return sizeof(buf);
}

export size_t buflen(void) {
    return str_len(buf);
}

export void error(Error err) {
    str_ecpy(buf, GetError(err));
}

export Error disassemble(void) {
    return Disassemble(buf, buf);
}

export Error assemble(void) {
    return Assemble(buf, buf);
}

export Error explain(void) {
    return Explain(buf, buf);
}

#elif !defined(TESTS)
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static bool is_interactive(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdin));
#else
    return isatty(STDIN_FILENO);
#endif
}

/**
 * This command reads lines of either 8-digit hex instructions to disassemble,
 * or assembly code to assemble.
 *
 * If not running interactively, the original input will be echoed back if an
 * error occurs during assembly/disassembly.
 */
int main(void) {
    char buf[4096];
    bool interactive = is_interactive();
    if (interactive)
        fprintf(stderr, "enter an instruction (8-digit hex) to disassemble, or anything else to assemble\n");
    while (fgets(buf, sizeof(buf), stdin)) {
        char *s = str_trim(buf);
        if (str_len(s) == 8) {
            char asmb[256];
            Error e = Disassemble(asmb, s);
            if (e != Error_Disassemble_Hex) {
                if (e) {
                    if (!interactive)
                        fprintf(stdout, "%s [%s]\n", s, asmb);
                    fprintf(stderr, "invalid instruction %s [%s]: %s\n", s, asmb, GetError(e));
                } else {
                    fprintf(stdout, "%s\n", asmb);
                }
                fflush(stdout);
                continue;
            }
        }
        char hb[16];
        Error e = Assemble(hb, s);
        if (e) {
            if (!interactive)
                fprintf(stdout, "%s\n", s);
            fprintf(stderr, "invalid instruction '%s': %s\n", s, GetError(e));
        } else {
            fprintf(stdout, "%s\n", hb);
        }
        fflush(stdout);
    }
    return !feof(stdin);
}

#else
#include <stdio.h>
#include <time.h>

int main(void) {
    fprintf(stderr, "> testing assembly\n");
    const char *asmtests[][2] = {
        {"28918000", "and r1, r2, r3"},
        {"30918000", "or r1, r2, r3"},
        {"18228000", "add r0, r4, r5"},
        {"20228000", "sub r0, r4, r5"},
        {"7b380000", "mul r6, r7"},
        {"83380000", "div r6, r7"},
        {"389a8000", "shr r1, r3, r5"},
        {"409a8000", "shra r1, r3, r5"},
        {"489a8000", "shl r1, r3, r5"},
        {"53320000", "ror r6, r6, r4"},
        {"5b320000", "rol r6, r6, r4"},
        {"88080000", "neg r0, r1"},
        {"90080000", "not r0, r1"},
        {"9900270F", "brzr r2, 9999"},
        {"9900270F", "brzr r2, +9999"},
        {NULL, "brzr r2, $+9999"},
        {NULL, "brzr r2, $+270f"},
        {"9900270F", "brzr r2, $270f"},
        {"9900270F", "brzr r2, $270F"},
        {"9900270F", "brzr r2, 0x270F"},
        {"9903F6D7", "brzr r2, -2345"},
        {NULL, "brzr r2, $-2345"},
        {"9903FFFF", "brzr r2, 0x3FFFF"},
        {"9903FFFF", "brzr r2, $3FFFF"},
        {NULL, "brzr r2, $0x3FFFF"},
        {"00000000", "ld r0, 0"},
        {"08080000", "ldi r0, 0(r1)"},
        {"08080000", "ldi r0, $0(r1)"},
        {"08080000", "ldi r0, $0000(r1)"},
        {"08080015", "ldi r0, 0b010101(r1)"},
        {"08080039", "ldi r0, 0o71(r1)"},
        {"08080039", "ldi r0, 0x39(r1)"},
        {"08080039", "ldi r0, $39(r1)"},
        {"08080039", "ldi r0, $000000039(r1)"},
        {NULL, "ldi r0, r0"},
        {NULL, "ldi r0, 0(r0)"},
        {NULL, "brzr r2, +0x3FFFF"},
        {NULL, "brzr r2, -0x3FFFF"},
        {NULL, "br r2, 0"},
        {NULL, "sdf r2, 0"},
        {NULL, "add r1, r2, r23"},
    };
    for (size_t x = 0; x < sizeof(asmtests)/sizeof(*asmtests); x++) {
        fprintf(stderr, ". %s %s\n", asmtests[x][0] ? asmtests[x][0] : "--------", asmtests[x][1]);

        Inst i;
        Error e = ParseInst(&i, asmtests[x][1]);
        if (!asmtests[x][0]) {
            if (!e)
                return printf("[%s] expected parse error, got none\n", asmtests[x][1]), 1;
            continue;
        } else if (e) {
            return printf("[%s] unexpected parse error %s\n", asmtests[x][1], GetError(e)), 1;
        }

        uint32_t ne;
        if (!u32be_fromhex(&ne, asmtests[x][0]))
            return printf("[%s] failed to parse test hex %s\n", asmtests[x][1], asmtests[x][0]), 1;

        uint32_t na = EncodeInst(i);
        if (ne != na) {
            char h[9];
            u32be_tohex(h, na);
            return printf("[%s] incorrect instruction encoding %s (expected %s)\n", asmtests[x][1], h, asmtests[x][0]), 1;
        }
    }

    fprintf(stderr, "> testing instruction encode/decode/parse/format consistency\n");
    time_t ts = time(NULL);
    time_t tx = ts;
    for (uint32_t tcn = 0, n1 = 0; n1 < UINT32_MAX; n1++) {
        char h[9];
        u32be_tohex(h, n1);

        if (n1%10000 == 0) {
            time_t tc = time(NULL);
            if (tc - tx > 5) {
                fprintf(stderr, ". %s %.0f%% (%d/sec)\n", h,
                    (double)(n1+1)/(double)(UINT32_MAX)*100,
                    (int)((tcn-n1)/(tc - tx)));
                tcn = n1;
                tx = tc;
            }
        }

        Inst i_d = DecodeInst(n1);

        uint32_t i_de = EncodeInst(i_d);

        char h_de[9];
        u32be_tohex(h_de, i_de);

        // decode/encode should only lose don't care bytes (i.e., shouldn't set any new ones)
        if (i_de &~ (i_de&n1))
            return printf("%s [# !~ de] %s != %s\n", h, h, h_de), 1;

        Inst i_ded = DecodeInst(n1);

        Error e_ded = CheckInst(i_ded);

        uint32_t i_dede = EncodeInst(i_ded);

        char h_dede[9];
        u32be_tohex(h_dede, i_dede);

        // encode/decode/encode should round-trip
        if (i_de != i_dede)
            return printf("%s [de !~ dede] %s != %s\n", h, h_de, h_dede), 1;

        char i_df[512] = {0};
        FormatInst(i_df, i_d);

        // format should never return an empty string
        if (!*i_df)
            return printf("%s [!fmt(df)]\n", h), 1;

        Inst i_dfp;
        Error e_dfp = ParseInst(&i_dfp, i_df);

        // format should have returned an invalid string (? isn't valid anywhere) if instruction was invalid/unknown
        if (e_ded && !e_dfp)
            return printf("%s [!valid(d) && !error(dfp)] %s\n", h, i_df), 1;

        // if the instruction was invalid/unknown, we're done with it
        if (e_ded)
            continue;

        char i_dfpf[512] = {0};
        FormatInst(i_dfpf, i_dfp);

        // format should never return an empty string
        if (!*i_dfpf)
            return printf("%s [!fmt(df)]\n", h), 1;

        // format/parse/format should round-trip
        if (!str_eq(i_df, i_dfpf, false))
            return printf("%s [df != dfpf] %s != %s", h,i_df, i_dfpf);

        uint32_t i_dfpe = EncodeInst(i_dfp);

        char h_dfpe[9];
        u32be_tohex(h_dfpe, i_dfpe);

        // decode/format/parse/encode should only lose don'Ft care bytes (i.e., shouldn't set any new ones)
        if (i_dfpe &~ (i_dfpe&n1))
            return printf("%s [# !~ de] %s != %s\n", h, h, h_dfpe), 1;

        Inst i_dfped = DecodeInst(i_dfpe);

        Error e_dfped = CheckInst(i_ded);

        // valid instruction should be effectively equal, and thus remain valid
        if (e_dfped)
            return printf("%s [valid(d) && !valid(dfped)]\n", h), 1;

        char i_dfpedf[512] = {0};
        FormatInst(i_dfpedf, i_dfped);

        // format should never return an empty string
        if (!*i_df)
            return printf("%s [!fmt(df)]\n", h), 1;

        uint32_t i_dfpede = EncodeInst(i_dfped);

        char h_dfpedfe[9];
        u32be_tohex(h_dfpedfe, i_dfpede);

        // decode/format/parse/encode (i.e., inst without don't care bits) should roundtrip with decode/encode
        if (i_dfpe != i_dfpede)
            return printf("%s [dfpe != dfpede]\n", h), 1;
    }
    return 0;
}

#endif
