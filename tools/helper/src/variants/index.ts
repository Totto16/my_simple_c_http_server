import path from "node:path"
import { addGenerateMacros, assert, writeFileAndDirs } from "../utils.js"
import { CaseName, getBrand, isSimpleTaggedType, type CEnumType, type CppFeatures, type ID, type StructOrder, type TaggedMember, type TaggedName, type TaggedType, type TaggedTypeSimple, type TaggedTypeStruct, type TaggedUnion, type TaggedUnionEnum } from "./base.js"
import { getGlobalTaggedUnions } from "./data.js"

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

function unionNameFor(name: TaggedName<"union">): string {
    return `_AnonymousUnionForVariant${name.inner.PascalCase()}Impl_DONT_USE`
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
        return [`struct ${taggedUnion.options.rawStruct.inner.PascalCase()} `, ""]
    }

    return ["typedef struct ", ` ${taggedUnion.name.inner.PascalCase()}`]
}

function getStaticAssertForAlignedAccess(taggedUnion: TaggedUnion): string[] {
    return [`static_assert(false, "TODO: ${taggedUnion.name.inner.PascalCase()}");`]
}


type WhichAssert = "1" | "2"

function getAssertStructName(unionName: TaggedName<"union">, which: WhichAssert): string {
    return `AssertTypeImplFor${unionName.inner.PascalCase()}Impl_${which}_DONT_USE_`
}

function inverseStructOrder(structOrder: StructOrderResolved): StructOrderResolved {
    // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    if (structOrder.first === "tag" && structOrder.second === "data") {
        return { first: "data", second: "tag" }
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (structOrder.first === "data" && structOrder.second === "tag") {
        return { first: "tag", second: "data" }
    } else {
        throw new Error("UNREACHABLE")
    }
}

function getStaticAssertForBestSize(taggedUnion: TaggedUnion, structOrder: StructOrderResolved): string[] {


    const tagName: string = getUnionTagName(taggedUnion.name);

    const dataName: string = getUnionDataName(taggedUnion.name);


    return `typedef struct {
	${generateStructType(structOrder, [
        { name: dataName, value: unionNameFor(taggedUnion.name), type: "data" },
        { name: tagName, value: taggedUnion.enum.name.inner.PascalCase(), type: "tag" }
    ])}
} ${getAssertStructName(taggedUnion.name, "1")};
typedef struct {
	${generateStructType(inverseStructOrder(structOrder), [
        { name: dataName, value: unionNameFor(taggedUnion.name), type: "data" },
        { name: tagName, value: taggedUnion.enum.name.inner.PascalCase(), type: "tag" }
    ])}
} ${getAssertStructName(taggedUnion.name, "2")};
static_assert(sizeof(${getAssertStructName(taggedUnion.name, "1")}) <= sizeof(${getAssertStructName(taggedUnion.name, "2")}), "Size for variant ${taggedUnion.name.inner.PascalCase()} not smaller as the inverted order, current order: ${structOrder.first} is first");`.split("\n")
}

