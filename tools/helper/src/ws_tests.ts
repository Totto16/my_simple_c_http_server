import net from "node:net"
import os from "node:os"
import fs from "node:fs"
import fsAsync from "node:fs/promises"
import path from "node:path"
import child_process from "node:child_process";
import http from "node:http"
import https from "node:https"

import { type AllCases, all_cases } from "./all_cases.js"
import { Logger } from "./log.js"

interface WaitOptions {
    host: string,
    port: number,
    timeout: number
}

async function sleep(ms: number): Promise<void> {
    return new Promise<void>((resolve) => {
        setTimeout(resolve, ms)
    })
}

async function connectTo(host: string, port: number, timeout: number): Promise<void> {
    return new Promise<void>((resolve, reject) => {
        const socket = net.createConnection({ port, host, timeout }, () => {

            socket.write("GET / HTTP/1.1\r\n\r\n");

            socket.end()

            socket.destroy();
            resolve()
            return;
        })

        socket.on('timeout', () => {
            socket.destroy();
            reject(new Error('socket timeout'));
            return;
        });


        socket.on('error', (error) => {
            socket.destroy();
            reject(new Error(`socket error: ${error}`));
            return;
        });

    });

}

async function waitForPort(options: WaitOptions): Promise<number> {

    const start = new Date();

    function error(reason: string): void {
        throw new Error(`Failed to wait for '${options.host}:${options.port.toString()}': ${reason}`)
    }

    const finalTimeout = setTimeout((): void => {
        error("timeout");
    }, options.timeout * 1000)

    function success(): number {
        clearTimeout(finalTimeout)
        const now = new Date()
        return now.getTime() - start.getTime()
    }

    const connectionTimeout = 1000;

    // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    while (true) {
        try {
            await connectTo(options.host, options.port, connectionTimeout)
            return success();
        } catch (_err) {
            // ignore
        }

    }

}


type CasesDescription = "all"

interface WsServer {
    name: string,
    url: string
}

interface FuzzClientConfig {
    outdir: string,
    servers: WsServer[],
    cases: CasesDescription
}

function expandCases(cases: CasesDescription): AllCases {

    switch (cases) {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        case "all": {
            if (Object.keys(all_cases).length != 517) {
                throw new Error("Invalid test case length")
            }
            return all_cases
        }
        default: {
            throw new Error("Unimplemented")
        }
    }


}

interface SplitConfigServer {
    name: string,
    configFile: string
    outdir: string
}

interface SplitConfigValue {
    servers: SplitConfigServer[]
    cases: string[]
}

interface SplitConfig {
    split: SplitConfigValue[]
    total_cases: number
}

function normalizeServerName(name: string): string {
    return name.replaceAll(" ", "_").replaceAll("-", "_")
}



function splitCasesBy(amount: number, allCases: AllCases): string[][] {

    interface Result {
        values: string[];
        total_duration: number
    }

    const result: Result[] = new Array(amount).fill(undefined).map(_ => ({ total_duration: 0, values: [] }))

    interface Flattended { name: string, duration: number }

    const flattended: Flattended[] = Object.entries(allCases).map(([key, val]): Flattended => {
        return { name: key, duration: val.duration }
    }
    )

    const sorted: Flattended[] = flattended.sort((a: Flattended, b: Flattended) => {

        return b.duration - a.duration
    })

    for (const item of sorted) {

        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        let minRes: Result = result[0]!;
        for (const res of result) {
            if (res.total_duration < minRes.total_duration) {
                minRes = res;
            }
        }

        // add the new item to the result with the min total duration, so that bigger ones get inserted earlier and the smaller ones fill in gaps, this works fairly well for these cases (manually checked for 16 cores, but should also work for 8, 6 etc)
        minRes.values.push(item.name);
        minRes.total_duration += item.duration;
    }

    return result.map(res => res.values)

}

