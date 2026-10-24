#!/usr/bin/env python3
"""Генерирует x86-заглушки нативной группы AnyPS5 (см. core/libs/prx/aps5native/Aps5Native.hpp).

Для каждой библиотеки группы берёт C-экспорты из x86-сборки (unpatched .prx), находит их определения
в исходниках и пишет заглушку: функцию с тем же именем (а значит, после NID-патчинга с тем же NID),
которая передаёт аргументы в нативную сборку через мост box64.

Аргументы x86 SysV раскладываются по классам независимо (целые/указатели — rdi..r9 и стек,
вещественные — xmm0..xmm7), поэтому заглушке достаточно знать класс каждого параметра: целые и
указатели объявляются как uint64_t, float и double — как есть. Структуры по значению, variadic и
вещественные результаты генератор не поддерживает и перечисляет: их нужно описать вручную
(--manual).

Использование:
  gen_native_stubs.py <x86 unpatched dir> <core/libs dir> <out dir> [--manual name,...]
"""

import os
import re
import subprocess
import sys

GROUP = {
    "libSceAgcDriver": ["libSceAgcDriver"],
    "libSceVideoOut": ["libSceVideoOut"],
    "libScePad": ["libScePad"],
    "libSceMouse": ["libSceMouse"],
    "libSceKeyboard": ["libSceKeyboard"],
    "libSceAudioOut": ["libSceAudioOut"],
}

# Внутренности статического SDL и прочие не-API экспорты.
SKIP = re.compile(r"^(_Z|SDL|HIDAPI|PLATFORM|hid_|OFFSCREEN|DUMMY|Compute|Guess|X11_|ALSA|PULSE|DISKAUDIO|KMSDRM|"
                  r"WAYLAND|VULKAN|Wayland|Android|LINUX_|EVDEV|PIPEWIRE|JACK|DSP|Emscripten|SNDIO|_init$|_fini$|yuv|rgb24_|decode_edid|dump_monitor_info)|_REAL$")

# Вызываются только нативным кодом группы (x86-заглушка не нужна; std::exception_ptr через ABI не передать).
NATIVE_ONLY = {"AgcDriverReportFailure_nid_postfix", "PadReportInputFailure_nid_postfix"}

INTEGER_WORDS = {
    "bool", "char", "short", "int", "long", "unsigned", "signed", "size_t", "ssize_t", "uintptr_t", "intptr_t",
    "ptrdiff_t", "off_t", "pid_t", "wchar_t", "char16_t", "char32_t",
}
FLOAT_WORDS = {"float"}
DOUBLE_WORDS = {"double"}


def exports(prx):
    out = subprocess.run(["llvm-nm", "-D", "--defined-only", prx], capture_output=True, text=True, check=True).stdout
    names = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in ("T", "W") and not SKIP.search(parts[2]):
            names.append(parts[2].split("@")[0])
    return sorted(set(names))


def read_sources(root):
    texts = {}
    for base, _, files in os.walk(root):
        if "/tests" in base or "/tools" in base:
            continue
        for name in files:
            if name.endswith((".cpp", ".hpp", ".h")):
                path = os.path.join(base, name)
                with open(path, encoding="utf-8", errors="replace") as f:
                    texts[path] = f.read()
    return texts


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", " ", text)


def find_definition(name, sources):
    pattern = re.compile(r"(?:^|[;}\n])([^;{}#]*?)\b" + re.escape(name) + r"\s*\(((?:[^()]|\([^()]*\))*)\)\s*(?:const\s*)?(?:noexcept\s*)?(?:try\s*)?\{")
    for path, text in sources.items():
        if name not in text:
            continue
        match = pattern.search(strip_comments(text))
        if match:
            return match.group(1).strip(), match.group(2).strip(), path
    return None


def split_params(params):
    if params in ("", "void"):
        return []
    depth, current, result = 0, "", []
    for ch in params:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        if ch == "," and depth == 0:
            result.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        result.append(current.strip())
    return result