function generateStaticAsserts(taggedUnion: TaggedUnion, structOrder: StructOrderResolved): string {
    const requirements = taggedUnion.options.requirements;

    if (requirements === undefined) {
        return ""
    }

    if (requirements.order === undefined) {
        return ""
    }

    let result = '\n	'

    if (requirements.order === "aligned_access") {
        result += getStaticAssertForAlignedAccess(taggedUnion).join('\n	');
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (requirements.order === "best_size") {
        result += getStaticAssertForBestSize(taggedUnion, structOrder).join('\n	');
    } else {
        throw new Error(`Unrecognized order requirement: ${requirements.order as string}`)
    }

    return result;

}

function generateVariantDeclaration(taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, structOrder: StructOrderResolved): string {

    const tagName: string = getUnionTagName(taggedUnion.name);

    const dataName: string = getUnionDataName(taggedUnion.name);

    const [structBefore, structAfter] = getStructInfo(taggedUnion)

    const generatedStaticAsserts = generateStaticAsserts(taggedUnion, structOrder)

    return `	/* raw union for variant */
	typedef union {
		${taggedUnion.member.filter(mem => mem.type !== null).map((mem) => {

        if (mem.type === null) {
            throw new Error("IMPLEMENTATION ERROR")
        }

        return `${typeForMember(mem.type, unnamedStructMap)} ${mem.name.inner.snake_case()};`.split("\n").join("\n		")

    }).join("\n		")}
	} ${unionNameFor(taggedUnion.name)};

	/* tagged union (variant) implementation */
${structBefore}{
	${generateStructType(structOrder, [
        { name: dataName, value: unionNameFor(taggedUnion.name), type: "data" },
        { name: tagName, value: taggedUnion.enum.name.inner.PascalCase(), type: "tag" }
    ])}
}${structAfter};${generatedStaticAsserts}`
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

type StructValueType = "tag" | "data"

interface StructValue { name: string, value: string, type: StructValueType }

type StructValues = [first: StructValue, second: StructValue]


type StructOrderResolved = {
    first: "tag"
    second: "data"
} | {
    first: "data"
    second: "tag"
}

function structOrderFor(what: Exclude<StructOrder, "auto">): StructOrderResolved {

    if (what === "tag_first") {
        return { first: "tag", second: "data" }
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
    } else if (what === "tag_second") {
        return { first: "data", second: "tag" }
    } else {
        throw new Error(`Unknown struct order string: ${what as string}`)
    }

}

function resolveStructOrder(taggedUnion: TaggedUnion): StructOrderResolved {

    if (taggedUnion.options.structOrder !== undefined) {
        if (taggedUnion.options.structOrder !== "auto") {
            return structOrderFor(taggedUnion.options.structOrder)
        }
    }

    // use auto
    //TODO: use better heuristic
    return structOrderFor("tag_first")

}

function sortStructValues(structOrder: StructOrderResolved, values: StructValues): StructValues {
    if (values[0].type === structOrder.first) {
        assert(values[1].type === structOrder.second, "wrong struct member types")
        return [values[0], values[1]]
    }

    assert(values[1].type === structOrder.first, "wrong struct member types")
    assert(values[0].type === structOrder.second, "wrong struct member types")
    return [values[1], values[0]]
}

function initializeStruct(structOrder: StructOrderResolved, values: StructValues): string {

    const sortedValues = sortStructValues(structOrder, values)

    return sortedValues.map(({ name, value }): string => {
        return `.${name} = ${value}`
    }).join(", ")
}


function generateStructType(structOrder: StructOrderResolved, values: StructValues): string {

    const sortedValues = sortStructValues(structOrder, values)

    return sortedValues.map(({ name, value }): string => {
        return `${value} ${name};`
    }).join("\n	")
}



function generateNewFunctionForMember(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap, structOrder: StructOrderResolved): string {

    if (member.type === null) {
        return `${inlineFunctionSpecifiers} ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(member, taggedUnion)}(void){
	return (${taggedUnion.name.inner.PascalCase()}){ ${initializeStruct(structOrder, [
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name), type: "tag" },
            { name: getUnionDataName(taggedUnion.name), value: `(${unionNameFor(taggedUnion.name)}){ }`, "type": "data" }
        ])} };
}
`


    } else if (isSimpleTaggedType(member.type)) {
        return `${inlineFunctionSpecifiers} ${taggedUnion.name.inner.PascalCase()} ${functionForNewVariant(member, taggedUnion)}(${member.type.name} ${cConst} value){
	return (${taggedUnion.name.inner.PascalCase()}){ ${initializeStruct(structOrder, [
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name), type: "tag" },
            { name: getUnionDataName(taggedUnion.name), value: `(${unionNameFor(taggedUnion.name)}){ .${member.name.inner.snake_case()} = value }`, type: "data" },
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
            { name: getUnionTagName(taggedUnion.name), value: memberNameForEnum(member, taggedUnion.enum.name), type: "tag" },
            {
                name: getUnionDataName(taggedUnion.name), value: `(${unionNameFor(taggedUnion.name)}){ .${member.name.inner.snake_case()} = (${getNameForUnnamedStruct(member.type, unnamedStructMap)}){ ${member.type.struct.members.map((m): string => {
                    return `.${m.name} = ${m.name}`
                }).join(", ")} } }`
                , type: "data"
            },

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
        return `${inlineFunctionSpecifiers} ${member.type.name}${cConstConditional(mutable)}* ${functionForGetAsRefVariant(member, taggedUnion, mutable)}(${cConstConditional(mutable)}${taggedUnion.name.inner.PascalCase()}* ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry->${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return &(variant_entry->${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()});
}
`

    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        return `${inlineFunctionSpecifiers}${cConstConditional(mutable)}${getNameForUnnamedStruct(member.type, unnamedStructMap)}* ${functionForGetAsRefVariant(member, taggedUnion, mutable)}(${cConstConditional(mutable)}${taggedUnion.name.inner.PascalCase()}* ${cConst} variant_entry){
	${getStateAssertNameFor(taggedUnion.name)}(variant_entry->${getUnionTagName(taggedUnion.name)}, ${memberNameForEnum(member, taggedUnion.enum.name)});
	return &(variant_entry->${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()});
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
			_Pragma ("GCC diagnostic push") _Pragma ("GCC diagnostic ignored \\"-Wswitch-bool\\"") /* FOR GCC */
	switch(enum_value){
			_Pragma ("GCC diagnostic pop")
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

interface GeneratedMacros {
    macros: string[]
}

function combineGeneratedMacros(macros: GeneratedMacros[]): GeneratedMacros {
    return macros.reduce<GeneratedMacros>((acc: GeneratedMacros, elem: GeneratedMacros): GeneratedMacros => {
        acc.macros.push(...elem.macros)
        return acc;
    }, { macros: [] });
}

function generateIfMacros(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): GeneratedMacros {

    if (member.type === null) {

        return {
            macros: [`#define ${getIfMacroName(taggedUnion.name, member.name, null)}(variant_entry)
	if((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`
            ]
        }

    } else if (isSimpleTaggedType(member.type)) {

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const memberType: TaggedTypeSimple = member.type

        return combineGeneratedMacros((["const", "mut", "ign"] as GeneratorVariant[]).map((variant): GeneratedMacros => {

            const result: GeneratedMacros = { macros: [] }

            const mainName: string = variant === "ign" ? getIfMacroName(taggedUnion.name, member.name, variant) : `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_2`)}`

            let mainDef = `#define ${mainName}(variant_entry${variant !== "ign" ? `, var_name` : ""})
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${memberType.name}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`
                // macro alias for not using var_name

                const helperName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`)}`;

                const defaultArgName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`)}`;

                result.macros.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getIfMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])

            }

            result.macros.push(mainDef)

            return result;

        }));


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForIfExpression: string = getNameTrickForIfExpression(taggedUnion.name)

        const memberType: TaggedTypeStruct = member.type

        return combineGeneratedMacros((["const", "mut", "ign"] as GeneratorVariant[]).map((variant): GeneratedMacros => {

            const result: GeneratedMacros = { macros: [] }

            const mainName: string = variant === "ign" ? getIfMacroName(taggedUnion.name, member.name, variant) : `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_2`)}`

            let mainDef = `#define ${mainName}(variant_entry${variant !== "ign" ? `, var_name` : ""})
	if ((variant_entry).${getUnionTagName(taggedUnion.name)} == ${memberNameForEnum(member, taggedUnion.enum.name)})`

            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForIfExpression} = true; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)
			for (${getNameForUnnamedStruct(memberType, unnamedStructMap)}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForIfExpression}; ${nameTrickForIfExpression} = false)`

                // macro alias for not using var_name

                const helperName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`)}`;

                const defaultArgName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getIfMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`)}`;

                result.macros.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getIfMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`
                ])

            }

            result.macros.push(mainDef)

            return result;

        }));
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

