import { type TaggedUnion, makeUnionName, CaseName, makeMemberName, makeStructType, makeStructMember, makeSimpleType, makeEnumName } from "./base.js";

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
        name: makeUnionName(CaseName.fromPascalCase("HuffmanDecodeResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeStructType([
                    makeStructMember(
                        "SizedBuffer",
                        "result",
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
            name: makeEnumName(CaseName.fromPascalCase("HuffmanDecodeResultType")),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("HuffmanEncodeFixedResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeStructType([
                    makeStructMember(
                        "size_t",
                        "size",
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
            name: makeEnumName(CaseName.fromPascalCase("HuffmanEncodeFixedResultType")),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
    {
        name: makeUnionName(CaseName.fromPascalCase("HuffmanEncodeResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: makeStructType([
                    makeStructMember(
                        "SizedBuffer",
                        "result",
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
            name: makeEnumName(CaseName.fromPascalCase("HuffmanEncodeResultType")),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
     {
        name: makeUnionName(CaseName.fromPascalCase("GenericResult")),
        member: [
            {
                name: makeMemberName(CaseName.fromPascalCase("Ok")),
                type: null
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
            name: makeEnumName(CaseName.fromPascalCase("GenericResultType")),
            underlyingType: "bool"
        },
        options: {
            requirements: {
                order: "best_size"
            }
        }
    },
]

