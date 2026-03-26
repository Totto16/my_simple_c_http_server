import { subcommandGenerator, subcommandWsTests } from "./src/subcommands.js"
import url from 'node:url';
import path from 'node:path';
import fs from 'node:fs';


function isCallingThisScript(value: string): boolean {

    try {

        const actualPath: string = path.resolve(value)

        const package_json = path.join(actualPath, "package.json")

        if (!fs.existsSync(package_json)) {
            return false;
        }

        const __filename = url.fileURLToPath(import.meta.url);

        const ext = path.extname(__filename);

        const __dirname = path.dirname(__filename);

        let thisPkgRoot = __dirname;

        if (ext === '.js') {
            thisPkgRoot = path.join(__dirname, "..")
        } else if (ext === ".ts") {
            thisPkgRoot = __dirname;
        } else {
            throw new Error(`Invalid extension: ${ext}`)
        }

        const this_package_json = path.join(thisPkgRoot, "package.json")

        if (!fs.existsSync(this_package_json)) {
            throw new Error(`Invalid local package detection`)
        }

        return this_package_json == package_json;


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

        if (isCallingThisScript(value)) {
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