def type_words(declaration):
    declaration = declaration.split("=")[0]
    for word in ("extern", '"C"', "APS5_VABI", "static", "inline", "const", "volatile", "struct", "enum", "[[noreturn]]", "[[nodiscard]]", "[[maybe_unused]]", "__attribute__((sysv_abi))"):
        declaration = declaration.replace(word, " ")
    return declaration


def classify(declaration, typedefs, is_return=False):
    """'i' — целое/указатель, 'f' — float, 'd' — double, 'v' — void, None — не распознан."""
    text = type_words(declaration)
    if "std::span<" in text or re.match(r"\s*span<", text):
        return "ii"  # {указатель, длина}: два целых регистра и на x86-64, и на aarch64
    if "(" in text and "*" in text:
        return "i"  # указатель на функцию
    if "*" in text or "&" in text:
        return "i"
    words = re.findall(r"[A-Za-z_][A-Za-z0-9_:]*", text)
    if not is_return and len(words) >= 2:
        words = words[:-1]  # имя параметра
    if not words:
        return None
    if words == ["void"]:
        return "v"
    resolved = []
    for word in words:
        word = word.split("::")[-1]
        seen = set()
        while word in typedefs and word not in seen:
            seen.add(word)
            word = typedefs[word]
        resolved.append(word)
    if any(w in FLOAT_WORDS for w in resolved):
        return "f"
    if any(w in DOUBLE_WORDS for w in resolved):
        return "d"
    if all(w in INTEGER_WORDS or re.fullmatch(r"u?int(8|16|32|64)_t", w) or w == "<int>" for w in resolved):
        return "i"
    return None


def collect_typedefs(texts):
    typedefs = {}
    for text in texts:
        text = strip_comments(text)
        for match in re.finditer(r"\btypedef\s+([^;{}()]+?)\s+([A-Za-z_]\w*)\s*;", text):
            base, name = match.group(1), match.group(2)
            typedefs[name] = "<ptr>" if "*" in base else base.split()[-1]
        for match in re.finditer(r"\busing\s+([A-Za-z_]\w*)\s*=\s*([^;{}]+);", text):
            name, base = match.group(1), match.group(2).strip()
            typedefs[name] = "<ptr>" if "*" in base else base.split()[-1].split("::")[-1]
        for match in re.finditer(r"\benum\s+(?:class\s+|struct\s+)?([A-Za-z_]\w*)\s*(?::\s*[\w:]+\s*)?\{", text):
            typedefs.setdefault(match.group(1), "<int>")
        for match in re.finditer(r"\btypedef\s+enum\b[^;]*?\}\s*([A-Za-z_]\w*)\s*;", text, flags=re.S):
            typedefs[match.group(1)] = "<int>"
    for key, value in list(typedefs.items()):
        if value == "<ptr>":
            typedefs[key] = "uintptr_t"
        elif value == "<int>":
            typedefs[key] = "int"
    return typedefs


def stub_function(index, name, ret, params):
    ints, fps, decl = [], [], []
    expanded = []
    for kind in params:
        expanded += list(kind) if kind == "ii" else [kind]
    params = expanded
    for i, kind in enumerate(params):
        if kind == "i":
            decl.append(f"std::uint64_t a{i}")
            ints.append(f"a{i}")
        elif kind == "f":
            decl.append(f"float a{i}")
            fps.append(f"floatBits(a{i})")
        else:
            decl.append(f"double a{i}")
            fps.append(f"a{i}")
    signature = ", ".join(decl)
    if fps:
        if len(ints) > 6 or len(fps) > 4:
            return None
        call = "Aps5NativeCallFd_nid_no_patch(functions[%d], %s)" % (index, ", ".join(ints + ["0"] * (6 - len(ints)) + fps + ["0.0"] * (4 - len(fps))))
    else:
        if len(ints) > 8:
            return None
        call = "Aps5NativeCall_nid_no_patch(functions[%d], %s)" % (index, ", ".join(ints + ["0"] * (8 - len(ints))))
    if ret == "v":
        body = f"    {call};"
        rtype = "void"
    else:
        body = f"    return {call};"
        rtype = "std::uint64_t"
    return f'extern "C" {rtype} {name}({signature}) {{\n    if (functions[{index}] == nullptr) Aps5NativeMissing_nid_no_patch("{name}");\n{body}\n}}\n'


