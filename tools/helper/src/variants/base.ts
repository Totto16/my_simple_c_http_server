import type { Expect, Equal } from "type-testing"
import { arrayIsEq, assert, isUTF8String } from "../utils.js"


function isUpperCase(str: string): boolean {
    if (str.length != 1) {
        throw new Error("Not of length 1")
    }

    return (str.charCodeAt(0) >= 'A'.charCodeAt(0)) && (str.charCodeAt(0) <= 'Z'.charCodeAt(0))
}

export class CaseName {
    private _parts: string[]

    protected constructor(parts: string[]) {
        this._parts = parts;
        for (const p of parts) {
            assert(p.toLowerCase() === p || p.toUpperCase() === p, "parts are all lowercase or all uppercase")
        }
    }

    public static fromParts(parts: string[]): CaseName {
        return new CaseName(parts)
    }


    public static fromPascalCase(str: string): CaseName {

        if (isUTF8String(str)) {
            throw new Error(`Unicode strings not yet supported: ${str}`)
        }

        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        if (!isUpperCase(str[0]!)) {
            throw new Error(`First character has to be uppercase: ${str}`)
        }

        const parts: string[] = []

        {

            type Temp = [chr: string, idx: number]

            const indexes: number[] = (str.split("").map((chr, idx) => ([chr, idx])) as Temp[]).filter((([chr, _]) => {
                return isUpperCase(chr);
            })).map(([_, idx]) => idx)

            for (let i = 0; i < indexes.length; ++i) {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                const start = indexes[i]!

                if (i - 1 < indexes.length) {
                    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                    const end = indexes[i + 1]!

                    if ((end - start) == 1) {
                        throw new Error(`Two UpperCase characters near each other: ${str}`)
                    }

                    const part = str.substring(start, end).toLowerCase()
                    parts.push(part)

                } else {
                    const part = str.substring(start).toLowerCase()
                    parts.push(part)
                }


            }


        }

        const result = new CaseName(parts)

        assert(str === result.PascalCase(), "Implementation error in fromPascalCase")


        return result;

    }

    public static fromSnakeCase(str: string): CaseName {

        if (isUTF8String(str)) {
            throw new Error(`Unicode strings not yet supported: ${str}`)
        }

        if (str.toLowerCase() != str) {
            throw new Error(`snake case strings can't have uppercase characters: ${str}`)
        }

        const parts: string[] = []

        {

            type Temp = [chr: string, idx: number]

            const indexes: number[] = (str.split("").map((chr, idx) => ([chr, idx])) as Temp[]).filter((([chr, _]) => {
                return chr == '_';
            })).map(([_, idx]) => idx)

            if (indexes.length == 0) {
                parts.push(str)
            }

            for (let i = 0; i < indexes.length; ++i) {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                const start = indexes[i]!

                if (i == 0) {
                    const end = start

                    if (end == 1) {
                        throw new Error(`Two underscore (_) characters near each other: ${str}`)
                    }

                    const part = str.substring(0, end).toLowerCase()
                    parts.push(part)

                }

                if (i - 1 < indexes.length) {
                    // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                    const end = indexes[i + 1]!

                    if ((end - start) == 1) {
                        throw new Error(`Two underscore (_) characters near each other: ${str}`)
                    }

                    const part = str.substring(start + 1, end).toLowerCase()
                    parts.push(part)

                } else {
                    const part = str.substring(start + 1).toLowerCase()
                    parts.push(part)
                }


            }


        }

        const result = new CaseName(parts)
        assert(str === result.snake_case(), "Implementation error in fromSnakeCase")


        return result;

    }

    public combine(other: CaseName): CaseName {

        const parts = [...this._parts]
        parts.push(...other._parts)

        return new CaseName(parts)
    }

    public eq(other: CaseName): boolean {
        return arrayIsEq<string>(this._parts, other._parts)
    }

    public PascalCase(): string {
        return this._parts.map((p) => {
            if (p.length === 1) {
                throw new Error("wrongfully constructed CaseName, one part has a size one string!")
            }

            // note: part 2 can be upper or lowercase, as it doesn't matter

            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            return `${p[0]!.toUpperCase()}${p.substring(1)}`
        }).join("")
    }

    public MACRO_NAME(): string {
        return this._parts.map((p) => {
            return p.toUpperCase()
        }).join("_")
    }

