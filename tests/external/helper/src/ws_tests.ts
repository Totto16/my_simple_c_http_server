import net from "node:net"
import os from "node:os"
import fsAsync from "node:fs/promises"
import path from "node:path"
import child_process from "node:child_process";
import http from "node:http"
import https from "node:https"
import { AllCases, all_cases } from "./all_cases"

interface WaitOptions {
    host: string,
    port: number,
    timeout: number
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

async function waitForPort(options: WaitOptions): Promise<void> {

    function error(reason: string): void {
        throw new Error(`Failed to wait for '${options.host}:${options.port}': ${reason}`)
    }

    const finalTimeout = setTimeout((): void => {
        error("timeout");
    }, options.timeout * 1000)

    function success(): void {
        clearTimeout(finalTimeout)
    }

    const connectionTimeout = 1000;

    while (true) {
        try {
            await connectTo(options.host, options.port, connectionTimeout)
            success();
            return;
        } catch (err) {
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

    const splitCases: string[][] = splitCasesBy(amount, allCases)

    if (splitCases.length != amount) {
        throw new Error("Implementation error")
    }

    const split: SplitConfigValue[] = [];

    for (let index = 0; index < amount; ++index) {
        const cases: string[] = splitCases[index]!

        const result: SplitConfigValue = { cases, servers: [] }

        for (const server of config.servers) {

            const serverName = normalizeServerName(server.name)

            const tempDir = await fsAsync.mkdtemp(
                path.join(os.tmpdir(), `ws_tests-${serverName}-${index}-`)
            );


            const configFilePath = path.join(tempDir, `${serverName}_idx_${index}_config.json`);

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


    return { split }
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
                reject(new Error(`Exited with exit code ${code}`));
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
                reject(new Error(`Exited with exit code ${code}`));
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

        child.stdout.on("data", (chunk) => {
            result.stdout += chunk.toString()
        })

        child.stderr.on("data", (chunk) => {
            result.stderr += chunk.toString()
        })

    });

}

async function launchWsTestProcessSingle(cfgFile: string): Promise<void> {

    const result = await executeAsync("pypy", ["-m", "autobahntestsuite.wstest", "--mode", "fuzzingclient", "--spec", cfgFile])

    if (result.stderr != "") {
        console.error(result.stderr)
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
                    reject(new Error(`Failed: ${res.statusCode}`));
                    return;
                }


                res.on('finish', () => {
                    resolve();
                });
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
                    reject(new Error(`Failed: ${res.statusCode}`));
                    return;
                }


                res.on('finish', () => {
                    resolve();
                });
            }).on('error', (err) => {
                reject(err);
            });

        } else {
            reject(new Error(`Invalid URL protocol: ${url.protocol}`))
        }
    });


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

export async function runWsTests(): Promise<void> {
    //TODO: ws tests, run autobahn and scan resulting json files

    const waitOptions: WaitOptions = { host: "localhost", port: 8080, timeout: 120 }

    await waitForPort(waitOptions)

    const cpu_amount = os.cpus().length;

    const config: FuzzClientConfig = global_config;

    const split_cfg = await splitConfigs(cpu_amount, config)

    try {

        const processes: Promise<void>[] = []

        for (const cfg of split_cfg.split) {
            processes.push(launchWsTestProcess(cfg)
            )
        }

        await Promise.all(processes)


        await makeHttpGetRequest(new URL("http://localhost:8080/shutdown"))

        // expect the server to be down
        try {

            connectTo(waitOptions.host, waitOptions.port, 1000)
            throw new Error("Server still alive")
        } catch (err) {
            //success
        }

        // scan results
        throw new Error("TODO: scan results")

    } catch (err) {
        throw err;
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
