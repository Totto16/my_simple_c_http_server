import { fsAsyncExists } from "../utils.js";
import { type TaggedUnion, CaseName, makeStructType, makeStructMember, makeSimpleType, type TaggedName, makeTaggedName, type TaggedMember, type TaggedUnionEnum, type StructMember, type TaggedTypeStruct, type StructOrder, type TaggedUnionRequirements, type CppFeatures } from "./base.js";
import * as z from "zod";
import fsAsync from "node:fs/promises"



const ParsedNamePascalCaseZ = z.string().nonempty().regex(/^[A-Z][a-zA-Z0-9]*$/)

const ParsedNameFromPartsZ = z.array(z.string().nonempty().regex(/^([A-Z0-9]*)|([a-z0-9]*)$/)).min(2)

const ParsedNameZ = z.xor([ParsedNamePascalCaseZ, ParsedNameFromPartsZ])

type ParsedName = z.infer<typeof ParsedNameZ>;

const ParsedCTypeZ = z.string().nonempty()

const ParsedTaggedTypeStructMemberZ = z.object({
    typeName: ParsedCTypeZ,
    name: z.string().nonempty()
}).strict()

type ParsedTaggedTypeStructMember = z.infer<typeof ParsedTaggedTypeStructMemberZ>


const ParsedTaggedTypeStructZ = z.object({
    struct: z.object({
        members: z.array(ParsedTaggedTypeStructMemberZ).min(1)
    }).strict()
}).strict()



const ParsedTaggedTypeSimpleZ = z.object({
    name: z.string().nonempty()
}).strict()

type ParsedTaggedTypeSimple = z.infer<typeof ParsedTaggedTypeSimpleZ>


const ParsedTaggedTypeZ = z.xor([ParsedTaggedTypeStructZ, ParsedTaggedTypeSimpleZ])

type ParsedTaggedType = z.infer<typeof ParsedTaggedTypeZ>

const ParsedTaggedMemberZ = z.object({
    name: ParsedNameZ,
    type: z.xor([z.null(), ParsedTaggedTypeZ])
}).strict()

type ParsedTaggedMember = z.infer<typeof ParsedTaggedMemberZ>

const ParsedCEnumTypeZ = z.xor([z.literal("bool"), z.literal("u8"), z.literal("u16"), z.literal("u32"), z.literal("u64")])

const ParsedEnumZ = z.object({
    name: ParsedNameZ,
    underlyingType: z.xor([ParsedCEnumTypeZ, z.literal("best_match"), z.null()]).optional()
}).strict()

type ParsedEnum = z.infer<typeof ParsedEnumZ>

const StructOrderZ = z.xor([z.literal("auto"), z.literal("tag_first"), z.literal("tag_second")])

const StructOrderRequirementZ = z.xor([z.literal("best_size"), z.literal("aligned_access")])

const ParsedTaggedUnionRequirementsZ = z.object(
    {
        order: StructOrderRequirementZ.optional()
    }
).strict()

type ParsedTaggedUnionRequirements = z.infer<typeof ParsedTaggedUnionRequirementsZ>

const ParsedCppFeaturesZ = z.object(
    {
        tagAsErrorVariant: z.boolean().optional()
    }
).strict()

type ParsedCppFeatures = z.infer<typeof ParsedCppFeaturesZ>

const ParsedOptionsZ = z.object({
    rawStruct: ParsedNameZ.optional(),
    structOrder: StructOrderZ.optional(),
    requirements: ParsedTaggedUnionRequirementsZ.optional(),
    cppFeatures: ParsedCppFeaturesZ.optional(),
}).strict()

const ParsedTaggedUnionZ = z.object({
    name: ParsedNameZ,
    member: z.array(ParsedTaggedMemberZ).min(2),
    enum: ParsedEnumZ,
    options: z.optional(ParsedOptionsZ).optional()
}).strict();

type ParsedTaggedUnion = z.infer<typeof ParsedTaggedUnionZ>;

function getTaggedNameFromRawName<T>(tag: T, rawName: ParsedName): TaggedName<T> {
    const name = Array.isArray(rawName) ? CaseName.fromParts(rawName) : CaseName.fromPascalCase(rawName)

    return makeTaggedName(tag, name)
}

