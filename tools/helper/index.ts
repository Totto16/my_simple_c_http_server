import { subcommandGenerator, subcommandWsTests } from "./src/subcommands.js"
import { fsAsyncExists, getThisPackageFile } from "./src/utils.js";

import path from 'node:path';

async function isCallingThisScript(value: string): Promise<boolean> {

    try {

        const actualPath: string = path.resolve(value)

        const packageJson = path.join(actualPath, "package.json")

        if (!(await fsAsyncExists(packageJson))) {
            return false;
        }

        const thisPackageJson = await getThisPackageFile()

        return thisPackageJson == packageJson;
    } catch (_err) {
        return false;
    }
}


type SubCommand = "ws_tests" | "generator"

async function main(): Promise<void> {

    let subcommand: SubCommand | null = null
    const args: string[] = []

    for (const value of process.argv) {

        if (value.endsWith('deno') || value.endsWith('node') || value.endsWith('bun')) {
            continue
        }

        if (value.endsWith('.js')) {
            continue
        }

        if (value.endsWith('.ts')) {
            continue
        }

        if (await isCallingThisScript(value)) {
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
            await subcommandGenerator(args);
            return;
        }
        case "ws_tests": {
            await subcommandWsTests(args);
            return;
        }
        default: {
            throw new Error(
                `No subcommand specified: ${subcommand as unknown as string}`
            )
        }
    }

}


void main()
