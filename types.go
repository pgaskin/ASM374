package asm374

import (
	"errors"
	"fmt"
	"strconv"
	"strings"
)

// Encodings:
//
//	R  opcode|Ra|Rb|Rc
//	R1 op Ra, Rb, Rc
//	I  opcode|Ra|Rb|C
//	I1 op Ra, C {Rb=R0}
//	I1 op Ra, C(Rb)
//	I2 op C, Ra {Rb=R0}
//	I2 op C(Rb), Ra
//	I3 op Ra, Rb, C
//	I4 op Ra, Rb
//	B  opcode|Ra|C2|C
//	B1 opC2 Ra, C
//	J  opcode|Ra
//	J1 op Ra
//	M  opcodes
//	M1 op
var insts = [1 << 5]string{
	"I1 ld",
	"I1 ldi",
	"I2 st",
	"R1 add",
	"R1 sub",
	"R1 and",
	"R1 or",
	"R1 shr",
	"R1 shra",
	"R1 shl",
	"R1 ror",
	"R1 rol",
	"I3 addi",
	"I3 andi",
	"I3 ori",
	"I4 mul",
	"I4 div",
	"I4 neg",
	"I4 not",
	"B1 br", // special: brC2
	"J1 jr",
	"J1 jal",
	"J1 in",
	"J1 out",
	"J1 mhfhi",
	"J1 mflo",
	"M1 nop",
	"M1 halt",
}

type Arg interface {
	String() string
	UnmarshalArg(string) error
}

func ParseArgs(s string, a ...Arg) error {
	ss := strings.FieldsFunc(s, func(r rune) bool { return r == ',' })
	if len(ss) != len(a) {
		return fmt.Errorf("expected %d args, got %d", len(a), len(ss))
	}
	for i, v := range a {
		x := strings.TrimSpace(ss[i])
		if err := v.UnmarshalArg(x); err != nil {
			return fmt.Errorf("invalid %T arg %q: %w", v, x, err)
		}
	}
	return nil
}

type Op uint8

func (op Op) String() string {
	if d := op.data(); d != "" {
		return d[3:]
	}
	return ""
}

func (op Op) GoString() string {
	if d := op.data(); d != "" {
		return d[3:]
	}
	return "Op(" + strconv.FormatUint(uint64(op.Mask()), 10) + ")"
}

func (op Op) Encoding() string {
	if d := op.data(); d != "" {
		return d[0:2]
	}
	return ""
}

func (op Op) Valid() bool {
	return op.data() != ""
}

func (op Op) Mask() Op {
	return op & (1<<5 - 1)
}

func (op Op) data() string {
	return insts[op.Mask()]
}

type Reg uint8

func (i *Reg) UnmarshalArg(s string) error {
	if s != "" && (s[0] == 'r' || s[0] == 'R') {
		if n, err := strconv.ParseUint(s[1:], 10, 4); err == nil {
			*i = Reg(n)
			return nil
		}
	}
	return errors.New("invalid register")
}

func (r Reg) Index() int {
	return int(r.Mask())
}

func (r Reg) String() string {
	return "r" + strconv.FormatUint(uint64(r.Mask()), 10)
}

func (r Reg) GoString() string {
	return "Reg(" + strconv.FormatUint(uint64(r.Mask()), 10) + ")"
}

func (r Reg) Mask() Reg {
	return r & (1<<4 - 1)
}

type Cond uint8

func (c Cond) String() string {
	return [1 << 2]string{
		"zr",
		"nz",
		"pl",
		"mi",
	}[c.Mask()]
}

func (c Cond) GoString() string {
	return "Cond(" + strconv.FormatUint(uint64(c.Mask()), 10) + ")"
}

func (c Cond) Mask() Cond {
	return c & (1<<2 - 1)
}

type Imm18s uint32

func (i *Imm18s) UnmarshalArg(s string) error {
	n, err := strconv.ParseInt(s, 0, 18)
	if err != nil {
		return err
	}
	*i = Imm18s(int32(n)).Mask()
	return nil
}

func (i Imm18s) Int32() int32 {
	n := i.Mask()           // mask to width
	if n&(1<<(18-1)) != 0 { // if sign bit set
		n = n - (1 << 18) // 2s complement
	}
	return int32(n) // as 2s complement
}

func (i Imm18s) String() string {
	return strconv.FormatInt(int64(i.Int32()), 10)
}

func (i Imm18s) Mask() Imm18s {
	return i & (1<<18 - 1)
}

type RegImm18s struct {
	Reg    Reg
	Imm18s Imm18s
}

func (i *RegImm18s) UnmarshalArg(s string) error {
	var tmp RegImm18s
	if s1, ok := strings.CutSuffix(s, ")"); ok {
		if s1a, s1b, ok := strings.Cut(s1, "("); ok {
			if err := tmp.Imm18s.UnmarshalArg(s1a); err != nil {
				return err
			}
			if err := tmp.Reg.UnmarshalArg(s1b); err != nil {
				return err
			}
			if tmp.Reg == 0 {
				return fmt.Errorf("register must not be %s", tmp.Reg)
			}
			*i = tmp
			return nil
		}
		return errors.New("invalid argument")
	} else {
		if err := tmp.Imm18s.UnmarshalArg(s); err != nil {
			return err
		}
		*i = tmp
		return nil
	}
}

func (v RegImm18s) String() string {
	if v.Reg == 0 {
		return v.Imm18s.String()
	}
	return v.Imm18s.String() + "(" + v.Reg.String() + ")"
}
