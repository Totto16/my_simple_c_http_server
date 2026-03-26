import fs from "node:fs"
import path from "node:path"
import fsAsync from "node:fs/promises"

export class BitArray {
    private _size: number
    private bytes: Uint8ClampedArray

    constructor(size: number) {
        this._size = size;
        this.bytes = new Uint8ClampedArray(Math.ceil(size / 8));
    }

    set(index: number, value: boolean): void {
        if (index >= this._size || index < 0) {
            throw new Error("Out of bounds set")
        }

        const byteIndex = index >> 3;
        const bitIndex = 7 - (index & 7);

        if (value) {
            this.bytes[byteIndex]! |= (1 << bitIndex);
        } else {
            this.bytes[byteIndex]! &= ~(1 << bitIndex);
        }
    }

    get(index: number): boolean {
        if (index >= this._size || index < 0) {
            throw new Error("Out of bounds get")
        }

        const byteIndex = index >> 3;
        const bitIndex = 7 - (index & 7);


        return ((this.bytes[byteIndex]! >> bitIndex) & 0x1) != 0;
    }

    get size(): number {
        return this._size;
    }

    toNumberArray(): number[] {

        return Array.from(this.bytes)
    }
}

export function num_array_is_eq(arr1: number[], arr2: number[]): boolean {

    if (arr1.length != arr2.length) {
        return false;
    }

    for (let i = 0; i < arr1.length; ++i) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }


    return true;

}


export function test_bitarray() {

    const values: [number, number, number[]][] = [[0b10101011, 8, [0b10101011]], [0xFF, 8, [0xFF]], [0xA8, 8, [0xA8]], [0x56F1, 16, [0x56, 0xF1]]]


    for (const [value, value_sz, value_arr] of values) {

        const temp = new BitArray(value_sz)

        for (let i = 0; i < value_sz; ++i) {

            const val = ((value >> (value_sz - i - 1)) & 0x1) != 0;

            temp.set(i, val)
        }

        const temp_res = temp.toNumberArray();

        if (!num_array_is_eq(temp_res, value_arr)) {
            throw new Error(`The bitarray doesn't work as expected: ${temp_res} - ${value_arr}`)
        }

    }

}

export function get_bit_array_from_bits(bits: string, bit_len: number, hex_value: string): BitArray {

    const values = bits.split("|")

    if (values.length == 0) {
        throw new Error("not a valid bits string")
    }

    const size = (values.length - 1) * 8 + values.at(-1)!.length;

    if (bit_len != size) {
        throw new Error("not a valid bits string")
    }

    const hex_b = BigInt(`0x${hex_value}`)

    const hex_b2 = BigInt(`0b${values.join("")}`)

    if (hex_b != hex_b2) {
        throw new Error(`not a valid bits string: ${hex_b} != ${hex_b2}`)
    }

    const result = new BitArray(size)

    for (let i = 0; i < values.length; ++i) {
        const value = values[i]!
        if (i != values.length - 1) {
            if (value.length != 8) {
                throw new Error("not a valid bits string")
            }
        }

        for (let j = 0; j < value.length; ++j) {

            const bitStr = value[j]

            const index = i * 8 + j;

            if (bitStr == "0") {
                result.set(index, false)
            } else if (bitStr == "1") {
                result.set(index, true)
            } else {
                throw new Error("not a valid bits string")
            }


        }

    }

    if (result.size > 32) {
        throw new Error(`Bits size has to be <= 32 but was: ${result.size}`);
    }

    return result;

}


export function assert(val: boolean, message: string): never | void {
    if (!val) {
        throw new Error(message)
    }

}

export async function writeFileAndDirs(file: string, content: string): Promise<void> {

    const dir = path.dirname(file)

    if (!fs.existsSync(dir)) {
        await fsAsync.mkdir(dir, { recursive: true })
    }

    await fsAsync.writeFile(file, content)

}


export function getOtherFile(inp_file: string, expected_ext: string, other_ext: string): string {


    assert(path.extname(inp_file) == expected_ext, `file has to end in "${expected_ext}"`)

    assert(other_ext.includes("."), `"${other_ext}" has to have a dot '.'`)

    const other_file = path.join(path.dirname(inp_file), path.basename(inp_file).replace(expected_ext, other_ext))

    return other_file;
}


export function is_utf8_string(text: string): boolean {

    const array = new TextEncoder().encode(text)

    return array.length != text.length;
}