async function splitConfigs(amount: number, config: FuzzClientConfig): Promise<SplitConfig> {

    const allCases: AllCases = expandCases(config.cases)

    interface WsServerRaw {
        agent: string,
        url: string
    }
    interface RawCfg {
        outdir: string,
        servers: WsServerRaw[],
        cases: string[]
    }

    const globalOutdir: string = path.resolve(config.outdir)

    { //create .gitignore file

        if (!fs.existsSync(globalOutdir)) {
            await fsAsync.mkdir(globalOutdir, { recursive: true })
        }

        const gitignore_file = path.join(globalOutdir, ".gitignore")

        await fsAsync.writeFile(gitignore_file, "*")

    }

    const splitCases: string[][] = splitCasesBy(amount, allCases)

    if (splitCases.length != amount) {
        throw new Error("Implementation error")
    }

    const split: SplitConfigValue[] = [];

    for (let index = 0; index < amount; ++index) {
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        const cases: string[] = splitCases[index]!

        const result: SplitConfigValue = { cases, servers: [] }

        for (const server of config.servers) {

            const serverName = normalizeServerName(server.name)

            const tempDir = await fsAsync.mkdtemp(
                path.join(os.tmpdir(), `ws_tests-${serverName}-${index.toString()}-`)
            );


            const configFilePath = path.join(tempDir, `${serverName}_idx_${index.toString()}_config.json`);

            const outdir = path.join(globalOutdir, serverName, index.toString())

            const raw_cfg: RawCfg =
            {
                "outdir": outdir,
                "servers": [
                    {
                        "agent": server.name,
                        "url": server.url
                    }
                ],
                cases
            }

            await fsAsync.writeFile(configFilePath, JSON.stringify(raw_cfg))

            const serverCfg: SplitConfigServer = {
                configFile: configFilePath,
                name: server.name,
                outdir
            }

            result.servers.push(serverCfg)

        }

        split.push(result)

    }


    return { split, total_cases: Object.keys(allCases).length }
}


interface ExecuteResult {
    stdout: string,
    stderr: string,
    code: number
}

async function executeAsync(cmd: string, args: string[]): Promise<ExecuteResult> {

    return new Promise<ExecuteResult>((resolve, reject) => {
        const child = child_process.spawn(
            cmd, args,
        );

        const result: ExecuteResult = { stderr: "", stdout: "", code: 0 }

        child.on('close', (code, signal) => {
            if (code === null && signal === null) {
                reject(new Error(`invalid state`));
                return;
            }

            if (signal !== null) {
                reject(new Error(`Exited with signal ${signal}`));
                return;
            }

            if (code !== 0) {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                reject(new Error(`Exited with exit code ${code!.toString()}`));
                return;
            }


            result.code = code;
            resolve(result);
            return;
        });

        child.on('exit', (code, signal) => {
            if (code === null && signal === null) {
                reject(new Error(`invalid state`));
                return;
            }

            if (signal !== null) {
                reject(new Error(`Exited with signal ${signal}`));
                return;
            }

            if (code !== 0) {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                reject(new Error(`Exited with exit code ${code!.toString()}\n${result.stderr}`));
                return;
            }

            result.code = code;
            resolve(result);
            return;
        });

        child.on('error', (err) => {
            reject(new Error(`Exited with error: ${err}`));
            return;
        });

        child.stdout.on("data", (chunk: Buffer) => {
            result.stdout += chunk.toString()
        })

        child.stderr.on("data", (chunk: Buffer) => {
            result.stderr += chunk.toString()
        })

    });

}

async function launchWsTestProcessSingle(cfgFile: string): Promise<void> {

    const result = await executeAsync("pypy", ["-m", "autobahntestsuite.wstest", "--mode", "fuzzingclient", "--spec", cfgFile])

    if (result.stderr != "") {
        throw new Error(result.stderr)
    }
}

async function launchWsTestProcess(cfg: SplitConfigValue): Promise<void> {

    // launch the individual servers separate, this is already parallelized

    for (const single of cfg.servers) {
        await launchWsTestProcessSingle(single.configFile);
    }

}

async function makeHttpGetRequest(url: URL): Promise<void> {
    return new Promise<void>((resolve, reject) => {

        const options: http.RequestOptions = { method: "GET" }

        if (url.protocol === "https:") {

            https.get(url, options, (res) => {
                if (res.statusCode == undefined) {
                    reject(new Error(`Failed: no status code received`));
                    return;
                }


                if (res.statusCode !== 200) {
                    reject(new Error(`Failed: ${res.statusCode.toString()}`));
                    return;
                }

                resolve();
                return;

            }).on('error', (err) => {
                reject(err);
            });


        } else if (url.protocol === "http:") {
            http.get(url, options, (res) => {
                if (res.statusCode == undefined) {
                    reject(new Error(`Failed: no status code received`));
                    return;
                }

                if (res.statusCode !== 200) {
                    reject(new Error(`Failed: ${res.statusCode.toString()}`));
                    return;
                }

                resolve();
                return;

            }).on('error', (err) => {
                reject(err);
                return;
            });

        } else {
            reject(new Error(`Invalid URL protocol: ${url.protocol}`))
            return;
        }
    });

}