function generateCaseMacros(member: TaggedMember, taggedUnion: TaggedUnion, unnamedStructMap: UnnamedStructMap): GeneratedMacros {


    if (member.type === null) {


        return {
            macros: [`#define ${getCaseMacroName(taggedUnion.name, member.name, null)}()
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`],

        }

    } else if (isSimpleTaggedType(member.type)) {

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const memberType: TaggedTypeSimple = member.type

        return combineGeneratedMacros((["const", "mut", "ign"] as GeneratorVariant[]).map((variant): GeneratedMacros => {

            const result: GeneratedMacros = { macros: [] }

            const mainName: string = variant === "ign" ? getCaseMacroName(taggedUnion.name, member.name, variant) : `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_2`)}`

            let mainDef = `#define ${mainName}(${variant !== "ign" ? "variant_entry, var_name" : ""})
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`


            if (variant !== "ign") {
                mainDef +=
                    `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${memberType.name}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`

                // macro alias for not using var_name


                const helperName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`)}`;

                const defaultArgName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`)}`;


                result.macros.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getCaseMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])

            }

            result.macros.push(mainDef)

            return result;

        }));


    } else {
        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        assert(getBrand(member.type.struct) === "c_anonymous_struct", "IMPLEMENTATION ERROR")

        const nameTrickForCaseExpression: string = getNameTrickForCaseExpression(taggedUnion.name)

        const memberType: TaggedTypeStruct = member.type

        return combineGeneratedMacros((["const", "mut", "ign"] as GeneratorVariant[]).map((variant): GeneratedMacros => {

            const result: GeneratedMacros = { macros: [] }

            const mainName: string = variant === "ign" ? getCaseMacroName(taggedUnion.name, member.name, variant) : `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_2`)}`

            let mainDef = `#define ${mainName}(${variant !== "ign" ? "variant_entry, var_name" : ""})
	case ${memberNameForEnum(member, taggedUnion.enum.name)}:`



            if (variant !== "ign") {
                mainDef += `		for (bool ${nameTrickForCaseExpression} = true; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)
			for (${getNameForUnnamedStruct(memberType, unnamedStructMap)}${cConstConditional(variant === "mut")}var_name = (variant_entry).${getUnionDataName(taggedUnion.name)}.${member.name.inner.snake_case()}; ${nameTrickForCaseExpression}; ${nameTrickForCaseExpression} = false)`

                // macro alias for not using var_name

                const helperName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_MACRO_HELPER_IMPL_`)}`;

                const defaultArgName = `__INTERNAL_HELPER_MACRO_DONT_USE_${getCaseMacroName(taggedUnion.name, member.name, `${variant}_IMPL_1`)}`;


                result.macros.push(...[
                    `#define ${helperName}(_1, _2, NAME, ...) NAME`,
                    `#define ${defaultArgName}(variant_entry) ${mainName}(variant_entry, ${member.name.inner.snake_case()})`,
                    `#define ${getCaseMacroName(taggedUnion.name, member.name, variant)}(...)
	${helperName}(__VA_ARGS__, ${mainName}, ${defaultArgName})(__VA_ARGS__)`

                ])

            }

            result.macros.push(mainDef)

            return result;

        }));

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

