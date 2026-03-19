import net from "node:net"
import os from "node:os"
import fs from "node:fs"
import fsAsync from "node:fs/promises"
import path from "node:path"

interface WaitOptions {
    host: string,
    port: number,
    timeout: number
}

async function connectTo(host: string, port: number, timeout: number): Promise<void> {
    return new Promise<void>((resolve, reject) => {
        const socket = net.createConnection({ port, host, timeout }, () => {
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
        throw new Error(`Failed to wait for ${options.host}:${options.port}. ${reason}`)
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

const all_cases: string[] = ["1.1.1", "1.1.2", "1.1.3", "1.1.4", "1.1.5", "1.1.6", "1.1.7", "1.1.8", "1.2.1", "1.2.2", "1.2.3", "1.2.4", "1.2.5", "1.2.6", "1.2.7", "1.2.8", "10.1.1", "12.1.1", "12.1.10", "12.1.11", "12.1.12", "12.1.13", "12.1.14", "12.1.15", "12.1.16", "12.1.17", "12.1.18", "12.1.2", "12.1.3", "12.1.4", "12.1.5", "12.1.6", "12.1.7", "12.1.8", "12.1.9", "12.2.1", "12.2.10", "12.2.11", "12.2.12", "12.2.13", "12.2.14", "12.2.15", "12.2.16", "12.2.17", "12.2.18", "12.2.2", "12.2.3", "12.2.4", "12.2.5", "12.2.6", "12.2.7", "12.2.8", "12.2.9", "12.3.1", "12.3.10", "12.3.11", "12.3.12", "12.3.13", "12.3.14", "12.3.15", "12.3.16", "12.3.17", "12.3.18", "12.3.2", "12.3.3", "12.3.4", "12.3.5", "12.3.6", "12.3.7", "12.3.8", "12.3.9", "12.4.1", "12.4.10", "12.4.11", "12.4.12", "12.4.13", "12.4.14", "12.4.15", "12.4.16", "12.4.17", "12.4.18", "12.4.2", "12.4.3", "12.4.4", "12.4.5", "12.4.6", "12.4.7", "12.4.8", "12.4.9", "12.5.1", "12.5.10", "12.5.11", "12.5.12", "12.5.13", "12.5.14", "12.5.15", "12.5.16", "12.5.17", "12.5.18", "12.5.2", "12.5.3", "12.5.4", "12.5.5", "12.5.6", "12.5.7", "12.5.8", "12.5.9", "13.1.1", "13.1.10", "13.1.11", "13.1.12", "13.1.13", "13.1.14", "13.1.15", "13.1.16", "13.1.17", "13.1.18", "13.1.2", "13.1.3", "13.1.4", "13.1.5", "13.1.6", "13.1.7", "13.1.8", "13.1.9", "13.2.1", "13.2.10", "13.2.11", "13.2.12", "13.2.13", "13.2.14", "13.2.15", "13.2.16", "13.2.17", "13.2.18", "13.2.2", "13.2.3", "13.2.4", "13.2.5", "13.2.6", "13.2.7", "13.2.8", "13.2.9", "13.3.1", "13.3.10", "13.3.11", "13.3.12", "13.3.13", "13.3.14", "13.3.15", "13.3.16", "13.3.17", "13.3.18", "13.3.2", "13.3.3", "13.3.4", "13.3.5", "13.3.6", "13.3.7", "13.3.8", "13.3.9", "13.4.1", "13.4.10", "13.4.11", "13.4.12", "13.4.13", "13.4.14", "13.4.15", "13.4.16", "13.4.17", "13.4.18", "13.4.2", "13.4.3", "13.4.4", "13.4.5", "13.4.6", "13.4.7", "13.4.8", "13.4.9", "13.5.1", "13.5.10", "13.5.11", "13.5.12", "13.5.13", "13.5.14", "13.5.15", "13.5.16", "13.5.17", "13.5.18", "13.5.2", "13.5.3", "13.5.4", "13.5.5", "13.5.6", "13.5.7", "13.5.8", "13.5.9", "13.6.1", "13.6.10", "13.6.11", "13.6.12", "13.6.13", "13.6.14", "13.6.15", "13.6.16", "13.6.17", "13.6.18", "13.6.2", "13.6.3", "13.6.4", "13.6.5", "13.6.6", "13.6.7", "13.6.8", "13.6.9", "13.7.1", "13.7.10", "13.7.11", "13.7.12", "13.7.13", "13.7.14", "13.7.15", "13.7.16", "13.7.17", "13.7.18", "13.7.2", "13.7.3", "13.7.4", "13.7.5", "13.7.6", "13.7.7", "13.7.8", "13.7.9", "2.1", "2.10", "2.11", "2.2", "2.3", "2.4", "2.5", "2.6", "2.7", "2.8", "2.9", "3.1", "3.2", "3.3", "3.4", "3.5", "3.6", "3.7", "4.1.1", "4.1.2", "4.1.3", "4.1.4", "4.1.5", "4.2.1", "4.2.2", "4.2.3", "4.2.4", "4.2.5", "5.1", "5.10", "5.11", "5.12", "5.13", "5.14", "5.15", "5.16", "5.17", "5.18", "5.19", "5.2", "5.20", "5.3", "5.4", "5.5", "5.6", "5.7", "5.8", "5.9", "6.1.1", "6.1.2", "6.1.3", "6.10.1", "6.10.2", "6.10.3", "6.11.1", "6.11.2", "6.11.3", "6.11.4", "6.11.5", "6.12.1", "6.12.2", "6.12.3", "6.12.4", "6.12.5", "6.12.6", "6.12.7", "6.12.8", "6.13.1", "6.13.2", "6.13.3", "6.13.4", "6.13.5", "6.14.1", "6.14.10", "6.14.2", "6.14.3", "6.14.4", "6.14.5", "6.14.6", "6.14.7", "6.14.8", "6.14.9", "6.15.1", "6.16.1", "6.16.2", "6.16.3", "6.17.1", "6.17.2", "6.17.3", "6.17.4", "6.17.5", "6.18.1", "6.18.2", "6.18.3", "6.18.4", "6.18.5", "6.19.1", "6.19.2", "6.19.3", "6.19.4", "6.19.5", "6.2.1", "6.2.2", "6.2.3", "6.2.4", "6.20.1", "6.20.2", "6.20.3", "6.20.4", "6.20.5", "6.20.6", "6.20.7", "6.21.1", "6.21.2", "6.21.3", "6.21.4", "6.21.5", "6.21.6", "6.21.7", "6.21.8", "6.22.1", "6.22.10", "6.22.11", "6.22.12", "6.22.13", "6.22.14", "6.22.15", "6.22.16", "6.22.17", "6.22.18", "6.22.19", "6.22.2", "6.22.20", "6.22.21", "6.22.22", "6.22.23", "6.22.24", "6.22.25", "6.22.26", "6.22.27", "6.22.28", "6.22.29", "6.22.3", "6.22.30", "6.22.31", "6.22.32", "6.22.33", "6.22.34", "6.22.4", "6.22.5", "6.22.6", "6.22.7", "6.22.8", "6.22.9", "6.23.1", "6.23.2", "6.23.3", "6.23.4", "6.23.5", "6.23.6", "6.23.7", "6.3.1", "6.3.2", "6.4.1", "6.4.2", "6.4.3", "6.4.4", "6.5.1", "6.5.2", "6.5.3", "6.5.4", "6.5.5", "6.6.1", "6.6.10", "6.6.11", "6.6.2", "6.6.3", "6.6.4", "6.6.5", "6.6.6", "6.6.7", "6.6.8", "6.6.9", "6.7.1", "6.7.2", "6.7.3", "6.7.4", "6.8.1", "6.8.2", "6.9.1", "6.9.2", "6.9.3", "6.9.4", "7.1.1", "7.1.2", "7.1.3", "7.1.4", "7.1.5", "7.1.6", "7.13.1", "7.13.2", "7.3.1", "7.3.2", "7.3.3", "7.3.4", "7.3.5", "7.3.6", "7.5.1", "7.7.1", "7.7.10", "7.7.11", "7.7.12", "7.7.13", "7.7.2", "7.7.3", "7.7.4", "7.7.5", "7.7.6", "7.7.7", "7.7.8", "7.7.9", "7.9.1", "7.9.2", "7.9.3", "7.9.4", "7.9.5", "7.9.6", "7.9.7", "7.9.8", "7.9.9", "9.1.1", "9.1.2", "9.1.3", "9.1.4", "9.1.5", "9.1.6", "9.2.1", "9.2.2", "9.2.3", "9.2.4", "9.2.5", "9.2.6", "9.3.1", "9.3.2", "9.3.3", "9.3.4", "9.3.5", "9.3.6", "9.3.7", "9.3.8", "9.3.9", "9.4.1", "9.4.2", "9.4.3", "9.4.4", "9.4.5", "9.4.6", "9.4.7", "9.4.8", "9.4.9", "9.5.1", "9.5.2", "9.5.3", "9.5.4", "9.5.5", "9.5.6", "9.6.1", "9.6.2", "9.6.3", "9.6.4", "9.6.5", "9.6.6", "9.7.1", "9.7.2", "9.7.3", "9.7.4", "9.7.5", "9.7.6", "9.8.1", "9.8.2", "9.8.3", "9.8.4", "9.8.5", "9.8.6"]

function expandCases(cases: CasesDescription): string[] {

    switch (cases) {
        case "all": {
            if (all_cases.length != 517) {
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

async function splitConfigs(amount: number, config: FuzzClientConfig): Promise<SplitConfig> {

    const allCases: string[] = expandCases(config.cases)

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

    const amountPerSplit = Math.ceil(allCases.length / amount)

    const split: SplitConfigValue[] = [];

    for (let index = 0; index < amount; ++index) {
        const cases: string[] = allCases.slice(index * amountPerSplit, (index + 1) * amountPerSplit)

        const result: SplitConfigValue = { cases, servers: [] }

        for (const server of config.servers) {

            const serverName = normalizeServerName(server.name)

            const tempDir = await fsAsync.mkdtemp(
                path.join(os.tmpdir(), `ws_tests-${index}-${serverName}-`)
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


async function launchWsTestProcess(cfg: SplitConfigValue): Promise<void> {

    throw new Error("TODO")

}

async function makeHttpRequest(method: string, url: string): Promise<void> {
    throw new Error("TODO")
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

    waitForPort(waitOptions)

    const cpu_amount = os.cpus().length;

    const config: FuzzClientConfig = global_config;

    const split_cfg = await splitConfigs(cpu_amount, config)

    const processes: Promise<void>[] = []

    for (const cfg of split_cfg.split) {
        processes.push(launchWsTestProcess(cfg)
        )
    }

    await Promise.all(processes)


    await makeHttpRequest("GET", "http://localhost:8080/shutdown")

    // expect the server to be down
    try {

        connectTo(waitOptions.host, waitOptions.port, 1000)
        throw new Error("Server still alive")
    } catch (err) {
        //success
    }

    // scan results

}




const commands = [



    "pypy -m autobahntestsuite.wstest --mode fuzzingclient --spec './tests/autobahn/config/fuzzingclient.json'",
]



