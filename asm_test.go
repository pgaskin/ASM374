package asm374

import (
	"encoding/binary"
	"encoding/hex"
	"testing"
)

func FuzzAsmRoundTrip(f *testing.F) {
	for _, c := range []struct {
		Encoding    string
		Instruction string
	}{
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
		{"9903F6D7", "brzr r2, -2345"},
	} {
		b, err := hex.DecodeString(c.Encoding)
		if err != nil {
			panic(err)
		}
		if len(b) != 4 {
			panic("invalid instruction length")
		}
		a, err := Assemble(c.Instruction)
		if err != nil {
			f.Errorf("initial cases: failed to assemble %q: %v", c.Instruction, err)
			continue
		}
		if [4]byte(b) != a {
			f.Errorf("initial cases: incorrect encoding for %q: expected %08X, got %08X", c.Instruction, b, a)
			continue
		}
		f.Add(binary.BigEndian.Uint32(b))
	}

	f.Fuzz(func(t *testing.T, inst uint32) {
		var b [4]byte
		binary.BigEndian.PutUint32(b[:], inst)

		s, err := Disassemble(b)
		if err != nil {
			t.Fatalf("failed to disassemble %08X: %v", b, err)
		}

		a, err := Assemble(s)
		if err != nil {
			t.Fatalf("failed to assemble %q (%08X): %v", s, b, err)
		}

		// note: we can't check a == b here since there are unused bits in some
		// instructions, so assemble it again, and test it

		s2, err := Disassemble(a)
		if err != nil {
			t.Fatalf("failed to disassemble %08X: %v", b, err)
		}

		if s != s2 {
			t.Errorf("round-trip mismatch: %08X -> %q -> %08X -> %q", b, s, a, s2)
		}
	})
}

// TODO: unit tests