    public snake_case(): string {
        return this._parts.map((p) => {
            return p.toLowerCase();
        }).join("_")
    }

}

const __brand_tag_never_use: unique symbol = Symbol("brandTag");

interface Brand<T extends string> {
    readonly [__brand_tag_never_use]: T;
}


function makeBranded<T extends string>(value: T): Brand<T> {
    return {
        [__brand_tag_never_use]: value,
    };
}

export function getBrand<T extends string = string>(b: Brand<T>): T {
    return b[__brand_tag_never_use]
}

export interface StructMember extends Brand<"simple"> {
    typeName: CType
    name: string,
}

type CType = string

interface CAnonymousStruct extends Brand<"c_anonymous_struct"> {
    members: StructMember[]
}

function makeCAnonymousStruct(members: StructMember[]): CAnonymousStruct {
    return {
        ...(makeBranded<"c_anonymous_struct">("c_anonymous_struct")),
        members: members
    }
}


export function makeTaggedName<T>(value: T, name: CaseName): TaggedName<T> {
    return {
        ...(makeBranded<"tagged_name">("tagged_name")),
        nameType: value,
        inner: name
    }
}


export interface TaggedTypeSimple extends Brand<"simple"> {
    name: CType
}

export function makeSimpleType(name: CType): TaggedTypeSimple {
    return {
        ...(makeBranded<"simple">("simple")),
        name: name
    }
}

export type ID = number

export interface TaggedTypeStruct extends Brand<"struct"> {
    struct: CAnonymousStruct
    id: ID
}

let globalStructId: ID = 0

export function makeStructType(members: StructMember[]): TaggedTypeStruct {
    const id = globalStructId
    globalStructId++;
    return {
        ...(makeBranded<"struct">("struct")),
        struct: makeCAnonymousStruct(members),
        id
    }
}

export type TaggedType = TaggedTypeSimple | TaggedTypeStruct


export function isSimpleTaggedType(t: TaggedType): t is TaggedTypeSimple {
    return getBrand(t) == "simple"
}

export interface TaggedMember {
    name: TaggedName<"member">,
    type: null | TaggedType
}

export function makeMemberName(name: CaseName): TaggedName<"member"> {
    return makeTaggedName<"member">("member", name)
}



export function makeEnumName(name: CaseName): TaggedName<"enum"> {
    return makeTaggedName<"enum">("enum", name)
}

type StructOrderRequirement = "best_size" | "aligned_access"

export interface TaggedUnionRequirements {
    order: StructOrderRequirement
}

export type StructOrder = "auto" | "tag_first" | "tag_second"

export interface CppFeatures {
    tagAsErrorVariant: boolean
}

export interface TaggedName<T> extends Brand<"tagged_name"> {
    nameType: T,
    inner: CaseName
}


interface TaggedUnionOptions {
    rawStruct: TaggedName<"raw_struct">
    structOrder: StructOrder
    requirements: TaggedUnionRequirements
    cppFeatures: CppFeatures
}


export function makeStructMember(typeName: CType, name: string): StructMember {
    return {
        ...(makeBranded<"simple">("simple")),
        name,
        typeName: typeName
    }
}


export type CEnumType = "bool" | "u8" | "u16" | "u32" | "u64"



export interface TaggedUnionEnum {
    underlyingType: CEnumType | "best_match" | null
    name: TaggedName<"enum">
}


export interface TaggedUnion {
    name: TaggedName<"union">
    member: TaggedMember[]
    enum: TaggedUnionEnum
    options: DeepPartial<TaggedUnionOptions, [TaggedName<"raw_struct">]>
}

type IsOneOf<T, Arr extends unknown[]> = Arr extends [] ? false : Arr extends [infer A, ...infer Rest] ? A extends T ? true : IsOneOf<T, Rest> : false

export type DeepPartial<T, Ends extends unknown[] = []> = IsOneOf<T, Ends> extends true ? T : T extends object ? {
    [P in keyof T]?: DeepPartial<T[P], Ends> | undefined
} : T;


type _type_assert_0 = Expect<Equal<IsOneOf<CaseName, [CaseName]>, true>>

type _type_assert_1 = Expect<Equal<(DeepPartial<TaggedUnionOptions, [TaggedName<"raw_struct">]>["rawStruct"]), TaggedName<"raw_struct"> | undefined>>

export function makeUnionName(name: CaseName): TaggedName<"union"> {
    return makeTaggedName<"union">("union", name)
}