function getVariantDeclarationMacro(end: boolean): string {
    return `__INTERNAL_VARIANT_DECLARATION_${end ? "END" : "START"}_IMPL_`
}

function generatedUnionForCHeader(taggedUnion: TaggedUnion): string {

    assert(taggedUnion.member.length >= 2, "at least two member are required")

    assert(taggedUnion.enum.underlyingType !== null, "prefer having enums with underlying type")

    const unnamedStructMap: UnnamedStructMap = generateUnnamedStructMap(taggedUnion);

    const structOrder: StructOrderResolved = resolveStructOrder(taggedUnion)

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
        generateVariantDeclaration(taggedUnion, unnamedStructMap, structOrder)
    ]

    const functionsString: string = generateFunctions(taggedUnion, unnamedStructMap, structOrder)

    const tagName: string = getUnionTagName(taggedUnion.name);

    const dataName: string = getUnionDataName(taggedUnion.name);

    const generateMacroEnum = `#define GENERATE_VARIANT_ENUM_${taggedUnion.name.inner.MACRO_NAME()}()
	${[generateEnumDeclaration(taggedUnion.enum, taggedUnion.member)].map(decl => decl.split("\n").join("\n	")).join("\n	\n")}`


    const generateMacroCore = `#define GENERATE_VARIANT_CORE_${taggedUnion.name.inner.MACRO_NAME()}()
	${getVariantDeclarationMacro(false)}()
	${declarations.map(decl => decl.split("\n").join("\n	")).join("\n	\n")}
	
	${functionsString.split("\n").join("\n	")}
	
	${generatePoisonPragma([getStateFunctionName(taggedUnion.name)])}
	${getVariantDeclarationMacro(true)}()`

    const generateMacroAll = `#define GENERATE_VARIANT_ALL_${taggedUnion.name.inner.MACRO_NAME()}()
	GENERATE_VARIANT_ENUM_${taggedUnion.name.inner.MACRO_NAME()}()
	GENERATE_VARIANT_CORE_${taggedUnion.name.inner.MACRO_NAME()}()`

    const poisonedNames: string[] = [tagName, dataName,
        getNameTrickForIfExpression(taggedUnion.name),
        getNameTrickForCaseExpression(taggedUnion.name),
        ...Object.values(unnamedStructMap).map((val): string => {
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            return val!
        }),
        unionNameFor(taggedUnion.name),
        ...(["1", "2"] as WhichAssert[]).map((w: WhichAssert): string => {
            return getAssertStructName(taggedUnion.name, w)
        })
    ]

    const macros: string[] = [
        generateMacroEnum,
        generateMacroCore,
        generateMacroAll,
        ...taggedUnion.member.flatMap((mem): string[] => {
            const macros: GeneratedMacros = generateIfMacros(mem, taggedUnion, unnamedStructMap)
            return macros.macros
        }
        ),
        ...taggedUnion.member.map((mem): string => generateIfNotMacro(mem, taggedUnion)),
        generateSwitchMacro(taggedUnion),
        ...taggedUnion.member.flatMap((mem): string[] => {
            const macros: GeneratedMacros = generateCaseMacros(mem, taggedUnion, unnamedStructMap)
            return macros.macros
        }),
    ]


    return (
        `#define ${getStateAssertNameFor(taggedUnion.name)}(state, expected_state) ${genericStateAssert}(state, expected_state, ${taggedUnion.name.inner.snake_case()}, ${toCStr(taggedUnion.name.inner.PascalCase())
        })

${macros.map(a => a.split("\n").join(" \\\n")).join("\n\n")}

${generatePoisonPragma(poisonedNames)
        }
`)
}

