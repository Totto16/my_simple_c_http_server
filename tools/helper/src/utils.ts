import fs from "node:fs"
import path from "node:path"
import fsAsync from "node:fs/promises"
import url from 'node:url';

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
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            this.bytes[byteIndex]! |= (1 << bitIndex);
        } else {
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            this.bytes[byteIndex]! &= ~(1 << bitIndex);
        }
    }

    get(index: number): boolean {
        if (index >= this._size || index < 0) {
            throw new Error("Out of bounds get")
        }

        const byteIndex = index >> 3;
        const bitIndex = 7 - (index & 7);


        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        return ((this.bytes[byteIndex]! >> bitIndex) & 0x1) != 0;
    }

    get size(): number {
        return this._size;
    }

    toNumberArray(): number[] {

        return Array.from(this.bytes)
    }
}

export function numberArrayIsEq(arr1: number[], arr2: number[]): boolean {

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


export function testBitarray(): void {

    const values: [number, number, number[]][] = [[0b10101011, 8, [0b10101011]], [0xFF, 8, [0xFF]], [0xA8, 8, [0xA8]], [0x56F1, 16, [0x56, 0xF1]]]


    for (const [value, valueSize, valueArray] of values) {

        const temp = new BitArray(valueSize)

        for (let i = 0; i < valueSize; ++i) {

            const val = ((value >> (valueSize - i - 1)) & 0x1) != 0;

            temp.set(i, val)
        }

        const tempResult = temp.toNumberArray();

        if (!numberArrayIsEq(tempResult, valueArray)) {
            throw new Error(`The bitarray doesn't work as expected: ${tempResult.join(", ")} - ${valueArray.join(", ")}`)
        }

    }

}

export function getBitArrayFromBits(bits: string, bitLen: number, hexValue: string): BitArray {

    const values = bits.split("|")

    if (values.length == 0) {
        throw new Error("not a valid bits string")
    }

    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
    const size = (values.length - 1) * 8 + values.at(-1)!.length;

    if (bitLen != size) {
        throw new Error("not a valid bits string")
    }

    const hexByte1 = BigInt(`0x${hexValue}`)

    const hexByte2 = BigInt(`0b${values.join("")}`)

    if (hexByte1 != hexByte2) {
        throw new Error(`not a valid bits string: ${hexByte1.toString()} != ${hexByte2.toString()}`)
    }

    const result = new BitArray(size)

    for (let i = 0; i < values.length; ++i) {
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
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
        throw new Error(`Bits size has to be <= 32 but was: ${result.size.toString()}`);
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


export function getOtherFile(inputFile: string, expectedExtension: string, otherExtension: string): string {


    assert(path.extname(inputFile) == expectedExtension, `file has to end in "${expectedExtension}"`)

    assert(otherExtension.includes("."), `"${otherExtension}" has to have a dot '.'`)

    const otherFile = path.join(path.dirname(inputFile), path.basename(inputFile).replace(expectedExtension, otherExtension))

    return otherFile;
}


export function isUTF8String(text: string): boolean {

    const array = new TextEncoder().encode(text)

    return array.length != text.length;
}

export function getThisPackageFile(): string {
    const __filename = url.fileURLToPath(import.meta.url);

    const ext = path.extname(__filename);

    const __dirname = path.dirname(__filename);

    let thisPkgRoot: string;

    if (ext === '.js') {
        thisPkgRoot = path.join(__dirname, "..", "..")
    } else if (ext === ".ts") {
        thisPkgRoot = path.join(__dirname, "..");
    } else {
        throw new Error(`Invalid extension: ${ext}`)
    }

    const thisPackageJson = path.join(thisPkgRoot, "package.json")

    if (!fs.existsSync(thisPackageJson)) {
        throw new Error(`Invalid local package detection`)
    }


    return thisPackageJson
}


function getAllFilesInDir(dir: string): string[] {
    if (!path.isAbsolute(dir)) {
        throw new Error(`Got non absolute dir: ${dir}`)
    }

    const st = fs.statSync(dir, { throwIfNoEntry: true })

    if (!st.isDirectory()) {
        throw new Error(`Not a dir: ${dir}`)
    }

    const result: string[] = []

    const dirHandle = fs.opendirSync(dir, { recursive: true })


    // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    while (true) {
        const dirEnt = dirHandle.readSync();

        if (dirEnt === null) {
            break;
        }

        if (dirEnt.isDirectory()) {
            continue;
        }

        result.push(path.join(dirEnt.parentPath, dirEnt.name))
    }

    dirHandle.closeSync()

    return result;

}

function getSourceFiles(): string[] {
    const packageFile = getThisPackageFile();

    const rootDir = path.dirname(packageFile)

    const tsconfigFile = path.join(rootDir, "tsconfig.json");

    if (!fs.existsSync(tsconfigFile)) {
        throw new Error(`Invalid tsconfig detection`)
    }

    const tsconfigContent = fs.readFileSync(tsconfigFile).toString();

    const tsconfig: Record<string, unknown> = JSON.parse(tsconfigContent) as Record<string, unknown>

    const outDir: unknown = (tsconfig.compilerOptions as Record<string, unknown>).outDir

    if (typeof outDir !== "string") {
        throw new Error(`Invalid tsconfig: missing outdir`)
    }

    const outDirAbs = path.join(rootDir, outDir)

    const result: string[] = []

    result.push(...getAllFilesInDir(outDirAbs))

    result.push(path.join(rootDir, "index.ts"))

    result.push(...getAllFilesInDir(path.join(rootDir, "src")))

    return result
}

export function addGenerateMacros(description: string): string {
    const files = getSourceFiles()

    return files.map((file): string => {
        assert(path.isAbsolute(file), "file needs to be in absolute form")
        return `#pragma GCC dependency "${file}" generated script: ${description}`
    }).join("\n")

}
