import { subcommandGenerator, subcommandWsTests } from "./src/subcommands.js"


type SubCommand = "ws_tests" | "generator"

async function main(): Promise<void> {

    let subcommand: SubCommand | null = null
    const args: string[] = []

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

        if (subcommand === null) {
            switch (value) {
                case "ws_tests": {
                    subcommand = "ws_tests"
                    break;
                }
                case "generator": {
                    subcommand = "generator"
                    break;
                }
                default: {
                    throw new Error(`Invalid subcommand: ${value}`)
                }
            }

        } else {
            args.push(value)
        }

    }

    switch (subcommand) {
        case "generator": {
            return await subcommandGenerator(args);
        }
        case "ws_tests": {
            return await subcommandWsTests(args);
        }
        default: {
            throw new Error(
                `No subcommand specified: ${subcommand}`
            )
        }
    }

}


main()
