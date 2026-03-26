import { generate_hpack_headerable_code_h, generate_hpack_huffman_code_c, generate_hpack_test_cases_cpp } from "./hpack.js";
import { test_bitarray } from "./utils.js";
import { generate_variant_code_c } from "./variants.js";


export async function generateFile(options: GenerateOptions): Promise<void> {

    test_bitarray();


    if (options.type === "c_hpack_huffman") {

        return await generate_hpack_huffman_code_c(options.output)
    } else if (options.type === "c_header_table") {
        return await generate_hpack_headerable_code_h(options.output)
    } else if (options.type === "cpp_tests") {

        return await generate_hpack_test_cases_cpp(options.output)
    } else if (options.type === "c_variants") {
        return await generate_variant_code_c(options.output)
    }


    throw new Error(`Unrecognized type: ${options.type}`)

}

export type GenerateType = "cpp_tests" | "c_hpack_huffman" | "c_header_table" | 'c_variants'

export interface GenerateOptions {
    output: string,
    type: GenerateType
}

