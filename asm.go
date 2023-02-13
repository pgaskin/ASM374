// Package asm374 manipulates W23 ELEC374 CPU instructions.
package asm374

import (
	"encoding/binary"
	"fmt"
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
	for op := Op(0); op < (1<<5 - 1); op++ {
		if op.Valid() && op == inst_op {
			switch op.Encoding() {
			case "R1": // add sub and or shr shra shl ror rol
				return fmt.Sprintf("%s %s, %s, %s", inst_op, inst_ra, inst_rb, inst_rc), nil
			case "I1": // ld ldi
				if inst_rb == 0 {
					return fmt.Sprintf("%s %s, %s", inst_op, inst_ra, inst_c), nil
				} else {
					return fmt.Sprintf("%s %s, %s(%s)", inst_op, inst_ra, inst_c, inst_rb), nil
				}
			case "I2": // st
				if inst_rb == 0 {
					return fmt.Sprintf("%s %s, %s", inst_op, inst_c, inst_ra), nil
				} else {
					return fmt.Sprintf("%s %s(%s), %s", inst_op, inst_c, inst_rb, inst_ra), nil
				}
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
			}
			panic("invalid op info")
		}
	}
	return "", fmt.Errorf("unrecognize opcode %05b (%d)", inst_op, inst_op)
}

func Assemble(s string) ([4]byte, error) {
	return [4]byte{}, fmt.Errorf("not implemented")
}
