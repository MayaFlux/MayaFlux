#!/usr/bin/env python3

import argparse
import json
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "src"
REGISTRY = SOURCE_ROOT / "MayaFlux/API/Proxy/Registry.hpp"
DEFAULT_COMPILE_COMMANDS = ROOT / "build/compile_commands.json"
DEFAULT_OUTPUT = SOURCE_ROOT / "MayaFlux/API/Proxy/Generated"
ENTRY = re.compile(r"^\s*([NWB])\((\w+),\s*(MayaFlux::[\w:]+)\)\s*\\?\s*$", re.M)
TYPE_TOKEN = re.compile(r"(?<![\w:])([A-Z]\w*)(?![\w:]|::)")
NAMESPACE = re.compile(r"\bnamespace\s+(MayaFlux(?:::\w+)*)\s*\{")
TYPE_DECLARATION = re.compile(r"\b(?:class|struct|enum(?:\s+class)?|using)\s+(?:MAYAFLUX_API\s+)?([A-Z]\w*)\b")
QUALIFIED_TYPE = re.compile(r"\b(?:MayaFlux::)?[A-Z]\w*(?:::[A-Z]\w*)+")


@dataclass(frozen=True)
class Factory:
    name: str
    type_name: str
    header: Path


@dataclass(frozen=True)
class Parameter:
    type_name: str
    name: str
    default: str | None


@dataclass(frozen=True)
class Constructor:
    parameters: tuple[Parameter, ...]


def entries() -> list[Factory]:
    result = []
    for _, name, type_name in ENTRY.findall(REGISTRY.read_text()):
        class_name = type_name.rsplit("::", 1)[1]
        declaration = re.compile(
            rf"^\s*(?:class|struct)\s+(?:\w+\s+)*{re.escape(class_name)}\s*(?:final\s*)?(?::|\{{)",
            re.M,
        )
        matches = [
            path for path in (SOURCE_ROOT / "MayaFlux").rglob(f"{class_name}.hpp")
            if declaration.search(path.read_text())
        ]
        if len(matches) != 1:
            raise ValueError(f"{name}: expected one defining class header, found {len(matches)}: {matches}")
        result.append(Factory(name, type_name, matches[0]))
    if not result or len({item.name for item in result}) != len(result):
        raise ValueError("registry is empty or contains duplicate factory names")
    return result


def compiler_flags(database: Path) -> tuple[list[str], Path]:
    commands = json.loads(database.read_text())
    match = next((item for item in commands if Path(item["file"]).name == "Creator.cpp"), None)
    if match is None:
        raise ValueError("compile_commands.json has no Creator.cpp entry")
    args = match.get("arguments") or shlex.split(match["command"])
    result = []
    paired = {"-I", "-isystem", "-iquote", "-idirafter", "-D", "-U", "-include", "--sysroot", "-target"}
    discarded = {"-o", "-MF", "-MT", "-MQ", "-MJ", "-x"}
    index = 1
    while index < len(args):
        arg = args[index]
        if arg in discarded:
            index += 2
            continue
        if arg in paired:
            result.extend(args[index:index + 2])
            index += 2
            continue
        if arg in {"-c", "-MD", "-MMD", "-MP"} or arg == match["file"] or Path(arg).name == "Creator.cpp":
            index += 1
            continue
        if arg.startswith(("-o", "-MF", "-MT", "-MQ", "-MJ")):
            index += 1
            continue
        result.append(arg)
        index += 1
    return result, Path(match["directory"]).resolve()


def json_documents(output: str) -> list[dict]:
    decoder = json.JSONDecoder()
    documents = []
    offset = 0
    while output[offset:].strip():
        offset += len(output[offset:]) - len(output[offset:].lstrip())
        document, consumed = decoder.raw_decode(output, offset)
        documents.append(document)
        offset = consumed
    return documents


def class_ast(factory: Factory, clang: str, flags: list[str], directory: Path) -> dict:
    command = [
        clang, *flags, "-I", str(SOURCE_ROOT), "-x", "c++", "-fsyntax-only",
        "-Xclang", "-ast-dump=json", "-Xclang", f"-ast-dump-filter={factory.type_name}",
        str(factory.header),
    ]
    completed = subprocess.run(command, cwd=directory, capture_output=True, text=True, check=False)
    if completed.returncode:
        raise ValueError(f"{factory.name}: Clang could not parse {factory.header}:\n{completed.stderr}")
    matches = [
        node for node in json_documents(completed.stdout)
        if node.get("kind") == "CXXRecordDecl"
        and node.get("name") == factory.type_name.rsplit("::", 1)[1]
        and node.get("completeDefinition")
    ]
    if len(matches) != 1:
        raise ValueError(f"{factory.name}: expected one complete class AST, found {len(matches)}")
    return matches[0]


def source_offset(node: dict) -> int:
    begin = node.get("range", {}).get("begin", {})
    if "offset" not in begin:
        raise ValueError("declaration has no direct source offset")
    return begin["offset"]