function isSimpleParsedType(tpe: ParsedTaggedType): tpe is ParsedTaggedTypeSimple {
    return (tpe as Partial<ParsedTaggedTypeSimple>).name !== undefined;
}

function convertParsedMember(member: ParsedTaggedMember): TaggedMember {

    const name = getTaggedNameFromRawName<"member">("member", member.name);

    if (member.type === null) {
        return { name, type: null }
    }

    if (isSimpleParsedType(member.type)) {
        return { name, type: makeSimpleType(member.type.name) };
    }

    const members: StructMember[] = member.type.struct.members.map((member: ParsedTaggedTypeStructMember): StructMember => {
        return makeStructMember(member.typeName, member.name)
    });

    const type: TaggedTypeStruct = makeStructType(members)

    return { name, type };
}

function convertUnderlyingEnumType(underlyingType: ParsedEnum["underlyingType"]): TaggedUnionEnum["underlyingType"] {
    if (underlyingType === null || underlyingType === undefined) {
        return null;
    }

    return underlyingType
}

function convertParsedEnum(enm: ParsedEnum): TaggedUnionEnum {

    const name = getTaggedNameFromRawName<"enum">("enum", enm.name)

    const underlyingType = convertUnderlyingEnumType(enm.underlyingType)

    return { name, underlyingType }

}

function convertRequirements(req: undefined | ParsedTaggedUnionRequirements): TaggedUnionRequirements | undefined {

    if (req === undefined) {
        return undefined;
    }

    if (req.order === undefined) {
        return undefined;
    }

    return { order: req.order };
}

function convertCppFeatures(features: ParsedCppFeatures | undefined): CppFeatures | undefined {
    if (features === undefined) {
        return undefined;
    }


    if (features.tagAsErrorVariant === undefined) {
        return undefined;
    }

    return { tagAsErrorVariant: features.tagAsErrorVariant };
}

function convertParsedOptions(rawOptions: ParsedTaggedUnion["options"]): TaggedUnion["options"] {

    const rawStruct: TaggedName<"raw_struct"> | undefined = rawOptions?.rawStruct ? getTaggedNameFromRawName<"raw_struct">("raw_struct", rawOptions.rawStruct) : undefined

    const structOrder: StructOrder | undefined = rawOptions?.structOrder;

    const requirements: TaggedUnionRequirements | undefined = convertRequirements(rawOptions?.requirements)

    const cppFeatures: CppFeatures | undefined = convertCppFeatures(rawOptions?.cppFeatures)


    return { rawStruct, structOrder, requirements, cppFeatures }

}

function convertParsedType(data: ParsedTaggedUnion[]): TaggedUnion[] {

    return data.map((raw: ParsedTaggedUnion): TaggedUnion => {

        const name: TaggedName<"union"> = getTaggedNameFromRawName<"union">("union", raw.name)

        const member: TaggedMember[] = raw.member.map(convertParsedMember);

        const enum_: TaggedUnionEnum = convertParsedEnum(raw.enum);

        const options: TaggedUnion["options"] = convertParsedOptions(raw.options);

        return { name, member, enum: enum_, options }

    })

}

const ParsedTaggedUnionFullSchemaZ = z.array(ParsedTaggedUnionZ).min(1)

export async function getGlobalTaggedUnions(path: string): Promise<TaggedUnion[]> {
    if (!(await fsAsyncExists(path))) {
        throw new Error(`Input file: ${path} doesn't exist!`)
    }

    const content = (await fsAsync.readFile(path)).toString()

    const unknownData: unknown = JSON.parse(content)

    const parsedResult: ParsedTaggedUnion[] = await ParsedTaggedUnionFullSchemaZ.parseAsync(unknownData, {
        reportInput: true,
    })

    const result = convertParsedType(parsedResult)

    return result
}


export function outputVariantJsonSchema(): void {

    const schema = z.toJSONSchema(ParsedTaggedUnionFullSchemaZ, {
        unrepresentable: "throw",
        cycles: "throw",
        reused: "ref",
        target: "draft-2020-12"

    })

    console.log(JSON.stringify(schema))



}