function generateCppTagAsErrorVariantFeature(taggedUnions: TaggedUnion[]): string {
    const macro = `#define CPP_DEFINE_ERROR_VARIANTS()
namespace cpp::error_variants {
	template <typename T>
	struct IsErrorVariant : std::false_type {};

	template <typename T>
	struct ErrorVariantConversionImpl;

${taggedUnions.map((union): string => {

        const members = union.member

        if (members.length != 2) {
            throw new Error("Trying to use a non error variant as one! Reason: not 2 members")
        }

        let errorMemberIdx: number;

        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        if (members[0]!.name.inner.eq(CaseName.fromPascalCase("Error"))) {
            errorMemberIdx = 0
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        } else if (members[1]!.name.inner.eq(CaseName.fromPascalCase("Error"))) {
            errorMemberIdx = 1
        } else {
            throw new Error("Trying to use a non error variant as one! Reason: no error member found")
        }

        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        const errorMember = members[errorMemberIdx]!

        let errorGetter: string

        if (errorMember.type === null) {
            throw new Error("Trying to use a non error variant as one! Reason: type is null")
        } else if (isSimpleTaggedType(errorMember.type)) {
            if (errorMember.type.name != "tstr_static") {
                throw new Error("Trying to use a non error variant as one! Reason: type is not a valid error type, 1")
            }
            errorGetter = ""
        } else {
            const structMembers = errorMember.type.struct.members
            if (structMembers.length != 1) {
                throw new Error("Trying to use a non error variant as one! Reason: type is not a valid error type, 2")
            }

            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            const structErrorMembers = structMembers[0]!

            if (structErrorMembers.typeName != "tstr_static") {
                throw new Error("Trying to use a non error variant as one! Reason: type is not a valid error type, 3")
            }

            errorGetter = `.${structErrorMembers.name}`
        }


        return `/* Specialization of IsErrorVariant for ${union.name.inner.PascalCase()} */
template <>
struct IsErrorVariant<${union.name.inner.PascalCase()}> : std::true_type {};

/* Specialization of ErrorVariantConversionImpl for ${union.name.inner.PascalCase()} */
template <>
struct ErrorVariantConversionImpl<${union.name.inner.PascalCase()}> {
	static constexpr std::optional<tstr_static> to_cpp_type(const ${union.name.inner.PascalCase()}& value){
		${getIfMacroName(union.name, errorMember.name, "const")}(value, error_result){
			return std::optional<tstr_static>{error_result${errorGetter}};
		}
		return std::nullopt;
	}
};
`


    }).join("\n\n")}
}`


    return macro.split("\n").join(" \\\n	")
}