def closing_paren(source: bytes, opening: int) -> int:
    depth = 0
    quote = None
    index = opening
    while index < len(source):
        char = source[index:index + 1]
        if quote:
            if char == b"\\":
                index += 2
                continue
            if char == quote:
                quote = None
        elif char in {b'"', b"'"}:
            quote = char
        elif char == b"(":
            depth += 1
        elif char == b")":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError("constructor has no closing parenthesis")


def parameter_spellings(constructor: dict, parameters: list[dict], source: bytes) -> list[str]:
    start = constructor.get("loc", {}).get("offset")
    if start is None:
        start = source_offset(constructor)
    opening = source.find(b"(", start)
    if opening < 0:
        raise ValueError("constructor has no parameter list")
    closing = closing_paren(source, opening)
    offsets = [source_offset(parameter) for parameter in parameters]
    if offsets != sorted(offsets) or any(not opening < offset < closing for offset in offsets):
        raise ValueError("parameter offsets do not match constructor source")
    spellings = []
    for index, offset in enumerate(offsets):
        end = offsets[index + 1] if index + 1 < len(offsets) else closing
        spelling = source[offset:end].decode("utf-8").strip()
        if index + 1 < len(offsets):
            spelling = spelling.removesuffix(",").strip()
        spellings.append(spelling)
    return spellings


def nested_names(record: dict) -> set[str]:
    kinds = {"CXXRecordDecl", "EnumDecl", "TypeAliasDecl", "TypedefDecl"}
    return {
        node["name"] for node in record.get("inner", [])
        if node.get("kind") in kinds and node.get("name")
    }


def qualify_nested(value: str, factory: Factory, names: set[str]) -> str:
    for name in sorted(names, key=len, reverse=True):
        value = re.sub(rf"(?<![\w:]){re.escape(name)}\b", f"{factory.type_name}::{name}", value)
    return value


def type_namespaces() -> dict[str, set[str]]:
    symbols: dict[str, set[str]] = {}
    for header in (SOURCE_ROOT / "MayaFlux").rglob("*.hpp"):
        source = header.read_text()
        source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
        source = re.sub(r"//[^\n]*", "", source)
        namespaces = list(NAMESPACE.finditer(source))
        for declaration in TYPE_DECLARATION.finditer(source):
            preceding = [item for item in namespaces if item.start() < declaration.start()]
            if preceding:
                symbols.setdefault(declaration.group(1), set()).add(preceding[-1].group(1))
    return symbols


def type_headers() -> dict[str, set[Path]]:
    headers: dict[str, set[Path]] = {}
    for header in (SOURCE_ROOT / "MayaFlux").rglob("*.hpp"):
        source = header.read_text()
        source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
        source = re.sub(r"//[^\n]*", "", source)
        namespaces = list(NAMESPACE.finditer(source))
        for declaration in TYPE_DECLARATION.finditer(source):
            tail = source[declaration.end():]
            boundary = re.search(r"[{};]", tail)
            is_alias = declaration.group().startswith("using ") and boundary and "=" in tail[:boundary.start()]
            if not boundary or (boundary.group() != "{" and not is_alias):
                continue
            preceding = [item for item in namespaces if item.start() < declaration.start()]
            if preceding:
                name = f"{preceding[-1].group(1)}::{declaration.group(1)}"
                headers.setdefault(name, set()).add(header)
    return headers


def signature_headers(
    factories: list[tuple[Factory, tuple[Constructor, ...]]],
) -> list[Path]:
    definitions = type_headers()
    registered_types = {type_name for _, _, type_name in ENTRY.findall(REGISTRY.read_text())}
    registered_types.update({
        "MayaFlux::Nodes::Node",
        "MayaFlux::Nodes::Network::NodeNetwork",
        "MayaFlux::Core::VKImage",
    })
    includes = set()
    for _, constructors_for_factory in factories:
        for constructor in constructors_for_factory:
            for parameter in constructor.parameters:
                for spelling in (parameter.type_name, parameter.default or ""):
                    for match in QUALIFIED_TYPE.finditer(spelling):
                        reference = match.group()
                        qualified_reference = reference if reference.startswith("MayaFlux::") else f"MayaFlux::{reference}"
                        candidate = qualified_reference
                        while "::" in candidate:
                            if candidate in registered_types and candidate == qualified_reference:
                                break
                            locations = definitions.get(candidate)
                            if locations:
                                if len(locations) != 1:
                                    raise ValueError(f"ambiguous header for {candidate}: {sorted(locations)}")
                                includes.update(locations)
                                break
                            candidate = candidate.rsplit("::", 1)[0]
    return sorted(includes)