function resolveJobs(jobs: number): number {
    if (jobs <= 0) {
        const cpu_amount = os.cpus().length;
        return cpu_amount;
    }

    return jobs;
}

interface ErrorWHere {
    server: string
    case: string
}

interface ProcessResultError {
    type: "error"
    error: string
    where: ErrorWHere
    more: unknown
}

function format_process_error(err: ProcessResultError): string {
    return `${err.error} at server ${err.where.server} case ${err.where.case}\n${JSON.stringify(err.more)}`
}


function parseJSONSafe(input: string): Record<string, unknown> | null {
    try {
        return JSON.parse(input) as Record<string, unknown>
    } catch (_err) {
        return null;
    }

}


type ProcessBehavior = "OK" | "NON-STRICT" | "INFORMATIONAL"
    /* TODO: not encountered atm, check error values*/ | "ERROR"


interface CaseDescriptionRaw {
    behavior: ProcessBehavior,
    duration: number,
    reportfile: string
}

type DetailedResultsDict = Record<ProcessBehavior, number>

class ProcessDetailedResults {
    private results: DetailedResultsDict
    private _total: number

    constructor(results: DetailedResultsDict) {
        this.results = results
        this._total = ProcessDetailedResults.getTotal(this.results)
    }

    private static getTotal(results: DetailedResultsDict): number {
        return Object.values(results).reduce((acc, val) => acc + val, 0)
    }

    static default(): ProcessDetailedResults {
        return new ProcessDetailedResults({ "NON-STRICT": 0, ERROR: 0, INFORMATIONAL: 0, OK: 0 });
    }

    public add(behavior: ProcessBehavior): void {
        this.results[behavior]++;
        this._total++;
    }

    public merge(other: ProcessDetailedResults): void {
        this._total += other._total;
        for (const [key, value] of Object.entries(other.results)) {
            this.results[key as ProcessBehavior] += value
        }
    }

    public get total(): number {
        return this._total
    }

    public [Symbol.iterator](): Iterator<[behavior: ProcessBehavior, amount: number]> {
        const entries: [behavior: ProcessBehavior, amount: number][] = Object.entries(this.results) as [behavior: ProcessBehavior, amount: number][];

        return entries[Symbol.iterator]()
    }

}

interface ProcessResultSingle {
    errors: ProcessResultError[]
    details: ProcessDetailedResults
    server: string
}

const validBehaviors: ProcessBehavior[] = [
    "NON-STRICT", "OK", "INFORMATIONAL"
] as const

async function process_single_result(server: SplitConfigServer, cases: string[]): Promise<ProcessResultSingle> {

    const index_file = path.join(server.outdir, "index.json");

    function single_error(err: ProcessResultError): ProcessResultSingle {
        return {
            details: ProcessDetailedResults.default(), errors: [err], server: server.name
        }
    }

    if (!fs.existsSync(index_file)) {
        return single_error({ type: "error", error: "index file doesn't exist", where: { server: server.name, case: "<None>" }, more: { file: index_file } })
    }

    const index_content = (await fsAsync.readFile(index_file)).toString()

    const index_json = parseJSONSafe(index_content)

    if (index_json === null) {
        return single_error({ type: "error", error: "index file isn't valid JSON", where: { server: server.name, case: "<None>" }, more: { raw: index_content } })
    }

    const sever_value = index_json[server.name] as Record<string, CaseDescriptionRaw | undefined> | null | undefined;

    if (!sever_value) {
        return single_error({ type: "error", error: "the index file doesn't contain the server information", where: { server: server.name, case: "<None>" }, more: { json: index_json, server: server.name } })
    }

    const results: ProcessResultSingle = { details: ProcessDetailedResults.default(), errors: [], server: server.name }

    for (const case_ of cases) {
        const result = sever_value[case_]

        if (!result) {
            results.errors.push({ type: "error", error: `case ${case_} not present in index file`, where: { server: server.name, case: case_ }, more: { value: sever_value, case: case_ } })
            continue;
        }

        if (!validBehaviors.includes(result.behavior)) {
            results.errors.push({ type: "error", error: `invalid behavior ${result.behavior} for case ${case_}`, where: { server: server.name, case: case_ }, more: { result: result } })
            // the missing continue is intended
        }

        results.details.add(result.behavior)
    }

    return results
}

