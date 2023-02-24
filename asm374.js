/* JavaScipt bindings for ASM374
 * Copyright 2023 Patrick Gaskin
 * Supports most browsers from mid-2021 (Chrome/Edge/Firefox 89, Safari 15)
 */
"use strict"

class AssemblyError extends Error {
    constructor(message) {
        super(message)
        this.name = "AssemblyError"
    }
}

export function assemble(s) {
    native.str = s
    const res = native.assemble()
    if (res) {
        native.error(res)
        throw new AssemblyError(native.str)
    }
    return native.str
}

export function disassemble(s) {
    native.str = s
    const res = native.disassemble()
    const asm = native.str
    if (res) {
        native.error(res)
        if (!asm.length) {
            throw new AssemblyError(native.str)
        }
    }
    return {asm, err: res ? native.str : null}
}

export function explain(s) {
    native.str = s
    const res = native.explain()
    const exp = native.str
    if (res) {
        native.error(res)
        if (!exp.length) {
            throw new AssemblyError(native.str)
        }
    }
    return {exp, err: res ? native.str : null}
}

const wasm = await WebAssembly.instantiateStreaming(fetch(/**/"asm374.wasm"/**/))
const native = {...wasm.instance.exports}

Object.defineProperty(native, "str", {
    get() {
        const buf = new Uint8Array(this.memory.buffer, this.buf, this.buflen())
        return new TextDecoder().decode(buf)
    },
    set(s) {
        const buf = new Uint8Array(this.memory.buffer, this.buf, this.bufsz())
        const res = new TextEncoder().encodeInto(s, buf)
        if (res.written < buf.length) {
            buf[res.written] = 0
        } else {
            buf[0] = 0
            throw new TypeError(`String exceeds maximum length ${buf.length - 1}`)
        }
    },
    enumerable: true,
})

try {
    if (disassemble(assemble("nop"))?.asm !== "nop") {
        throw new Error(`Incorrect assembly/disassembly ${JSON.stringify(res)}`)
    }
} catch (ex) {
    throw new Error(`ASM374 is broken: ${ex}`)
}
