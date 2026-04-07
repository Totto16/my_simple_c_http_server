import { type TaggedUnion, makeUnionName, CaseName, makeMemberName, makeStructType, makeStructMember, makeSimpleType, makeEnumName, type TaggedType, type DeepPartial } from "./base.js";

function resolveType(successType: string | null, successName: string | null): null | TaggedType {
    if (successType === null) {
        return null;
    } else if (successName == null) {
        return makeSimpleType(successType)
    } else {
        return makeStructType([
            makeStructMember(
                successType,
                successName,
            )
        ])
    }
}

interface ErrorVariantOptions {
    usageOnlyInC: boolean
}

function makeErrorVariant(baseName: string, successType: string | null, successName: string | null = null, errorVariantOptions: DeepPartial<ErrorVariantOptions> = {}): TaggedUnion {

    const resolvedType: null | TaggedType = resolveType(successType, successName)

    return {
        name: makeUnionName(CaseName.fromPascalCase(baseName)),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: resolvedType
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeStructType([
                    makeStructMember(
                        "tstr_static",
                        "error",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`${baseName}Type`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            },
            cppFeatures: {
                tagAsErrorVariant: errorVariantOptions.usageOnlyInC ? false : true
            }
        }
    };
}

function makeOptionalVariant(baseName: string, successType: string | null, successName: string | null = null): TaggedUnion {

    const resolvedType: null | TaggedType = resolveType(successType, successName)

    return {
        name: makeUnionName(CaseName.fromPascalCase(baseName)),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Value")),
                type: resolvedType
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Null")),
                type: null
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`${baseName}Type`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    };
}

export const globalTaggedUnions: TaggedUnion[] = [
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
        options: {
            requirements: {
                order: "best_size"
            }
        }
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
        options: {
            requirements: {
                order: "best_size"
            }
        }
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
            rawStruct: CaseName.fromPascalCase("AuthenticationProviderImpl"),
            requirements: {
                order: "best_size"
            }
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
                        "tstr_static",
                        "message",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("AuthenticationValidity")),
            underlyingType: "u8"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
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
        options: {
            requirements: {
                order: "best_size"
            }
        }
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
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("HuffmanNode")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Node")),
                type: makeSimpleType("HuffmanNodeNode")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("End")),
                type: makeStructType([
                    makeStructMember(
                        "uint8_t",
                        "value",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeStructType([
                    makeStructMember(
                        "tstr_static",
                        "error",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("HuffmanNodeType")),
            underlyingType: "u8"
        },
        options: {
            rawStruct: CaseName.fromPascalCase("HuffmanNodeImpl"),
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("FastStringCmpNode")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Normal")),
                type: makeStructType([
                    makeStructMember(
                        "FastStringCmpPrefixes",
                        "prefixes",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("End")),
                type: makeStructType([
                    makeStructMember(
                        "FastStringCompareResult",
                        "result",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("FastStringCmpNodeType")),
            underlyingType: "u8"
        },
        options: {
            rawStruct: CaseName.fromPascalCase("FastStringCmpNodeImpl"),
            requirements: {
                order: "best_size"
            }
        }
    },
    makeErrorVariant("HuffmanDecodeResult", "SizedBuffer", "result"),
    makeErrorVariant("HuffmanEncodeFixedResult", "size_t", "size"),
    makeErrorVariant("HuffmanEncodeResult", "SizedBuffer", "result"),
    makeErrorVariant("GenericResult", null),
    makeOptionalVariant("HpackHeaderEntryResult", "HpackHeaderDynamicEntry"),
    makeErrorVariant("Http2HpackDecompressResult", "HttpHeaderFields"),
    makeErrorVariant("LiteralStringResult", "tstr", "value", { usageOnlyInC: true }),
    makeErrorVariant("HpackVariableIntegerResult", "HpackVariableInteger", "value"),
    {
        name: makeUnionName(CaseName.fromPascalCase("HttpAnalyzeHeadersResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeStructType([
                    makeStructMember(
                        "HTTPAnalyzeHeaders",
                        "result",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeStructType([
                    makeStructMember(
                        "HTTPAnalyzeHeaderError",
                        "error",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`HttpAnalyzeHeadersResultType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    makeErrorVariant("HttpBodyReadResult", "SizedBuffer", "body", { usageOnlyInC: true }),
    makeErrorVariant("ParsedRequestUriResult", "ParsedRequestURI", "uri"),
    makeErrorVariant("Http2FrameResult", "Http2Frame", "frame", { usageOnlyInC: true }),
    makeErrorVariant("Http2StartResult", null),
    {
        name: makeUnionName(CaseName.fromPascalCase("WsFragmentOption")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Off")),
                type: null
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Auto")),
                type: null
            }, {
                name: makeMemberName(CaseName.fromPascalCase("Set")),
                type: makeStructType([
                    makeStructMember(
                        "size_t",
                        "fragment_size",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`WsFragmentOptionType`)),
            underlyingType: "u8"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    makeErrorVariant("Utf8DataResult", "Utf8Data", "result"),
    makeErrorVariant("RawHeaderOneResult", "RawHeaderOne", "header", { usageOnlyInC: true }),
    makeErrorVariant("WebSocketRawMessageResult", "WebSocketRawMessage", "message", { usageOnlyInC: true }),
    {
        name: makeUnionName(CaseName.fromPascalCase("FtpCommandTypeInformation")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Normal")),
                type: makeStructType([
                    makeStructMember(
                        "FtpTransmissionType",
                        "type",
                    )
                ])
            }, {
                name: makeMemberName(CaseName.fromPascalCase("Other")),
                type: makeStructType([
                    makeStructMember(
                        "uint8_t",
                        "num",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`FtpCommandTypeInformationType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    makeOptionalVariant("OptionalString", "tstr", "value"),
    {
        name: makeUnionName(CaseName.fromPascalCase("ActiveConnectionData")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Connected")),
                type: makeSimpleType("ActiveConnectedDataImpl")
            }, {
                name: makeMemberName(CaseName.fromPascalCase("Resumed")),
                type: makeSimpleType("ActiveResumeDataImpl")
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`ActiveConnectionDataType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("SendData")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("File")),
                type: makeSimpleType("SingleFile*")
            }, {
                name: makeMemberName(CaseName.fromPascalCase("MultipleFiles")),
                type: makeSimpleType("MultipleFiles*")
            }, {
                name: makeMemberName(CaseName.fromPascalCase("RawData")),
                type: makeStructType([
                    makeStructMember(
                        "SizedBuffer",
                        "data",
                    )
                ])
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`SendType`)),
            underlyingType: "u8"
        },
        options: {
            rawStruct: CaseName.fromPascalCase("SendDataImpl"),
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("JsonValue")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Object")),
                type: makeStructType([
                    makeStructMember(
                        "JsonObject*",
                        "obj",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Array")),
                type: makeStructType([
                    makeStructMember(
                        "JsonArray*",
                        "arr",
                    )
                ])
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Number")),
                type: makeSimpleType("JsonNumber")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("String")),
                type: makeSimpleType("JsonString*")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Boolean")),
                type: makeSimpleType("JsonBoolean")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Null")),
                type: null
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase("JsonValueType")),
            underlyingType: "u8"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("JsonParseResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeSimpleType("JsonValue")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeSimpleType("JsonError")
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`JsonParseResultType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            },
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("JsonSource")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("File")),
                type: makeSimpleType("JsonFileSource")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("String")),
                type: makeSimpleType("JsonStringSource")
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`JsonSourceType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            },
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("Utf8NextCharResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeSimpleType("Utf8Codepoint")
            },
            {
                name: makeMemberName(CaseName.fromPascalCase("Error")),
                type: makeSimpleType("JsonError")
            },
        ],
        enum: {
            name: makeEnumName(CaseName.fromPascalCase(`Utf8NextCharResultType`)),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            },
        }
    },
]