interface ProcessResultAll {
    errors: ProcessResultError[]
    details: ProcessDetailedResults
}

type ProcessResults = Record<string, ProcessResultAll>

async function process_results(split_cfg: SplitConfig): Promise<ProcessResults> {

    const to_process: Promise<ProcessResultSingle>[] = []

    for (const cfg of split_cfg.split) {
        for (const server of cfg.servers) {
            to_process.push(process_single_result(server, cfg.cases))
        }
    }

    const results: ProcessResultSingle[] = await Promise.all(to_process)

    const result: ProcessResults = results.reduce<ProcessResults>((acc, value) => {
        if (!acc[value.server]) {
            acc[value.server] = { errors: [], details: ProcessDetailedResults.default() };
        }

        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        acc[value.server]!.details.merge(value.details)
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        acc[value.server]!.errors.push(...value.errors)

        return acc;
    }, {})

    return result;
}


const global_config: FuzzClientConfig = {
    "outdir": "./reports/servers",
    "servers": [
        {
            "name": "custom server - normal",
            "url": "ws://127.0.0.1:8080/ws"
        },
        {
            "name": "custom server - fragmented",
            "url": "ws://127.0.0.1:8080/ws?fragmented"
        }
    ],
    "cases": "all"
}

export async function runWsTests(jobs: number): Promise<void> {

    const logger = Logger.getLogger()

    const waitOptions: WaitOptions = { host: "localhost", port: 8080, timeout: 120 }

    logger.info(`Wait ${waitOptions.timeout.toString()}s for ${waitOptions.host}:${waitOptions.port.toString()}`)
    const waitedFor = await waitForPort(waitOptions)
    logger.info(`Waited for ${waitedFor.toString()}ms`)

    const amount = resolveJobs(jobs)

    const config: FuzzClientConfig = global_config;

    const split_cfg = await splitConfigs(amount, config)

    try {

        logger.info(`Running ${split_cfg.total_cases.toString()} cases for ${config.servers.length.toString()} server`)

        const processes: Promise<void>[] = []

        for (const cfg of split_cfg.split) {
            processes.push(launchWsTestProcess(cfg))
        }

        await Promise.all(processes)

        logger.info(`Shutting server down`)
        await makeHttpGetRequest(new URL("http://localhost:8080/shutdown"))

        // expect the server to be down
        let error: Error | null = null;
        try {
            await sleep(100)
            await connectTo(waitOptions.host, waitOptions.port, 1000)
            error = new Error("Server still alive")
        } catch (_err) {
            //success
            error = null
        }

        if (error !== null) {
            throw error;
        }

        // scan results
        const results: ProcessResults = await process_results(split_cfg)

        let error_amount = 0;

        for (const [server, result] of Object.entries(results)) {
            logger.info(`Server: ${server}`)

            if (result.errors.length != 0) {

                for (const err of result.errors) {
                    logger.error(format_process_error(err))
                }

                logger.error(`Got ${result.errors.length.toString()} errors`)
                error_amount += result.errors.length;
                continue;
            }

            logger.info(`Successfully ran ${result.details.total.toString()} tests with ${amount.toString()} jobs`)
            for (const [behavior, amount] of result.details) {
                logger.info(`Got ${amount.toString()} cases with result ${behavior}`)
            }
        }

        if (error_amount != 0) {
            throw new Error(`Got ${error_amount.toString()} errors in total`)
        } else {
            logger.info(`Success`)
        }

    } catch (err) {
        logger.fail(err as Error)
    } finally {

        const cleanup_calls: Promise<void>[] = []

        for (const cfg of split_cfg.split) {
            for (const server of cfg.servers) {
                cleanup_calls.push(fsAsync.unlink(server.configFile))
            }
        }

        await Promise.all(cleanup_calls)

    }

}