function getCppFeatures(taggedUnions: TaggedUnion[]): string {

    const features: (keyof CppFeatures)[] = ["tagAsErrorVariant"]

    const results: string[] = []

    for (const feature of features) {
        const haveFeature: TaggedUnion[] = taggedUnions.filter((t): boolean => {
            return (t.options.cppFeatures?.[feature] ?? undefined) === true;
        })

        switch (feature) {
            // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
            case "tagAsErrorVariant": {

                results.push(generateCppTagAsErrorVariantFeature(haveFeature));
                break;
            }
            default: {
                throw new Error("UNREACHABLE cpp feature")
            }
        }
    }


    return results.join("\n")


}

export async function generateVariantCodeC(generatedVariantsFileH: string, inputDataPath: string): Promise<void> {

    const tasks: Promise<void>[] = []

    assert(path.extname(generatedVariantsFileH) == ".h", "variant file has to end in .h")

    const globalTaggedUnions = await getGlobalTaggedUnions(inputDataPath)

    if (globalTaggedUnions.length == 0) {
        throw new Error('No tagged unions defined')
    }

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


    const nameForCompoundLiteralIgnoreMacro = "__INTERNALS_IGNORE_C_COMPOUND_LITERALS_ERROR_IN_CPP"

    const headerData = `
#pragma once

${await addGenerateMacros("variants")}

#ifdef __cplusplus

#if defined(__clang__)
	#define ${nameForCompoundLiteralIgnoreMacro}() _Pragma ("GCC diagnostic ignored \\"-Wc99-extensions\\"")
#elif defined(__GNUC__)
	/* I don't care, that this disables many errors, as this header should only be used from C, if it is used fom C++, the errors don't matter */
	#define ${nameForCompoundLiteralIgnoreMacro}() _Pragma ("GCC diagnostic ignored \\"-Wpedantic\\"")
#else
	#define ${nameForCompoundLiteralIgnoreMacro}()
#endif

	#define ${getVariantDeclarationMacro(false)}() _Pragma ("GCC diagnostic push") ${nameForCompoundLiteralIgnoreMacro}()
	#define ${getVariantDeclarationMacro(true)}() _Pragma ("GCC diagnostic pop")
#else
	#define ${getVariantDeclarationMacro(false)}()
	#define ${getVariantDeclarationMacro(true)}()
#endif

#include <tstr.h>

#ifdef __cplusplus
extern "C" {
	${getVariantDeclarationMacro(false)}()
#endif

${globalMacros.map(m => m.split("\n").join(" \\\n")).join("\n\n")}


${globalTaggedUnions.map(un => generatedUnionForCHeader(un)).join("\n\n")}


#ifdef __cplusplus
	${getVariantDeclarationMacro(true)}()
}
#endif

#ifdef __cplusplus
${getCppFeatures(globalTaggedUnions)}
#endif
`

    tasks.push(writeFileAndDirs(generatedVariantsFileH, headerData))

    await Promise.all(tasks)


}