def qualify_type(value: str, factory: Factory, nested: set[str], symbols: dict[str, set[str]]) -> str:
    value = qualify_nested(value, factory, nested)
    scope = factory.type_name.rsplit("::", 1)[0]

    def replace(match: re.Match[str]) -> str:
        name = match.group(1)
        candidates = [namespace for namespace in symbols.get(name, ()) if scope == namespace or scope.startswith(namespace + "::")]
        if not candidates:
            return name
        namespace = max(candidates, key=len)
        if sum(len(candidate) == len(namespace) for candidate in candidates) != 1:
            raise ValueError(f"{factory.name}: ambiguous type name {name}")
        return f"{namespace}::{name}"

    return TYPE_TOKEN.sub(replace, value)


def constructors(factory: Factory, record: dict, symbols: dict[str, set[str]]) -> tuple[Constructor, ...]:
    source = factory.header.read_bytes()
    nested = nested_names(record)
    access = "private" if record.get("tagUsed") == "class" else "public"
    result = []
    for node in record.get("inner", []):
        kind = node.get("kind")
        if kind == "AccessSpecDecl":
            access = node.get("access", access)
            continue
        if kind == "FunctionTemplateDecl" and any(
            child.get("kind") == "CXXConstructorDecl" for child in node.get("inner", [])
        ):
            raise ValueError(f"{factory.name}: constructor template needs explicit generator support")
        if kind != "CXXConstructorDecl" or node.get("isImplicit"):
            continue
        if access != "public" or node.get("explicitlyDeleted") or node.get("isDeleted"):
            continue
        parameter_nodes = [child for child in node.get("inner", []) if child.get("kind") == "ParmVarDecl"]
        try:
            spellings = parameter_spellings(node, parameter_nodes, source)
        except ValueError as error:
            raise ValueError(f"{factory.name}: {error}") from error
        params = []
        for number, (child, spelling) in enumerate(zip(parameter_nodes, spellings)):
            type_info = child.get("type", {})
            type_name = type_info.get("qualType") or type_info.get("desugaredQualType")
            if not type_name or "(*)" in type_name or "(&)" in type_name:
                raise ValueError(f"{factory.name}: cannot spell parameter {number} as a method parameter")
            type_name = qualify_type(type_name, factory, nested, symbols)
            default = None
            if child.get("init"):
                if "=" not in spelling:
                    raise ValueError(f"{factory.name}: cannot recover default for parameter {number}")
                default = qualify_nested(spelling.split("=", 1)[1].strip(), factory, nested)
            params.append(Parameter(type_name, child.get("name") or f"arg{number}", default))
        result.append(Constructor(tuple(params)))
    if not result:
        raise ValueError(f"{factory.name}: no public source-declared constructor found")
    return tuple(result)


def parameter_declaration(parameter: Parameter) -> str:
    text = f"{parameter.type_name} {parameter.name}"
    if parameter.default is not None:
        text += f" = {parameter.default}"
    return text


def generate(factories: list[tuple[Factory, tuple[Constructor, ...]]], output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    signature_includes = ["#pragma once", ""]
    signature_includes.extend(
        f'#include "{header.relative_to(SOURCE_ROOT).as_posix()}"'
        for header in signature_headers(factories)
    )
    class_includes = ["#pragma once", ""]
    class_includes.extend(f'#include "{factory.header.relative_to(SOURCE_ROOT).as_posix()}"' for factory, _ in factories)
    declarations = []
    definitions = []
    for factory, overloads in factories:
        result_type = f"std::shared_ptr<{factory.type_name}>"
        for constructor in overloads:
            parameters = ", ".join(parameter_declaration(item) for item in constructor.parameters)
            signature = ", ".join(f"{item.type_name} {item.name}" for item in constructor.parameters)
            arguments = ", ".join(f"std::forward<decltype({item.name})>({item.name})" for item in constructor.parameters)
            declarations.append(f"    {result_type} {factory.name}({parameters});")
            definitions.extend([
                f"{result_type} Creator::{factory.name}({signature})",
                "{",
                f"    auto obj = std::make_shared<{factory.type_name}>({arguments});",
                f'    MF_LIVE_EXPOSE_NAMED("{factory.name}", obj);',
                "    return obj;",
                "}",
                "",
            ])
    (output / "CreatorIncludes.hpp").write_text("\n".join(signature_includes) + "\n")
    (output / "CreatorClasses.hpp").write_text("\n".join(class_includes) + "\n")
    (output / "CreatorDeclarations.inc").write_text("\n".join(declarations) + "\n")
    (output / "CreatorDefinitions.inc").write_text("\n".join(definitions))


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Creator factory overloads from registered class constructors")
    parser.add_argument("--compile-commands", type=Path, default=DEFAULT_COMPILE_COMMANDS)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--clang", default="clang++")
    args = parser.parse_args()
    try:
        flags, directory = compiler_flags(args.compile_commands.resolve())
        factories = []
        symbols = type_namespaces()
        for factory in entries():
            record = class_ast(factory, args.clang, flags, directory)
            if record.get("definitionData", {}).get("isAbstract", False):
                continue
            factories.append((factory, constructors(factory, record, symbols)))
        generate(factories, args.output.resolve())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