def generate(library, names, sources, typedefs, manual, out_dir, host):
    lines = [
        f"// Сгенерировано tools/gen_native_stubs.py: x86-заглушка {library}.prx, переадресующая вызовы",
        "// в нативную (aarch64) сборку через мост box64. Не править вручную.",
        '#include "prx/aps5native/X86Host.hpp"',
        "",
        "#include <cstdint>",
        "#include <cstring>",
        "",
        "namespace {",
        "",
        "[[maybe_unused]] double floatBits(float value) {",
        "    // float передаётся в младших 32 битах d-регистра (нативная сторона читает s-регистр).",
        "    std::uint64_t bits = 0;",
        "    std::memcpy(&bits, &value, sizeof(value));",
        "    double result;",
        "    std::memcpy(&result, &bits, sizeof(result));",
        "    return result;",
        "}",
        "",
    ]
    bodies, kept, problems = [], [], []
    for name in names:
        if name in NATIVE_ONLY:
            continue
        if name in manual:
            problems.append((name, "описана вручную"))
            continue
        found = find_definition(name, sources)
        if not found:
            problems.append((name, "определение не найдено"))
            continue
        ret_text, param_text, path = found
        ret = classify(ret_text, typedefs, is_return=True)
        params = [classify(p, typedefs) for p in split_params(param_text)]
        if "..." in param_text:
            problems.append((name, "variadic"))
            continue
        if ret in (None, "f", "d") or None in params:
            problems.append((name, f"не распознано: {ret_text} ({param_text}) [{os.path.relpath(path)}]"))
            continue
        code = stub_function(len(kept), name, ret, params)
        if code is None:
            problems.append((name, "слишком много аргументов"))
            continue
        kept.append(name)
        bodies.append(code)
    lines.append(f"const char* const names[] = {{")
    lines += [f'    "{n}",' for n in kept]
    lines.append("};")
    lines.append(f"void* functions[{max(len(kept), 1)}] = {{}};")
    lines.append("")
    lines.append("__attribute__((constructor)) void loadNative() {")
    lines.append(f'    Aps5NativeResolve_nid_no_patch("{library}.prx", names, functions, {len(kept)});')
    lines.append("}")
    lines.append("")
    lines.append("} // namespace")
    lines.append("")
    lines += bodies
    path = os.path.join(out_dir, f"{library}.cpp")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    return kept, problems


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    unpatched, libs_dir, out_dir = sys.argv[1:4]
    manual = set()
    if "--manual" in sys.argv:
        manual = set(sys.argv[sys.argv.index("--manual") + 1].split(","))
    os.makedirs(out_dir, exist_ok=True)
    all_texts = []
    per_lib = {}
    for library, dirs in GROUP.items():
        sources = {}
        for d in dirs:
            sources.update(read_sources(os.path.join(libs_dir, "prx", d)))
        per_lib[library] = sources
        all_texts += list(sources.values())
    for base in ("prx/libc/include", "prx/libkernel", "prx/libScePad/include"):
        all_texts += list(read_sources(os.path.join(libs_dir, base)).values())
    with open(os.path.join(libs_dir, "SceTypes.hpp"), encoding="utf-8", errors="replace") as f:
        all_texts.append(f.read())
    typedefs = collect_typedefs(all_texts)
    status = 0
    for library in GROUP:
        names = exports(os.path.join(unpatched, f"{library}.prx"))
        kept, problems = generate(library, names, per_lib[library], typedefs, manual, out_dir, library == "libSceAgcDriver")
        print(f"{library}: {len(kept)} заглушек, {len(problems)} требуют внимания")
        for name, why in problems:
            print(f"  {name}: {why}")
    return status


if __name__ == "__main__":
    sys.exit(main())
