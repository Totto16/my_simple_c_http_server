import * as core from '@actions/core';

export abstract class Group {

    protected constructor() {
        //
    }

    abstract start(): void;

    abstract end(): void;

    abstract process<T>(fn: () => T): T;

    abstract processAsync<T = void>(fn: () => Promise<T>): Promise<T>;
}


type CiStatus = "None" | "CI" | "DebugCI"

function get_ci_status(): CiStatus {
    // see: https://docs.github.com/en/actions/reference/workflows-and-actions/variables#default-environment-variables
    if (process.env.GITHUB_ACTIONS === "true") {
        if (core.isDebug()) {
            return "DebugCI"
        }

        return "CI"
    }

    return "None"
}


export abstract class Logger {

    protected constructor() {
        //
    }

    static getLogger(): Logger {

        const ci_status = get_ci_status()

        if (ci_status != "None") {
            return new CILogger(ci_status == "DebugCI")
        }

        return new NormalLogger()
    }

    abstract fail(err: string | Error): never;

    abstract error(msg: string | Error): void;

    abstract warning(msg: string | Error): void;

    abstract info(msg: string): void;

    abstract notice(msg: string): void;

    abstract debug(msg: string): void;

    abstract getGroup(name: string): Group
}

class NormalGroup extends Group {

    private name: string

    constructor(name: string) {
        super()
        this.name = name;
    }

    start(): void {
        console.log(`Start group: ${this.name}`)
    }


    end(): void {
        console.log(`End group: ${this.name}`)
    }

    process<T>(fn: () => T): T {

        this.start()
        const result = fn()
        this.end()
        return result;
    }

    async processAsync<T = void>(fn: () => Promise<T>): Promise<T> {
        this.start()
        const result = await fn()
        this.end()
        return result;
    }

}


class NormalLogger extends Logger {

    fail(err: string | Error): never {
        console.error(`Failed with: ${err}`)
        process.exit(1)
    }


    error(msg: string | Error): void {
        console.error(msg);
    }

    warning(msg: string | Error): void {
        console.warn(msg);
    }

    info(msg: string): void {
        console.info(msg);
    }

    notice(msg: string): void {
        console.info(msg);
    }

    debug(msg: string): void {
        console.debug(msg);
    }

    getGroup(name: string): Group {
        return new NormalGroup(name)
    }

}


class CIGroup extends Group {

    private _debug: boolean
    private name: string

    constructor(debug: boolean, name: string) {
        super()
        this._debug = debug;
        this.name = name;
    }

    start(): void {
        core.startGroup(this.name)
    }


    end(): void {
        core.endGroup()
    }

    process<T>(fn: () => T): T {

        this.start()
        const result = fn()
        this.end()
        return result;
    }

    async processAsync<T = void>(fn: () => Promise<T>): Promise<T> {
        return await core.group(this.name, fn)
    }

}



class CILogger extends Logger {

    private _debug: boolean

    constructor(debug: boolean) {
        super()
        this._debug = debug
    }

    fail(err: string | Error): never {
        core.setFailed(err)
        process.exit(1)
    }

    error(msg: string | Error): void {
        core.error(msg);
    }

    warning(msg: string | Error): void {
        core.warning(msg);
    }

    info(msg: string): void {
        core.info(msg);
    }

    notice(msg: string): void {
        core.notice(msg);
    }

    debug(msg: string): void {
        if (!this._debug) {
            return;
        }

        core.debug(msg);
    }

    getGroup(name: string): Group {
        return new CIGroup(this._debug, name)
    }

}
