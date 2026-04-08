import { generateHpackHeaderTableCodeH, generateHpackHuffmanCodeC, generateHpackTestCasesCPP } from "./hpack.js";
import { assert, testBitarray } from "./utils.js";
import { generateVariantCodeC } from "./variants/index.js";


export async function generateFile(options: GenerateOptions): Promise<void> {

    testBitarray();


    if (options.type === "c_hpack_huffman") {

        await generateHpackHuffmanCodeC(options.output)
        return;
    } else if (options.type === "c_header_table") {
        await generateHpackHeaderTableCodeH(options.output)
        return;
    } else if (options.type === "cpp_tests") {

        await generateHpackTestCasesCPP(options.output)
        return;
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (options.type === "c_variants") {
        assert(!!options.input, "expected input")
        await generateVariantCodeC(options.output, options.input)
        return;
    }


    throw new Error(`Unrecognized type: ${options.type as string}`)

}

export type GenerateType = "cpp_tests" | "c_hpack_huffman" | "c_header_table" | 'c_variants'

export interface GenerateOptions {
    output: string,
    type: GenerateType
    input: string
}

