import path from "node:path"
import { assert, isUTF8String, writeFileAndDirs } from "./utils.js"

function isUpperCase(str: string): boolean {
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

type CType = string

const __brand_tag_never_use: unique symbol = Symbol("brandTag");

interface Brand<T extends string> {
    readonly [__brand_tag_never_use]: T;
}

function makeBranded<T extends string>(value: T): Brand<T> {
    return {
        [__brand_tag_never_use]: value,
    };
}

function getBrand<T extends string = string>(b: Brand<T>): T {
    return b[__brand_tag_never_use]
}

interface StructMember extends Brand<"simple"> {
    name: string,
    typeName: CType
}

function makeStructMember(name: string, typeName: CType): StructMember {
    return {
        ...(makeBranded<"simple">("simple")),
        name,
        typeName: typeName
    }
}


interface CAnonymousStruct extends Brand<"c_anonymous_struct"> {
    members: StructMember[]
}

function makeCAnonymousStruct(members: StructMember[]): CAnonymousStruct {
    return {
        ...(makeBranded<"c_anonymous_struct">("c_anonymous_struct")),
        members: members
    }
}

type CEnumType = "bool" | "u8" | "u16" | "u32" | "u64"

interface TaggedName<T> extends Brand<"tagged_name"> {
    nameType: T,
    inner: CaseName
}

function makeTaggedName<T>(value: T, name: CaseName): TaggedName<T> {
    return {
        ...(makeBranded<"tagged_name">("tagged_name")),
        nameType: value,
        inner: name
    }
}


interface TaggedTypeSimple extends Brand<"simple"> {
    name: CType
}

function makeSimpleType(name: CType): TaggedTypeSimple {
    return {
        ...(makeBranded<"simple">("simple")),
        name: name
    }
}

type ID = number

interface TaggedTypeStruct extends Brand<"struct"> {
    struct: CAnonymousStruct
    id: ID
}

let globalStructId: ID = 0

function makeStructType(members: StructMember[]): TaggedTypeStruct {
    const id = globalStructId
    globalStructId++;
    return {
        ...(makeBranded<"struct">("struct")),
        struct: makeCAnonymousStruct(members),
        id
    }
}

type TaggedType = TaggedTypeSimple | TaggedTypeStruct


function isSimpleTaggedType(t: TaggedType): t is TaggedTypeSimple {
    return getBrand(t) == "simple"
}

interface TaggedMember {
    name: TaggedName<"member">,
    type: null | TaggedType
}

function makeMemberName(name: CaseName): TaggedName<"member"> {
    return makeTaggedName<"member">("member", name)
}

interface TaggedUnionEnum {
    underlyingType: CEnumType | "best_match" | null
    name: TaggedName<"enum">
}

function makeEnumName(name: CaseName): TaggedName<"enum"> {
    return makeTaggedName<"enum">("enum", name)
}

interface TaggedUnionOptions {
    rawStruct?: CaseName | undefined
}

interface TaggedUnion {
    name: TaggedName<"union">
    member: TaggedMember[]
    enum: TaggedUnionEnum
    options: TaggedUnionOptions
}

function makeUnionName(name: CaseName): TaggedName<"union"> {
    return makeTaggedName<"union">("union", name)
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

function resolveUnderlyingType(original: CEnumType | "best_match" | null, memberLen: number): CEnumType | null {

    if (original === "best_match") {
        return bestTypeForLength(memberLen)
    }

    return original

}

function cTypeForEnum(tpe: CEnumType): string {
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

function memberNameForEnum(member: TaggedMember, enumName: TaggedName<"enum">): string {
    return enumName.inner.combine(member.name.inner).PascalCase()
}


function generateEnumDeclaration(unionEnum: TaggedUnionEnum, member: TaggedMember[]): string {

    const underlyingType: CEnumType | null = resolveUnderlyingType(unionEnum.underlyingType, member.length)

    return `/* @enum value */
typedef enum${underlyingType === null ? "" : ` C_23_NARROW_ENUM_TO(${cTypeForEnum(underlyingType)})`} {
	${member.map((mem, idx) => {

        if (underlyingType === "bool") {
            return `${memberNameForEnum(mem, unionEnum.name)} = ${idx ? "true" : "false"}`
        }

        if (idx == 0) {
            return `${memberNameForEnum(mem, unionEnum.name)} = 0`
        }

        return memberNameForEnum(mem, unionEnum.name)
    }).join(",\n	")}
} ${unionEnum.name.inner.PascalCase()};`


}

function getUnionTagName(name: TaggedName<"union">): string {
    return `_variant_tag_for_${name.inner.snake_case()}_tag_member`
}

function getUnionDataName(name: TaggedName<"union">): string {
    return `_variant_data_for_${name.inner.snake_case()}_data_member`
}


function typeForMember(val: TaggedType, unnamedStructMap: UnnamedStructMap): string {
    if (isSimpleTaggedType(val)) {
        return val.name;
    }

    return getNameForUnnamedStruct(val, unnamedStructMap)


}

type StructInfo = [string, string]

function getStructInfo(taggedUnion: TaggedUnion): StructInfo {
    if (taggedUnion.options.rawStruct) {
        return [`struct ${taggedUnion.options.rawStruct.PascalCase()} `, ""]
    }

    return ["typedef struct ", ` ${taggedUnion.name.inner.PascalCase()}`]
}

function generateVariantDeclaration(taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string {

    const tagName: string = getUnionTagName(taggedUnion.name);

    const dataName: string = getUnionDataName(taggedUnion.name);

    const [structBefore, structAfter] = getStructInfo(taggedUnion)


    return `	/* tagged union (variant) implementation */
${structBefore}{
	${taggedUnion.enum.name.inner.PascalCase()} ${tagName};
	union {
		${taggedUnion.member.filter(mem => mem.type !== null).map((mem) => {

        if (mem.type === null) {
            throw new Error("IMPLEMENTATION ERROR")
        }

        return `${typeForMember(mem.type, unnamedStructMap)} ${mem.name.inner.snake_case()};`.split("\n").join("\n		")

    }).join("\n		")}
	} ${dataName};
}${structAfter};`
}


function generateUnnamedStruct(struct: TaggedTypeStruct, unnamedStructMap: UnnamedStructMap): string {

    return `typedef struct {
	${struct.struct.members.map(mem => {
        return `${mem.typeName} ${mem.name};`
    }).join("\n	")}
} ${getNameForUnnamedStruct(struct, unnamedStructMap)};`
}


function generatePoisonPragma(names: string[]): string {
    return `_Pragma ("GCC poison ${names.join(" ")}")`
}

function functionForNewVariant(mem: TaggedMember, taggedUnion: TaggedUnion): string {

    return `new_${taggedUnion.name.inner.snake_case()}_${mem.name.inner.snake_case()}`

}

const cConst = "const"

function cConstConditional(mutable: boolean): string {
    if (!mutable) {
        return ` ${cConst} `
    }

    return " "
}


function generateNewFunctionForMem(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string {

    if (mem.type === null) {
        return `static inline ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(mem, taggedUnion)}(void){
	return (${taggedUnion.name.inner.PascalCase()}){ .${getUnionTagName(taggedUnion.name)} = ${memberNameForEnum(mem, taggedUnion.enum.name)} };
}
`


    } else if (isSimpleTaggedType(mem.type)) {
        return `static inline ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(mem, taggedUnion)}(${mem.type.name} ${cConst} value){
	return (${taggedUnion.name.inner.PascalCase()}){ .${getUnionTagName(taggedUnion.name)} = ${memberNameForEnum(mem, taggedUnion.enum.name)}, .${getUnionDataName(taggedUnion.name)} = { .${mem.name.inner.snake_case()} = value } };
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(mem.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")


        return `static inline ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(mem, taggedUnion)}(${mem.type.struct.members.map(m => {
            return `${m.typeName} ${cConst} ${m.name}`
        }).join(", ")}){
	return (${taggedUnion.name.inner.PascalCase()}){ .${getUnionTagName(taggedUnion.name)} = ${memberNameForEnum(mem, taggedUnion.enum.name)}, .${getUnionDataName(taggedUnion.name)} = { .${mem.name.inner.snake_case()} = (${getNameForUnnamedStruct(mem.type, unnamedStructMap)}){ ${mem.type.struct.members.map((m): string => {
            return `.${m.name} = ${m.name}`
        }).join(", ")} } } };
}
`


    }

}

function functionForGetAsVariant(mem: TaggedMember, taggedUnion: TaggedUnion): string {
    return `${taggedUnion.name.inner.snake_case()}_get_as_${mem.name.inner.snake_case()}`

}

function functionForGetAsRefVariant(mem: TaggedMember, taggedUnion: TaggedUnion, mutable: boolean): string {
    return `${taggedUnion.name.inner.snake_case()}_get_as_${mem.name.inner.snake_case()}_${mutable ? "mut" : "const"}_ref`

}

function generateGetAsFunctionForMem(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string | null {

    if (mem.type === null) {
        //doesn't make sense, check if it is presnet yes, but not getting the inner content
        return null;



    } else if (isSimpleTaggedType(mem.type)) {
        return `static inline ${mem.type.name} ${functionForGetAsVariant(mem, taggedUnion)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(mem, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()};
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(mem.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        return `static inline ${getNameForUnnamedStruct(mem.type, unnamedStructMap)} ${functionForGetAsVariant(mem, taggedUnion)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(mem, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()};
}
`



    }




}

function generateGetAsRefFunctionForMem(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, mutable: boolean): string | null {

    if (mem.type === null) {
        //doesn't make sense, check if it is presnet yes, but not getting the inner content
        return null;

    } else if (isSimpleTaggedType(mem.type)) {
        return `static inline${cConstConditional(mutable)}${mem.type.name}* ${functionForGetAsRefVariant(mem, taggedUnion, mutable)}(${cConstConditional(mutable)}${taggedUnion.name.inner.PascalCase()}* ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry->${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(mem, taggedUnion.enum.name)});
	return &(variant_entry->${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()});
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(mem.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        return `static inline ${getNameForUnnamedStruct(mem.type, unnamedStructMap)} ${functionForGetAsRefVariant(mem, taggedUnion, mutable)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(mem, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()};
}
`
    }


}

function generateGetAsRefFunctionsForMem(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): (string | null)[] {
    return [generateGetAsRefFunctionForMem(mem, taggedUnion, unnamedStructMap, true), generateGetAsRefFunctionForMem(mem, taggedUnion, unnamedStructMap, false)]
}


function generateMemberFunctionsForMem(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string {

    const functions: string[] = [
        generateNewFunctionForMem(mem, taggedUnion, unnamedStructMap),
        generateGetAsFunctionForMem(mem, taggedUnion, unnamedStructMap),
        ...generateGetAsRefFunctionsForMem(mem, taggedUnion, unnamedStructMap)
    ].filter((fn: string | null): fn is string => fn !== null)


    return functions.join("\n\n");


}

const stateStrFNPrefix = "_impl_get_state_string_for_variant_"


function getStateFunctionName(name: TaggedName<"union">): string {
    return `${stateStrFNPrefix}${name.inner.snake_case()}`
}

function generateFunctions(taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string {

    return `static inline tstr_static ${getStateFunctionName(taggedUnion.name)}(${taggedUnion.enum.name.inner.PascalCase()} ${cConst} enum_value){
	switch(enum_value){
		${taggedUnion.member.map(mem => {
        return `case ${memberNameForEnum(mem, taggedUnion.enum.name)}: {
	return TSTR_STATIC_LIT("${mem.name.inner.snake_case()}");
}`}).join("\n").split("\n").join("\n		")}
		default: {
			return TSTR_STATIC_LIT("<unknown>");
		}
	}
}

${taggedUnion.member.map((mem): string => {

            return generateMemberFunctionsForMem(mem, taggedUnion, unnamedStructMap);

        }).join("\n")}
`

}

function getIfMacroName(unionName: TaggedName<"union">, memberName: TaggedName<"member">, suffix: string | null): string {
    return `IF_${unionName.inner.MACRO_NAME()}_IS_${memberName.inner.MACRO_NAME()}${suffix ? `_${suffix.toUpperCase()}` : ""}`
}

function getIfNotMacroName(unionName: TaggedName<"union">, memberName: TaggedName<"member">): string {
    return `IF_${unionName.inner.MACRO_NAME()}_IS_NOT_${memberName.inner.MACRO_NAME()}`
}


function getSwitchMacroName(unionName: TaggedName<"union">): string {
    return `SWITCH_${unionName.inner.MACRO_NAME()}`
}

function getCaseMacroName(unionName: TaggedName<"union">, memberName: TaggedName<"member">, suffix: string | null): string {
    return `CASE_${unionName.inner.MACRO_NAME()}_IS_${memberName.inner.MACRO_NAME()}${suffix ? `_${suffix.toUpperCase()}` : ""}`
}


function getNameTrickForIfExpression(unionName: TaggedName<"union">): string {
    return `_for_macro_trick_for_if_expr_impl_once_variant_${unionName.inner.snake_case()}_variant_impl`

}

type GeneratorVariant = "const" | "mut" | "ign"

function generateIfMacros(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string[] {

    if (mem.type === null) {

        return [`#define ${getIfMacroName(taggedUnion.name, mem.name, null)}(variant_entry)
	if((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(mem, taggedUnion.enum.name)})`
        ]

    } else if (isSimpleTaggedType(mem.type)) {

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const member: TaggedTypeSimple = mem.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).map((variant): string => {

            let result = `#define ${getIfMacroName(taggedUnion.name, mem.name, variant)}(variant_entry)
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(mem, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                result += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${member.name}${cConstConditional(variant === "mut")}${mem.name.inner.snake_case()} = (variant_entry).${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`
            }

            return result;

        });


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(mem.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const member: TaggedTypeStruct = mem.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).map((variant): string => {

            let result = `#define ${getIfMacroName(taggedUnion.name, mem.name, variant)}(variant_entry)
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(mem, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                result += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${getNameForUnnamedStruct(member, unnamedStructMap)}${cConstConditional(variant === "mut")}${mem.name.inner.snake_case()} = (variant_entry).${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`
            }

            return result;

        });
    }


}

function generateIfNotMacro(mem: TaggedMember, taggedUnion: TaggedUnion): string {

    return `#define ${getIfNotMacroName(taggedUnion.name, mem.name)}(variant_entry)
	if((variant_entry).${getUnionTagName(taggedUnion.name)} != ${memberNameForEnum(mem, taggedUnion.enum.name)})`
}

function generateSwitchMacro(taggedUnion: TaggedUnion): string {

    return `#define ${getSwitchMacroName(taggedUnion.name)}(variant_entry)
	switch((variant_entry).${getUnionTagName(taggedUnion.name)})`
}


function getNameTrickForCaseExpression(unionName: TaggedName<"union">): string {
    return `_for_macro_trick_for_case_expr_impl_once_variant_${unionName.inner.snake_case()}_variant_impl`

}

function generateCaseMacros(mem: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string[] {


    if (mem.type === null) {

        return [`#define ${getCaseMacroName(taggedUnion.name, mem.name, null)}()
	case ${memberNameForEnum(mem, taggedUnion.enum.name)}:`]

    } else if (isSimpleTaggedType(mem.type)) {

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const member: TaggedTypeSimple = mem.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).map((variant): string => {

            let result = `#define ${getCaseMacroName(taggedUnion.name, mem.name, variant)}(variant_entry)
	case ${memberNameForEnum(mem, taggedUnion.enum.name)}:`


            if (variant !== "ign") {
                result +=
                    `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${member.name}${cConstConditional(variant === "mut")}${mem.name.inner.snake_case()} = (variant_entry).${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`
            }

            return result;

        });


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(mem.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const member: TaggedTypeStruct = mem.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).map((variant): string => {

            let result = `#define ${getCaseMacroName(taggedUnion.name, mem.name, variant)}(variant_entry)
	case ${memberNameForEnum(mem, taggedUnion.enum.name)}:`



            if (variant !== "ign") {
                result += `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${getNameForUnnamedStruct(member, unnamedStructMap)}${cConstConditional(variant === "mut")}${mem.name.inner.snake_case()} = (variant_entry).${getUnionDataName(taggedUnion.name)}.${mem.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`
            }

            return result;

        });

    }

}

function toCStr(str: string): string {
    return `"${str}"`
}

function generateNameForUnnamedStruct(unionName: TaggedName<"union">, id: ID, memberName: TaggedName<"member">): string {
    return `_variant_impl_unnamed_struct_for_variant_${unionName.inner.snake_case()}_id_${id.toString()}_name_${memberName.inner.snake_case()}_impl_`
}

type UnnamedStructMap = Record<ID, string | undefined>

function generateUnnamedStructMap(taggedUnion: TaggedUnion): UnnamedStructMap {

    const map: UnnamedStructMap = {}

    for (const member of taggedUnion.member) {
        if (member.type === null) {
            continue;
        }

        if (isSimpleTaggedType(member.type)) {
            continue
        }

        map[member.type.id] = generateNameForUnnamedStruct(taggedUnion.name, member.type.id, member.name)
    }

    return map;

}

function getNameForUnnamedStruct(struct: TaggedTypeStruct, unnamedStructMap: UnnamedStructMap): string {

    const value = unnamedStructMap[struct.id]

    if (value === undefined) {
        throw new Error("IMPLEMENTATION ERROR")
    }

    return value;

}

const genericStateAssert = "VARIANT_STATE_ASSERT"

function getStateAssertNameFor(unionName: TaggedName<"union">): string {
    return `VARIANT_${unionName.inner.MACRO_NAME()}_STATE_ASSERT`
}

function generatedUnionForCHeader(taggedUnion: TaggedUnion): string {

    assert(taggedUnion.member.length >= 2, "at least two member are required")

    assert(taggedUnion.enum.underlyingType !== null, "prefer having enums with underlying type")

    const unnamedStructMap: UnnamedStructMap = generateUnnamedStructMap(taggedUnion);


    const declarations: string[] = [
        ...taggedUnion.member.map((mem): string | null => {
            if (mem.type === null) {
                return null;
            }

            if (isSimpleTaggedType(mem.type)) {
                return null;
            }

            return generateUnnamedStruct(mem.type, unnamedStructMap)

        }).filter(m => m !== null),
        generateVariantDeclaration(taggedUnion, unnamedStructMap)
    ]

    const functionsString: string = generateFunctions(taggedUnion, unnamedStructMap)

    const tagName: string = getUnionTagName(taggedUnion.name);

    const dataName: string = getUnionDataName(taggedUnion.name);

    const generateMacroEnum = `#define GENERATE_VARIANT_ENUM_${taggedUnion.name.inner.MACRO_NAME()}()
	${[generateEnumDeclaration(taggedUnion.enum, taggedUnion.member)].map(decl => decl.split("\n").join("\n	")).join("\n	\n")}`


    const generateMacroCore = `#define GENERATE_VARIANT_CORE_${taggedUnion.name.inner.MACRO_NAME()}()
	${declarations.map(decl => decl.split("\n").join("\n	")).join("\n	\n")}
	
	${functionsString.split("\n").join("\n	")}
	
	${generatePoisonPragma([getStateFunctionName(taggedUnion.name)])}`

    const generateMacroAll = `#define GENERATE_VARIANT_ALL_${taggedUnion.name.inner.MACRO_NAME()}()
	GENERATE_VARIANT_ENUM_${taggedUnion.name.inner.MACRO_NAME()}()
	GENERATE_VARIANT_CORE_${taggedUnion.name.inner.MACRO_NAME()}()`

    const macros: string[] = [
        generateMacroEnum,
        generateMacroCore,
        generateMacroAll,
        ...taggedUnion.member.flatMap((mem): string[] => generateIfMacros(mem, taggedUnion, unnamedStructMap)),
        ...taggedUnion.member.map((mem): string => generateIfNotMacro(mem, taggedUnion)),
        generateSwitchMacro(taggedUnion),
        ...taggedUnion.member.flatMap((mem): string[] => generateCaseMacros(mem, taggedUnion, unnamedStructMap)),
    ]


    return (
        `#define ${getStateAssertNameFor(taggedUnion.name)}(state, expected_state) ${genericStateAssert}(state, expected_state, ${taggedUnion.name.inner.snake_case()}, ${toCStr(taggedUnion.name.inner.PascalCase())
        })
	
${macros.map(a => a.split("\n").join(" \\\n")).join("\n\n")}

${generatePoisonPragma([tagName, dataName,
            getNameTrickForIfExpression(taggedUnion.name),
            getNameTrickForCaseExpression(taggedUnion.name),
            ...Object.values(unnamedStructMap).map((val): string => {
                // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
                return val!
            })])
        }
`)


}


const globalTaggedUnions: TaggedUnion[] = [
    {
        name: makeUnionName(CaseName.fromPascalCase("AccountInfo")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Empty")),
                type: null
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("OnlyUser")),
                type: makeStructType([
                    makeStructMember(
                        "username",
                        "tstr"
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeSimpleType("AccountOkData")
            }
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("AccountState")),
            underlyingType: "u8"
        },
        options: {}
    },
    {
        name: makeUnionName(CaseName.fromParts(["IP", "address"])),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("V4")),
                type: makeSimpleType("IPV4Address")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("V6")),
                type: makeSimpleType("IPV6Address")
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromParts(["IP", "protocol", "version"])),
            underlyingType: "u8"
        },
        options: {}
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("AuthenticationProvider")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Simple")),
                type: makeSimpleType("SimpleAuthenticationProviderData")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("System")),
                type: null,
            }
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("AuthenticationProviderType")),
            underlyingType: "u8"
        },
        options: {
            rawStruct: CaseName.fromPascalCase("AuthenticationProviderImpl")
        }
    },
]


export async function generateVariantCodeC(generatedVariantsFileH: string): Promise<void> {

    const tasks: Promise<void>[] = []

    assert(path.extname(generatedVariantsFileH) == ".h", "variant file has to end in .h")

    const headerPreamble = `#define ${genericStateAssert}(state, expected_state, variant_name, VariantName)
do {
	if ((state) != (expected_state)) {
		tstr_static ${cConst} state_str = ${stateStrFNPrefix}##variant_name(state);
		tstr_static ${cConst} expected_state_str = ${stateStrFNPrefix}##variant_name(expected_state);
		fprintf(stderr,
			"[%s %s:%d]: Invalid variant access for variant '%s': state was " TSTR_FMT
				" but expected " TSTR_FMT "\\n",
			__func__, __FILE__, __LINE__, VariantName, TSTR_STATIC_FMT_ARGS(state_str),
			TSTR_STATIC_FMT_ARGS(expected_state_str));
		UNREACHABLE();
	}
} while (false)
`

    const headerData = `
#pragma once

#include <tstr.h>

${headerPreamble.split("\n").join(" \\\n")}


${globalTaggedUnions.map(un => generatedUnionForCHeader(un)).join("\n\n")}


`

    tasks.push(writeFileAndDirs(generatedVariantsFileH, headerData))

    await Promise.all(tasks)


}
