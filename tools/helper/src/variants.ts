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
    typeName: CType
    name: string,
}

function makeStructMember(typeName: CType, name: string): StructMember {
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

type StructOrder = "auto" | "tag_first" | "tag_second"

interface TaggedUnionOptions {
    rawStruct?: CaseName | undefined
    structOrder?: StructOrder | undefined
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

function bestEnumTypeForLength(len: number): CEnumType {
    if (len < maxLengthForCEnumType("bool")) {
        throw new Error("Should not use tagged unions in this case")
    }

    if (len == maxLengthForCEnumType("bool")) {
        return "bool"
    }

    if (len <= maxLengthForCEnumType("u8")) {
        return "u8"
    }

    if (len <= maxLengthForCEnumType("u16")) {
        return "u16"
    }

    if (len <= maxLengthForCEnumType("u32")) {
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

function resolveUnderlyingEnumType(original: CEnumType | "best_match" | null, memberLen: number): CEnumType | null {

    if (original === "best_match") {
        return bestEnumTypeForLength(memberLen)
    }

    return original

}

function maxLengthForCEnumType(tpe: CEnumType): number {
    switch (tpe) {
        case "bool": {
            return 2;
        }
        case "u8": {
            return (1 << 8) - 1;
        }
        case "u16": {
            return (1 << 16) - 1
        }
        case "u32": {
            // note 1 << 32 want work, as it uses int32_t math, and thus that is wrong
            return (0xFFFFFFFF) - 1;
        }
        case "u64": {
            throw new Error("Unimplemented, length is huuuuuuge")

        }
        default: {
            throw new Error("Unimplemented")
        }
    }
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

    const underlyingType: CEnumType | null = resolveUnderlyingEnumType(unionEnum.underlyingType, member.length)

    if (underlyingType !== null && maxLengthForCEnumType(underlyingType) < member.length) {
        throw new Error(`Underlying enum type ${underlyingType} can#t fit ${member.length.toString()} members!`)
    }

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
	union {
		${taggedUnion.member.filter(mem => mem.type !== null).map((mem) => {

        if (mem.type === null) {
            throw new Error("IMPLEMENTATION ERROR")
        }

        return `${typeForMember(mem.type, unnamedStructMap)} ${mem.name.inner.snake_case()};`.split("\n").join("\n		")

    }).join("\n		")}
	} ${dataName};
	${taggedUnion.enum.name.inner.PascalCase()} ${tagName};
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

function functionForNewVariant(member: TaggedMember, taggedUnion: TaggedUnion): string {

    return `new_${taggedUnion.name.inner.snake_case()}_${member.name.inner.snake_case()}`

}

const cConst = "const"

function cConstConditional(mutable: boolean): string {
    if (!mutable) {
        return ` ${cConst} `
    }

    return " "
}

const inlineFunctionSpecifiers = "NODISCARD MAYBE_UNUSED static inline"

interface StructValue { name: string, value: string }

type StructValues = [first: StructValue, second: StructValue]

interface StructOrderResolved {
    first: string
    second: string
}

function structOrderFor(unionName: TaggedName<"union">, what: Exclude<StructOrder, "auto">): StructOrderResolved {

    const tagName = getUnionTagName(unionName)
    const dataName = getUnionDataName(unionName)

    if (what === "tag_first") {
        return { first: tagName, second: dataName }
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (what === "tag_second") {
        return { first: dataName, second: tagName }
    } else {
        throw new Error(`Unknown struct order string: ${what as string}`)
    }

}

function resolveStructOrder(taggedUnion: TaggedUnion): StructOrderResolved {

    if (taggedUnion.options.structOrder !== undefined) {
        if (taggedUnion.options.structOrder !== "auto") {
            return structOrderFor(taggedUnion.name, taggedUnion.options.structOrder)
        }
    }

    // use auto
    //TODO: use better heuristic
    return structOrderFor(taggedUnion.name, "tag_first")

}

function sortStructValues(structOrder: StructOrderResolved, values: StructValues): StructValues {
    if (values[0].name === structOrder.first) {
        assert(values[1].name === structOrder.second, "wrong struct member names")
        return [values[0], values[1]]
    }

    assert(values[1].name === structOrder.first, "wrong struct member names")
    assert(values[0].name === structOrder.second, "wrong struct member names")
    return [values[1], values[0]]
}

function initializeStruct(structOrder: StructOrderResolved, values: StructValues): string {

    const sortedValues = sortStructValues(structOrder, values)

    return sortedValues.map(({ name, value }): string => {
        return `.${name} = ${value}`
    }).join(", ")
}


function generateNewFunctionForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, structOrder: StructOrderResolved): string {

    if (member.type === null) {
        return `${inlineFunctionSpecifiers} ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(member, taggedUnion)}(void){
	return (${taggedUnion.name.inner.PascalCase()}){ ${initializeStruct(structOrder, [
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name) },
            { name: getUnionDataName(taggedUnion.name), value: "{ }" }
        ])} };
}
`


    } else if (isSimpleTaggedType(member.type)) {
        return `${inlineFunctionSpecifiers} ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(member, taggedUnion)}(${member.type.name} ${cConst} value){
	return (${taggedUnion.name.inner.PascalCase()}){ ${initializeStruct(structOrder, [
            { name: getUnionDataName(taggedUnion.name), value: `{ .${member.name.inner.snake_case()} = value }` },
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name) }
        ])} };
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")


        return `${inlineFunctionSpecifiers} ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(member, taggedUnion)}(${member.type.struct.members.map(m => {
            return `${m.typeName} ${cConst} ${m.name}`
        }).join(", ")}){
	return (${taggedUnion.name.inner.PascalCase()}){ ${initializeStruct(structOrder, [
            {
                name: getUnionDataName(taggedUnion.name), value: `{ .${member.name.inner.snake_case()} = (${getNameForUnnamedStruct(member.type, unnamedStructMap)}){ ${member.type.struct.members.map((m): string => {
                    return `.${m.name} = ${m.name}`
                }).join(", ")} } }`
            },
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name) }
        ])} };
}
`


    }

}

function functionForGetAsVariant(member: TaggedMember, taggedUnion: TaggedUnion): string {
    return `${taggedUnion.name.inner.snake_case()}_get_as_${member.name.inner.snake_case()}`

}

function functionForGetAsRefVariant(member: TaggedMember, taggedUnion: TaggedUnion, mutable: boolean): string {
    return `${taggedUnion.name.inner.snake_case()}_get_as_${member.name.inner.snake_case()}_${mutable ? "mut" : "const"}_ref`

}

function generateGetAsFunctionForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string | null {

    if (member.type === null) {
        //doesn't make sense, check if it is presnet yes, but not getting the inner content
        return null;



    } else if (isSimpleTaggedType(member.type)) {
        return `${inlineFunctionSpecifiers} ${member.type.name} ${functionForGetAsVariant(member, taggedUnion)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()};
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        return `${inlineFunctionSpecifiers} ${getNameForUnnamedStruct(member.type, unnamedStructMap)} ${functionForGetAsVariant(member, taggedUnion)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()};
}
`



    }




}


function generateGetAsRefFunctionForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, mutable: boolean): string | null {

    if (member.type === null) {
        //doesn't make sense, check if it is presnet yes, but not getting the inner content
        return null;

    } else if (isSimpleTaggedType(member.type)) {
        return `${inlineFunctionSpecifiers}${cConstConditional(mutable)}${member.type.name}* ${functionForGetAsRefVariant(member, taggedUnion, mutable)}(${cConstConditional(mutable)}${taggedUnion.name.inner.PascalCase()}* ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry->${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return &(variant_entry->${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()});
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        return `${inlineFunctionSpecifiers} ${getNameForUnnamedStruct(member.type, unnamedStructMap)} ${functionForGetAsRefVariant(member, taggedUnion, mutable)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry.${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return variant_entry.${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()};
}
`
    }


}

function generateGetAsRefFunctionsForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): (string | null)[] {
    return [generateGetAsRefFunctionForMember(member, taggedUnion, unnamedStructMap, true), generateGetAsRefFunctionForMember(member, taggedUnion, unnamedStructMap, false)]
}


function generateMemberFunctionsForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, structOrder: StructOrderResolved): string {

    const functions: string[] = [
        generateNewFunctionForMember(member, taggedUnion, unnamedStructMap, structOrder),
        generateGetAsFunctionForMember(member, taggedUnion, unnamedStructMap),
        ...generateGetAsRefFunctionsForMember(member, taggedUnion, unnamedStructMap)
    ].filter((fn: string | null): fn is string => fn !== null)


    return functions.join("\n\n");


}

const stateStrFNPrefix = "_impl_get_state_string_for_variant_"


function getStateFunctionName(name: TaggedName<"union">): string {
    return `${stateStrFNPrefix}${name.inner.snake_case()}`
}

function getTagTypeFunctionName(name: TaggedName<"union">): string {
    return `get_current_tag_type_for_${name.inner.snake_case()}`
}

function generateFunctions(taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, structOrder: StructOrderResolved): string {

    return `${inlineFunctionSpecifiers} tstr_static ${getStateFunctionName(taggedUnion.name)}(${taggedUnion.enum.name.inner.PascalCase()} ${cConst} enum_value){
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

${inlineFunctionSpecifiers} ${taggedUnion.enum.name.inner.PascalCase()} ${getTagTypeFunctionName(taggedUnion.name)}(${taggedUnion.name.inner.PascalCase()} ${cConst} variant_entry){
	return variant_entry.${getUnionTagName(taggedUnion.name)};
}

${taggedUnion.member.map((mem): string => {

            return generateMemberFunctionsForMember(mem, taggedUnion, unnamedStructMap, structOrder);

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

function generateIfMacros(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string[] {

    if (member.type === null) {

        return [`#define ${getIfMacroName(taggedUnion.name, member.name, null)}(variant_entry)
	if((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`
        ]

    } else if (isSimpleTaggedType(member.type)) {

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const memberType: TaggedTypeSimple = member.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).flatMap((variant): string[] => {

            const results: string[] = []

            const mainName: string = getIfMacroName(taggedUnion.name, member.name, variant === "ign" ? variant : `${variant}_IMPL_2`)

            let mainDef = `#define ${mainName}(variant_entry${variant !== "ign" ? `, var_name` : ""})
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${memberType.name}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`
                // macro alias for not using var_name

                const helperName = getIfMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`);

                const defaultArgName = getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`);

                results.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getIfMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])
            }

            results.push(mainDef)

            return results;

        });


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const memberType: TaggedTypeStruct = member.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).flatMap((variant): string[] => {

            const results: string[] = []

            const mainName: string = getIfMacroName(taggedUnion.name, member.name, variant === "ign" ? variant : `${variant}_IMPL_2`)

            let mainDef = `#define ${mainName}(variant_entry${variant !== "ign" ? `, var_name` : ""})
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${getNameForUnnamedStruct(memberType, unnamedStructMap)}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`

                // macro alias for not using var_name

                const helperName = getIfMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`);

                const defaultArgName = getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`);

                results.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getIfMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])

            }

            results.push(mainDef)

            return results;

        });
    }


}

