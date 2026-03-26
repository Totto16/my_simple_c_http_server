import path from "node:path"
import { generateFile, GenerateOptions } from "./hpack.js"
import { runWsTests } from "./ws_tests.js"

export async function subcommandGenerator(args: string[]): Promise<void> {
    const options: Partial<GenerateOptions> = {
    }

    for (let i = 0; i < args.length; ++i) {
        const value = args[i]!


        if (value == '-o' || value == '--output') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the output argument`
                )
            }


            const output = path.resolve(args[i + 1]!)

            options.output = output
            ++i
            continue
        }

        if (value == '-t' || value == '--type') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the type argument`
                )
            }

            const typeRaw = args[i + 1]!

            //TODO: make generating tagged unions here too!


            if (typeRaw !== "c_hpack_huffman" && typeRaw !== "c_header_table" && typeRaw !== "cpp_tests") {
                throw new Error(
                    `Invalid type: ${typeRaw}`
                )
            }

            options.type = typeRaw

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

    return await generateFile(options as GenerateOptions)
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
        const value = args[i]!

        if (value == '-j' || value == '--jobs') {
            if (i + 1 >= args.length) {
                throw new Error(
                    `Expected another argument for the jobs argument`
                )
            }

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



    return await runWsTests(options.jobs ?? 0)

}
