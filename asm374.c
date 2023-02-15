// clang -Oz -Wall -Wextra -nostdlib -mbulk-memory -Wl,--no-entry -Wl,--export-dynamic -o asm374.wasm --target=wasm32 asm374.c
// clang -Wall -Wextra -o asm374 asm374.c

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * chr_tolower converts c to ASCII lowercase.
 */
static char chr_tolower(char c) {
    if ('a' <= c && c <= 'z')
        c -= 'a' - 'A';
    return c;
}

/**
 * chr_isspace checks if c is ASCII whitespace or CR.
 */
static bool chr_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/**
 * str_len returns the length of s.
 */
static size_t str_len(const char *s) {
    const char *a = s;
    while (s && *s) s++;
	return s-a;
}

/**
 * str_trim trims a string in-place.
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
 * str_spl replaces the first instance of one of the provided characters with a
 * null byte, returning a pointer to the next character, or NULL if no match was
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
 * str_eq compares a and b, optionally case-insensitively.
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
 * str_ecpy unsafely appends b onto a, returning a pointer to the new end of a.
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
 * str_ecpy copies up to n-1 bytes of b into a, sets the following to null, and
 * returns a pointer to the null, or NULL if b was too large.
 */
static char *str_ecpyn(char *a, const char *b, size_t n) {
    if (a) {
        char c;
        while (b && (c = *b++) && n-- > 1)
            *a++ = c;
        *a = '\0';
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
 * By default, an error is returned if the provided signed value is out of
 * range. If a sign is not specified, and a base is, the value is treated as an
 * unsigned value (which can include the sign bit).
 */
static Error ParseImm18s(Imm18s *imm, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;

    bool neg = false, pos = false;
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

    int base = 10;
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
        if (str_eq(GetReg(x), str, false)) {
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
    [12] = {InstEnc_I, "addi", false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [13] = {InstEnc_I, "andi", false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [14] = {InstEnc_I, "ori",  false, {InstArg_Ra,  InstArg_Rb,  InstArg_Rc}},
    [15] = {InstEnc_I, "mul",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [16] = {InstEnc_I, "div",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [17] = {InstEnc_I, "neg",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [18] = {InstEnc_I, "not",  false, {InstArg_Ra,  InstArg_Rb,  InstArg__ }},
    [19] = {InstEnc_B, "br",   true,  {InstArg_Ra,  InstArg_C,   InstArg__ }},
    [20] = {InstEnc_J, "jr",   false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [21] = {InstEnc_J, "jal",  false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [22] = {InstEnc_J, "in",   false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [23] = {InstEnc_J, "out",  false, {InstArg_Ra,  InstArg__,   InstArg__ }},
    [24] = {InstEnc_J, "mhfhi",false, {InstArg_Ra,  InstArg__,   InstArg__ }},
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
#include <stdio.h>
/**
 * DecodeInst decodes inst.
 */
static Inst DecodeInst(uint32_t b) {
    Inst i = {};
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

static Error ParseInst(Inst *inst, const char *str) {
    if (!str || !*str)
        return Error_Parse_EmptyArgument;

    char buf[4096];
    if (!str_ecpyn(buf, str, sizeof(buf)))
        return Error_Parse_LongArgument;

    char *s_op = str_trim(buf);
    char *s_args = str_trim(str_spl(s_op, " \t"));

    Inst tmp = {};
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

#define BUFFER_SIZE 4096

#ifdef __wasm__
#define export __attribute__((visibility("default")))

static Inst inst;
export char buf[BUFFER_SIZE];

static size_t error(Error err) {
    char *r = buf;
    for (const char *x = GetError(err); x && *x; x++)
        *r++ = *x;
    return r - buf;    
}

export size_t bufsz() {
    return sizeof(buf);
}

export void decode(uint32_t data) {
    inst = DecodeInst(data);
}

export uint32_t encode() {
    return EncodeInst(inst);
}

export size_t format() {
    return FormatInst(buf, inst) - buf;
}

export size_t parse(size_t len) {
    buf[len] = '\0';
    return error(ParseInst(&inst, buf));
}

export size_t check() {
    return error(CheckInst(inst));
}

#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * u4_tohex converts the top 4 bits of x to a hex digit.
 */
static char u4_tohex(uint8_t x) {
    return "0123456789ABCDEF"[x&15];
}

/**
 * u4_fromhex converts the the hex digit c to a 4-bit number, returning 0xFF on
 * error.
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
 * u32be_tohex appends 8 hex digits and a null terminator to str, big-endian.
 */
static char *u32be_tohex(char *str, uint32_t n) {
    *str++ = u4_tohex(n>>28);
    *str++ = u4_tohex(n>>24);
    *str++ = u4_tohex(n>>20);
    *str++ = u4_tohex(n>>16);
    *str++ = u4_tohex(n>>12);
    *str++ = u4_tohex(n>>8);
    *str++ = u4_tohex(n>>4);
    *str++ = u4_tohex(n>>0);
    *str++ = '\0';
    return str;
}

/**
 * u32be_fromhex converts 8 hex digits into a uint32, big-endian.
 */
static bool u32be_fromhex(uint32_t *n, const char *str) {
    uint32_t x, r = 0;
    if ((x = u4_fromhex(str[0])) != 0xFF) r |= x << 28; else return false;
    if ((x = u4_fromhex(str[1])) != 0xFF) r |= x << 24; else return false;
    if ((x = u4_fromhex(str[2])) != 0xFF) r |= x << 20; else return false;
    if ((x = u4_fromhex(str[3])) != 0xFF) r |= x << 16; else return false;
    if ((x = u4_fromhex(str[4])) != 0xFF) r |= x << 12; else return false;
    if ((x = u4_fromhex(str[5])) != 0xFF) r |= x << 8;  else return false;
    if ((x = u4_fromhex(str[6])) != 0xFF) r |= x << 4;  else return false;
    if ((x = u4_fromhex(str[7])) != 0xFF) r |= x << 0;  else return false;
    *n = r;
    return true;
}

/**
 * This command reads lines of either 8-digit hex instructions to disassemble,
 * or assembly code to assemble.
 *
 * If not running interactively, the original input will be echoed back if an
 * error occurs during assembly/disassembly.
 */
int main() {
    char buf[BUFFER_SIZE];
    char buf1[BUFFER_SIZE];
    bool interactive = isatty(STDIN_FILENO);
    if (interactive)
        fprintf(stderr, "enter an instruction (8-digit hex) to disassemble, or anything else to assemble\n");
    while (fgets(buf, sizeof(buf), stdin)) {
        char *s = str_trim(buf);
        if (str_len(s) == 8) {
            uint32_t b;
            if (u32be_fromhex(&b, s)) {
                Inst i = DecodeInst(b);
                FormatInst(buf1, i);

                Error e = CheckInst(i);
                if (e) {
                    if (!interactive)
                        fprintf(stdout, "%s\n", s);
                    fprintf(stderr, "invalid instruction %s (%s): %s\n", s, buf1, GetError(e));
                } else {
                    fprintf(stdout, "%s\n", buf1);
                }
                fflush(stdout);
                continue;
            }
        }
        Inst i;
        Error e = ParseInst(&i, s);
        if (e) {
            if (!interactive)
                fprintf(stdout, "%s\n", s);
            fprintf(stderr, "invalid instruction '%s': %s\n", s, GetError(e));
        } else {
            u32be_tohex(buf1, EncodeInst(i));
            fprintf(stdout, "%s\n", buf1);
        }
        fflush(stdout);
    }
    return !feof(stdin);
}
#endif