function generateIfNotMacro(member: TaggedMember, taggedUnion: TaggedUnion): string {

    return `#define ${getIfNotMacroName(taggedUnion.name, member.name)}(variant_entry)
	if((variant_entry).${getUnionTagName(taggedUnion.name)} != ${memberNameForEnum(member, taggedUnion.enum.name)})`
}

function generateSwitchMacro(taggedUnion: TaggedUnion): string {

    return `#define ${getSwitchMacroName(taggedUnion.name)}(variant_entry)
	switch((variant_entry).${getUnionTagName(taggedUnion.name)})`
}


function getNameTrickForCaseExpression(unionName: TaggedName<"union">): string {
    return `_for_macro_trick_for_case_expr_impl_once_variant_${unionName.inner.snake_case()}_variant_impl`

}

function generateCaseMacros(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): string[] {


    if (member.type === null) {

        return [`#define ${getCaseMacroName(taggedUnion.name, member.name, null)}()
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`]

    } else if (isSimpleTaggedType(member.type)) {

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const memberType: TaggedTypeSimple = member.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).flatMap((variant): string[] => {

            const results: string[] = []

            const mainName: string = getCaseMacroName(taggedUnion.name, member.name, variant === "ign" ? variant : `${variant}_IMPL_2`)

            let mainDef = `#define ${mainName}(${variant !== "ign" ? "variant_entry, var_name" : ""})
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`


            if (variant !== "ign") {
                mainDef +=
                    `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${memberType.name}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`

                // macro alias for not using var_name

                const helperName = getCaseMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`);

                const defaultArgName = getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`);

                results.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getCaseMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])
            }

            results.push(mainDef)

            return results;

        });


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const memberType: TaggedTypeStruct = member.type

        return (["const", "mut", "ign"] as GeneratorVariant[]).flatMap((variant): string[] => {

            const results: string[] = []

            const mainName: string = getCaseMacroName(taggedUnion.name, member.name, variant === "ign" ? variant : `${variant}_IMPL_2`)

            let mainDef = `#define ${mainName}(${variant !== "ign" ? "variant_entry, var_name" : ""})
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`



            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${getNameForUnnamedStruct(memberType, unnamedStructMap)}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`

                // macro alias for not using var_name

                const helperName = getCaseMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`);

                const defaultArgName = getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`);

                results.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getCaseMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])
            }

            results.push(mainDef)

            return results;

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

    const structOrder: StructOrderResolved = resolveStructOrder(taggedUnion)

    const functionsString: string = generateFunctions(taggedUnion, unnamedStructMap, structOrder)

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
                        "tstr",
                        "username",
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
    {
        name: makeUnionName(CaseName.fromPascalCase("AuthenticationFindResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("NoSuchUser")),
                type: null
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("WrongPassword")),
                type: null
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeSimpleType("AuthUserWithContext")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeStructType([
                    makeStructMember(
                        "const char*",
                        "message",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("AuthenticationValidity")),
            underlyingType: "u8"
        },
        options: {}
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("ConnectionTypeIdentifier")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Active")),
                type: makeStructType([
                    makeStructMember(
                        "FTPConnectAddr",
                        "addr",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Passive")),
                type: makeStructType([
                    makeStructMember(
                        "FTPPortField",
                        "port",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Automatic")),
                type: null
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("ConnectionTypeIdentifierEnumType")),
            underlyingType: "best_match"
        },
        options: {}
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("StaticTableFindResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("NotFound")),
                type: null,
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("KeyFound")),
                type: makeStructType([
                    makeStructMember(
                        "size_t",
                        "index",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("AllFound")),
                type: makeStructType([
                    makeStructMember(
                        "size_t",
                        "index",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("StaticTableFindResultType")),
            underlyingType: "u8"
        },
        options: {}
    },
]


export async function generateVariantCodeC(generatedVariantsFileH: string): Promise<void> {

    const tasks: Promise<void>[] = []

    assert(path.extname(generatedVariantsFileH) == ".h", "variant file has to end in .h")

    const globalMacros: string[] = [
        `#define UNREACHABLE_WITH_MESSAGE(msg, ...) do { 
	fprintf(stderr,
		"[%s %s:%d]: UNREACHABLE: " msg "\\n",
		__func__, __FILE__, __LINE__, __VA_ARGS__
	);
		UNREACHABLE();
} while (false)`,
        // 
        `#define UNREACHABLE_WITH_MESSAGE_SINGLE(msg) UNREACHABLE_WITH_MESSAGE(msg "%s", "")`,
        // 
        `#define ${genericStateAssert}(state, expected_state, variant_name, VariantName)
do {
	if ((state) != (expected_state)) {
		tstr_static ${cConst} state_str = ${stateStrFNPrefix}##variant_name(state);
		tstr_static ${cConst} expected_state_str = ${stateStrFNPrefix}##variant_name(expected_state);
		UNREACHABLE_WITH_MESSAGE("Invalid variant access for variant '%s': state was " TSTR_FMT
				" but expected " TSTR_FMT, VariantName, TSTR_STATIC_FMT_ARGS(state_str),
			TSTR_STATIC_FMT_ARGS(expected_state_str));
	}
} while (false)`,
        //
        `#define VARIANT_CASE_END()
	UNREACHABLE_WITH_MESSAGE_SINGLE("macro trick with for loops for getting the value was implemented wrong")`
    ]

    const headerData = `
#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <tstr.h>

${globalMacros.map(m => m.split("\n").join(" \\\n")).join("\n\n")}


${globalTaggedUnions.map(un => generatedUnionForCHeader(un)).join("\n\n")}

#ifdef __cplusplus
}
#endif

`

    tasks.push(writeFileAndDirs(generatedVariantsFileH, headerData))

    await Promise.all(tasks)


}
