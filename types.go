package asm374

import "strconv"

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
	return "Op(" + strconv.FormatUint(uint64(op.mask()), 10) + ")"
}

func (op Op) Encoding() string {
	if d := op.data(); d != "" {
		return d[0:2]
	}
	return ""
}

func (op Op) Format() byte {
	if d := op.data(); d != "" {
		return d[0]
	}
	return 0
}

func (op Op) Variant() byte {
	if d := op.data(); d != "" {
		return d[1]
	}
	return 0
}

func (op Op) Valid() bool {
	return op.data() != ""
}

func (op Op) mask() Op {
	return op & (1<<5 - 1)
}

func (op Op) data() string {
	return insts[op.mask()]
}

type Reg uint8

func (r Reg) Index() int {
	return int(r.mask())
}

func (r Reg) String() string {
	return "r" + strconv.FormatUint(uint64(r.mask()), 10)
}

func (r Reg) GoString() string {
	return "Reg(" + strconv.FormatUint(uint64(r.mask()), 10) + ")"
}

func (r Reg) mask() Reg {
	return r & (1<<4 - 1)
}

type Cond uint8

func (c Cond) String() string {
	return [1 << 2]string{
		"zr",
		"nz",
		"pl",
		"mi",
	}[c.mask()]
}

func (c Cond) GoString() string {
	return "Cond(" + strconv.FormatUint(uint64(c.mask()), 10) + ")"
}

func (c Cond) mask() Cond {
	return c & (1<<2 - 1)
}

type Imm18s uint32

func (i Imm18s) Int32() int32 {
	const bits = 18
	n := uint32(i) & (1<<bits - 1) // mask to width
	if n&(1<<(bits-1)) != 0 {      // if sign bit set
		n = n - (1 << bits) // 2s complement
	}
	return int32(n) // as 2s complement
}

func (i Imm18s) String() string {
	return strconv.FormatInt(int64(i.Int32()), 10)
}
