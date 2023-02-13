// Command asmstream disassembles and assembles single W23 ELEC374 CPU
// instructions.
package main

import (
	"bufio"
	"encoding/hex"
	"fmt"
	"os"
	"strings"

	"github.com/pgaskin/W23-ELEC374-CPU/asm374"
)

func main() {
	sc := bufio.NewScanner(os.Stdin)
	for sc.Scan() {
		if s := strings.TrimSpace(sc.Text()); s != "" {
			if len(s) == 8 {
				if b, err := hex.DecodeString(s); err == nil {
					if s, err := asm374.Disassemble([4]byte(b)); err == nil {
						fmt.Fprintf(os.Stdout, "%s\n", s)
					} else {
						os.Stdout.Write(sc.Bytes())
						fmt.Fprintf(os.Stderr, " ; error: disassemble %08X: %v\n", b, err)
						os.Stdout.Write([]byte{'\n'})
					}
					continue
				}
			}
			if b, err := asm374.Assemble(s); err == nil {
				fmt.Fprintf(os.Stdout, "%08X\n", b[:])
			} else {
				os.Stdout.Write(sc.Bytes())
				fmt.Fprintf(os.Stderr, " ; error: assemble %q: %v\n", s, err)
				os.Stdout.Write([]byte{'\n'})
			}
			continue
		}
		os.Stdout.Write(sc.Bytes())
		os.Stdout.Write([]byte{'\n'})
	}
}
