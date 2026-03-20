import path from "node:path"
import { generateTestFiles } from "./src/compression"
import { runWsTests } from "./src/ws_tests"

type GenerateType = "cpp_tests" | "run_ws_tests"


interface GenerateOptions {
    output: string,
    type: GenerateType
    jobs: number
}

async function main(): Promise<void> {
    const options: Partial<GenerateOptions> = {
        jobs: 0
    }

    for (let i = 0; i < process.argv.length; ++i) {
        const value = process.argv[i]!

        if (value.endsWith('deno') || value.endsWith('node') || value.endsWith('bun')) {
            continue
        }

        if (value.endsWith('.js')) {
            continue
        }

        if (value.endsWith('.ts')) {
            continue
        }

        if (value == '-o' || value == '--output') {
            if (i + 1 >= process.argv.length) {
                throw new Error(
                    `Expected another argument for the output argument`
                )
            }


            const output = path.resolve(process.argv[i + 1]!)

            options.output = output
            ++i
            continue
        }

        if (value == '-j' || value == '--jobs') {
            if (i + 1 >= process.argv.length) {
                throw new Error(
                    `Expected another argument for the jobs argument`
                )
            }

            const jobRaw = process.argv[i + 1]!

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

        if (value == '-t' || value == '--type') {
            if (i + 1 >= process.argv.length) {
                throw new Error(
                    `Expected another argument for the type argument`
                )
            }

            const typeRaw = process.argv[i + 1]!

            if (typeRaw !== "run_ws_tests" && typeRaw !== "cpp_tests") {
                throw new Error(
                    `Invalid type: ${typeRaw}`
                )
            }

            options.type = typeRaw

            ++i
            continue
        }

        if (value == '--ignore-after') {
            break
        }

        throw new Error(`Unrecognized argument: ${value}`)
    }

    if (!options.type) {
        throw new Error(`No type given`)
    }

    switch (options.type) {
        case "cpp_tests": {
            if (!options.output) {
                throw new Error(`No output given`)
            }
            return generateTestFiles(options.output)
        }
        case "run_ws_tests": {
            return await runWsTests(options.jobs ?? 0)
        }
        default: {
            throw new Error(
                `Invalid type: ${options.type}`
            )
        }
    }

}


main()
