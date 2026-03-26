import path from "node:path"
import { assert, getOtherFile, is_utf8_string, writeFileAndDirs } from "./utils.js"

function isUpperCase(str: string) {
    if (str.length != 1) {
        throw new Error("Not of length 1")
    }

    return (str.charCodeAt(0) >= 'A'.charCodeAt(0)) && (str.charCodeAt(0) <= 'Z'.charCodeAt(0))
}

class CaseName {
    private _parts: string[]

    protected constructor(parts: string[]) {
        this._parts = parts;
        for (const p of parts) {
            assert(p.toLowerCase() === p, "parts are all lowercase")
        }
    }

    public static fromPascalCase(str: string): CaseName {

        if (is_utf8_string(str)) {
            throw new Error(`Unicode strings not yet supported: ${str}`)
        }

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
                const start = indexes[i]!

                if (i - 1 < indexes.length) {
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

    public combine(other: CaseName): CaseName {

        const parts = [...this._parts]
        parts.push(...other._parts)

        return new CaseName(parts)

    }

    public PascalCase(): string {
        return this._parts.map((p) => {
            if (p.length === 1) {
                throw new Error("wrongfully constructed CaseName, one part has a size one string!")
            }
            return `${p[0]?.toUpperCase()}${p.substring(1)}`
        }).join("")
    }

    public MACRO_NAME(): string {
        return this._parts.map((p) => {
            return p.toUpperCase()
        }).join("_")
    }

    public snake_case(): string {
        return this._parts.map((p) => {
            assert(p === p.toLowerCase(), "all parts need to be lowercase")
            return p;
        }).join("_")
    }

}

type CType = string

interface StructMember {
    name: string,
    type: CType | CAnonymousStruct
}

interface CAnonymousStruct {
    type: "c_anonymous_struct"
    members: StructMember[]
}

type CEnumType = "bool" | "u8" | "u16" | "u32" | "u64"

interface TaggedMember {
    name: CaseName,
    value: null | CType | CAnonymousStruct
}

interface TaggedUnionEnum {
    underlying_type: CEnumType | "best_match" | null
    name: CaseName
}

interface TaggedUnion {
    name: CaseName
    member: TaggedMember[]
    enum: TaggedUnionEnum
}

function bestTypeForLength(len: number): CEnumType {


    if (len < 2) {
        throw new Error("Should not use tagged unions in this case")
    }

    if (len == 2) {
        return "bool"
    }

    if (len < (1 << 8)) {
        return "u8"
    }

    if (len < (1 << 16)) {
        return "u16"
    }

    if (len < (1 << 16)) {
        return "u32"
    }

    if (len >= Number.MAX_SAFE_INTEGER) {
        throw new Error("This is unreasonable")
    }

    if (Number.isNaN(len)) {
        throw new Error("How did you managed to get here?!?")
    }

    throw new Error("Unimplemented, length is huuuuuuge")

}

function resolveUnderlyingType(original: CEnumType | "best_match" | null, member_len: number): CEnumType | null {

    if (original === "best_match") {
        return bestTypeForLength(member_len)
    }

    return original

}

function c_type_for_enum(tpe: CEnumType): string {
    switch (tpe) {
        case "bool": {
            return "bool";
        }
        case "u8": {
            return "uint8_t";
        }
        case "u16": {
            return "uint16_t";
        }
        case "u32": {
            return "uint32_t";
        }
        case "u64": {
            return "uint64_t";
        }
        default: {
            throw new Error("UNREACHABLE")
        }
    }
}

function memberNameForEnum(member: TaggedMember, union_name: CaseName): string {
    return union_name.combine(member.name).PascalCase()
}


function generateEnumDeclaration(tu_enum: TaggedUnionEnum, member: TaggedMember[]): string {

    const underlying_type: CEnumType | null = resolveUnderlyingType(tu_enum.underlying_type, member.length)

    return `
/* @enum value */
typedef enum${underlying_type === null ? "" : ` C_23_NARROW_ENUM_TO(${c_type_for_enum(underlying_type)})`} {
	${member.map((mem, idx) => {

        if (underlying_type === "bool") {
            return `${memberNameForEnum(mem, tu_enum.name)} = ${idx ? "true" : "false"}`
        }

        if (idx == 0) {
            return `${memberNameForEnum(mem, tu_enum.name)} = 0`
        }

        return memberNameForEnum(mem, tu_enum.name)
    }).join(",\n	")}
} ${tu_enum.name.PascalCase()};
`


}

function getUnionTagName(name: CaseName): string {
    return `_variant_tag_for_${name.snake_case()}_tag_member`
}

function getUnionDataName(name: CaseName): string {
    return `_variant_data_for_${name.snake_case()}_data_member`
}

function typeForMember(val: string | CAnonymousStruct): string {
    if (typeof (val) == "string") {
        return val;
    }

    return `struct {
	${val.members.map(mem => {
        return `${mem.type} ${mem.name};`
    }).join("\n	")}
}`

}

function generateVariantDeclaration(tagged_union: TaggedUnion): string {

    const tag_name: string = getUnionTagName(tagged_union.name);

    const data_name: string = getUnionDataName(tagged_union.name);


    return `
/* tagged union implementation */
typedef struct {
	${tagged_union.enum.name.PascalCase()} ${tag_name};
	union {
		${tagged_union.member.filter(mem => mem.value !== null).map((mem) => {

        if (mem.value === null) {
            throw new Error("IMPLEMENTATION ERROR")
        }

        return `${typeForMember(mem.value)} ${mem.name.snake_case()};`.split("\n").join("\n		")

    }).join("\n		")}
	} ${data_name};
} ${tagged_union.name.PascalCase()};
`
}

function generatePoisonPragma(names: string[]): string {
    return `_Pragma ("GCC poison ${names.join(" ")}")`
}


function generatedUnionForCHeader(tagged_union: TaggedUnion): string {

    assert(tagged_union.member.length >= 2, "at least two member are required")

    assert(tagged_union.enum.underlying_type !== null, "prefer having enums with underlying type")

    const enumString = generateEnumDeclaration(tagged_union.enum, tagged_union.member)

    const variantString = generateVariantDeclaration(tagged_union)

    const tag_name: string = getUnionTagName(tagged_union.name);

    const data_name: string = getUnionDataName(tagged_union.name);



    return `#define GENERATE_VARIANT_${tagged_union.name.MACRO_NAME()}
	${enumString.split("\n").join("\n	")}
	${variantString.split("\n").join("\n	")}
	${generatePoisonPragma([tag_name, data_name])}
`.split("\n").join("\\\n")



}


const unitagged_unions: TaggedUnion[] = [
    {
        name: CaseName.fromPascalCase("AccountInfo"),
        member: [
            {
                name: CaseName.fromPascalCase("Empty"),
                value: null
            },
            {
                name: CaseName.fromPascalCase("OnlyUser"),
                value: {
                    type: "c_anonymous_struct",
                    members: [
                        {
                            name: "username",
                            type: "tstr"
                        }
                    ]
                }
            },
            {
                name: CaseName.fromPascalCase("Ok"),
                value: "AccountOkData"
            }
        ],
        enum: {
            name: CaseName.fromPascalCase("AccountState"),
            underlying_type: "u8"
        }

    }

]


export async function generate_variant_code_c(generated_variants_file_h: string): Promise<void> {

    const tasks: Promise<void>[] = []

    assert(path.extname(generated_variants_file_h) == ".h", "variant file has to end in .h")

    const h_data = `
#pragma once

${unitagged_unions.map(un => generatedUnionForCHeader(un)).join("\n\n")}


`

    tasks.push(writeFileAndDirs(generated_variants_file_h, h_data))

    const c_data = `
#include "./${path.basename(generated_variants_file_h)}"

`

    const generated_variants_file_c = getOtherFile(generated_variants_file_h, ".h", ".c")

    tasks.push(writeFileAndDirs(generated_variants_file_c, c_data))

    await Promise.all(tasks)


}
