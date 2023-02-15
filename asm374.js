async function InitASM374(url) {
    const res = await fetch(url)
    const buf = await res.arrayBuffer()
    const obj = await WebAssembly.instantiate(buf, {})

    const uint32 = n => n >>> 0

    const exec_val = (fn, ...a) => obj.instance.exports[fn](...a)
    const exec_str = (fn, ...a) => get_str(uint32(exec_val(fn, ...a)))
    const exec_exc = (fn, ...a) => { const err = exec_str(fn, ...a); if (err.length) throw new Error(err) }

    const get_mem = len => new Uint8Array(obj.instance.exports.memory.buffer, obj.instance.exports.buf, len)
    const get_str = len => new TextDecoder().decode(get_mem(len))

    const put_mem = arr => { const sz = uint32(exec_val("bufsz")); if (arr.length >= sz) throw new TypeError(`Data of length ${arr.length} is longer than maximum ${sz} bytes`); get_mem(arr.length).set(arr); return arr.length }
    const put_str = str => put_mem(new TextEncoder().encode(str))

    return Object.freeze({
        assemble(s) {
            if (typeof s !== "string")
                throw new TypeError("Instruction to assemble must be a string")
            exec_exc("parse", put_str(s))
            return uint32(exec_val("encode")).toString(16).padStart(8, "0").toUpperCase()
        },
        disassemble(b) {
            if (typeof b !== "string" || !/^[a-fA-F0-9]{8}$/.test(b))
                throw new TypeError("Instruction to disassemble must be 8 hex digits")
            exec_val("decode", uint32(parseInt(b, 16)))
            return exec_str("format")
        },
        check(b) {
            if (typeof b !== "string" || !/^[a-fA-F0-9]{8}$/.test(b))
                throw new TypeError("Instruction to disassemble must be 8 hex digits")
            exec_val("decode", uint32(parseInt(b, 16)))
            const r = exec_str("check")
            return r?.length ? r : null;
        },
    })
}
