// Package asm374 manipulates W23 ELEC374 CPU instructions.
package asm374

import (
	"encoding/binary"
	"fmt"
	"strings"
)

func Disassemble(x [4]byte) (string, error) {
	var (
		inst    = binary.BigEndian.Uint32(x[0:4])
		inst_op = Op((inst >> 27) & (1<<5 - 1))      // opcode
		inst_ra = Reg((inst >> 23) & (1<<4 - 1))     // register
		inst_rb = Reg((inst >> 19) & (1<<4 - 1))     // register
		inst_rc = Reg((inst >> 15) & (1<<4 - 1))     // register
		inst_c2 = Cond((inst >> 19) & (1<<2 - 1))    // condition
		inst_c  = Imm18s((inst >> 00) & (1<<18 - 1)) // signed 18-bit constant
	)
	for op := Op(0); op < 1<<5; op++ {
		if op.Valid() && op == inst_op {
			switch op.Encoding() {
			case "R1": // add sub and or shr shra shl ror rol
				return fmt.Sprintf("%s %s, %s, %s", inst_op, inst_ra, inst_rb, inst_rc), nil
			case "I1": // ld ldi
				if inst_rb == 0 {
					return fmt.Sprintf("%s %s, %s", inst_op, inst_ra, inst_c), nil
				}
				return fmt.Sprintf("%s %s, %s(%s)", inst_op, inst_ra, inst_c, inst_rb), nil
			case "I2": // st
				if inst_rb == 0 {
					return fmt.Sprintf("%s %s, %s", inst_op, inst_c, inst_ra), nil
				}
				return fmt.Sprintf("%s %s(%s), %s", inst_op, inst_c, inst_rb, inst_ra), nil
			case "I3": // addi andi ori
				return fmt.Sprintf("%s %s, %s, %s", inst_op, inst_ra, inst_rb, inst_c), nil
			case "I4": // mul div neg not
				return fmt.Sprintf("%s %s, %s", inst_op, inst_ra, inst_rb), nil
			case "B1": // br
				return fmt.Sprintf("%s%s %s, %s", inst_op, inst_c2, inst_ra, inst_c), nil
			case "J1": // jr jal in out mfhi mflo
				return fmt.Sprintf("%s %s", inst_op, inst_ra), nil
			case "M1": // nop halt
				return inst_op.String(), nil
			default:
				panic("invalid op info")
			}
		}
	}
	return "", fmt.Errorf("unknown opcode %05b (%d)", inst_op, inst_op)
}

func Assemble(s string) ([4]byte, error) {
	var (
		st               = strings.ToLower(strings.Join(strings.Fields(s), " ")) // normalize whitespace
		st_op, s_args, _ = strings.Cut(st, " ")
	)
	for inst_op := Op(0); inst_op < 1<<5; inst_op++ {
		var (
			ok       bool
			inst_c2  Cond
			inst_ra  Reg
			inst_rb  Reg
			inst_rc  Reg
			inst_c   Imm18s
			inst_rbc RegImm18s
		)
		if inst_op.Valid() {
			switch inst_op {
			case Op(0b10011): // special: br
				for inst_c2 = Cond(0); inst_c2 < 1<<2; inst_c2++ {
					if st_op == inst_op.String()+inst_c2.String() {
						ok = true
						break
					}
				}
			default:
				if st_op == inst_op.String() {
					ok = true
				}
			}
		}
		if !ok {
			continue
		}
		var err error
		var inst uint32
		switch inst_op.Encoding() {
		case "R1": // op Ra, Rb, Rc
			if err = ParseArgs(s_args, &inst_ra, &inst_rb, &inst_rc); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_rb.Mask()) << 19
				inst |= uint32(inst_rc.Mask()) << 15
			}
		case "I1": // op Ra, C    op Ra, C(Rb!=R0)
			if err = ParseArgs(s_args, &inst_ra, &inst_rbc); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_rbc.Reg.Mask()) << 19
				inst |= uint32(inst_rbc.Imm18s.Mask()) << 0
			}
		case "I2": // op C, Ra    op C(Rb!=R0), Ra
			if err = ParseArgs(s_args, &inst_rbc, &inst_ra); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_rbc.Reg.Mask()) << 19
				inst |= uint32(inst_rbc.Imm18s.Mask()) << 0
			}
		case "I3": // op Ra, Rb, C
			if err = ParseArgs(s_args, &inst_ra, &inst_rb, &inst_c); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_rb.Mask()) << 19
				inst |= uint32(inst_c.Mask()) << 0
			}
		case "I4": // op Ra, Rb
			if err = ParseArgs(s_args, &inst_ra, &inst_rb); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_rb.Mask()) << 19
			}
		case "B1": // opC2 Ra, C
			if err = ParseArgs(s_args, &inst_ra, &inst_c); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
				inst |= uint32(inst_c2.Mask()) << 19
				inst |= uint32(inst_c.Mask()) << 0
			}
		case "J1": // op Ra
			if err = ParseArgs(s_args, &inst_ra); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
				inst |= uint32(inst_ra.Mask()) << 23
			}
		case "M1": // op
			if err = ParseArgs(s_args); err == nil {
				inst |= uint32(inst_op.Mask()) << 27
			}
		default:
			panic("invalid op info")
		}
		if err != nil {
			return [4]byte{}, fmt.Errorf("op %s: %w", inst_op, err)
		}
		var b [4]byte
		binary.BigEndian.PutUint32(b[:], inst)
		return b, nil
	}
	return [4]byte{}, fmt.Errorf("invalid op %q", st_op)
}
