import path from "node:path"
import { generateFile, type GenerateOptions, type GenerateType } from "./generator.js"
import { runWsTests } from "./ws_tests.js"
import { outputVariantJsonSchema } from "./variants/data.js"

export async function subcommandGenerator(args: string[]): Promise<void> {
    const options: Partial<GenerateOptions> = {
    }

    const allTypes: GenerateType[] = ["c_hpack_huffman", "c_header_table", "cpp_tests", "c_variants"] as const

    for (let i = 0; i < args.length; ++i) {
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        const value = args[i]!

        if (value == '--variant-json-schema') {
            outputVariantJsonSchema()
            return;
        }


        if (value == '-o' || value == '--output') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the output argument`
                )
            }


            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const output = path.resolve(args[i + 1]!)

            options.output = output
            ++i
            continue
        }

        if (value == '-i' || value == '--input') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the input argument`
                )
            }


            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const input = path.resolve(args[i + 1]!)

            options.input = input
            ++i
            continue
        }

        if (value == '-t' || value == '--type') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the type argument`
                )
            }

            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const typeRaw = args[i + 1]!

            //TODO: make generating tagged unions here too!




            if (!allTypes.includes(typeRaw as GenerateType)) {
                throw new Error(
                    `Invalid type: ${typeRaw}`
                )
            }

            options.type = typeRaw as GenerateType


            ++i
            continue
        }

        throw new Error(`Unrecognized argument: ${value}`)
    }

    if (!options.type) {
        throw new Error(`No type given`)
    }

    if (!options.output) {
        throw new Error(`No output given`)
    }

    if (options.type === "c_variants" && !options.input) {
        throw new Error(`No input given`)
    }

    await generateFile(options as GenerateOptions)
}


interface WsTestOptions {
    output: string,
    jobs: number
}

export async function subcommandWsTests(args: string[]): Promise<void> {
    const options: Partial<WsTestOptions> = {
        jobs: 0
    }

    for (let i = 0; i < args.length; ++i) {
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        const value = args[i]!

        if (value == '-j' || value == '--jobs') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the jobs argument`
                )
            }

            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const jobRaw = args[i + 1]!

            const jobs = parseInt(jobRaw)

            if (isNaN(jobs)) {
                throw new Error(
                    `Invalid jobs value: ${jobRaw}`
                )
            }

            options.jobs = jobs

            ++i
            continue
        }


        throw new Error(`Unrecognized argument: ${value}`)
    }



    await runWsTests(options.jobs ?? 0)

}
