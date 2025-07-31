#!/usr/bin/env python3

import argparse
import os
import re
import sys
from typing import List, Tuple
# implemented modules
import cpp_mangled_name_parser
import parse_cpp_func_args

def get_cpp_files(root_dir):
    cpp_files = []
    for dirpath, _, filenames in os.walk(root_dir):
        for f in filenames:
            if f.endswith('.cpp'):
                cpp_files.append(os.path.join(dirpath, f))
    return cpp_files

def get_header_files(header_dir):
    header_files = []
    for dirpath, _, filenames in os.walk(header_dir):
        for f in filenames:
            if f.endswith('.h') or f.endswith('.hpp'):
                header_files.append(os.path.join(dirpath, f))
    return header_files

def read_file_skip_comments(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read()

    # Regular expression to match C++ comments
    # Matches single-line comments (//) and multi-line comments (/* */)
    comment_pattern = r'//.*?$|/\*.*?\*/'

    # Remove comments using re.sub
    cleaned_content = re.sub(comment_pattern, '', content, flags=re.DOTALL | re.MULTILINE)

    # Split into lines and strip whitespace
    lines = [line.strip() for line in cleaned_content.splitlines() if line.strip()]

    return lines

namespace_pattern = re.compile(r'\s*namespace\s+([\w+:]+)\s*{')
class_pattern = re.compile(r'^\s*class\s+(\w+)\s*(:.+)?\s*{')
func_export_pattern = r"\s*(XCL_DRIVER_DLLESPEC|XRT_API_EXPORT)"
ualias_pattern = re.compile(r'\s*using\s+(?P<name>[\w_]+)\s*=.+;')
ualias_template_pattern = re.compile(r'\s*template[<\s].*\s*using\s+(?P<name>[\w_]+)\s*=.+;')
enum_pattern = re.compile(r'\s*enum\s+(class\s+)?(?P<name>[\w_]+)\s*(?::\s*[\w_\s]+)?\s*{')
struct_pattern = re.compile(r'\s*struct\s+(?P<name>[\w_]+)\s*{')

def types_map_add(new_type, attr, old_types=None, container_pipe=[]):
    if old_types:
        type_maps = old_types
    else:
        type_maps = dict()
    type_add = "::".join(container_pipe)
    if type_add == "":
        type_add = new_type
    else:
        type_add = type_add + "::" + new_type
    type_maps[type_add] = attr
    return type_maps

def types_get_path(type_name, types_map, class_type):
    if not class_type or class_type == "":
        return type_name
    class_pipe = class_type.split("::")
    while class_pipe:
        class_pipe.append(type_name)
        type_str = "::".join(class_pipe)
        if type_str in types_map:
            return type_str
        class_pipe.pop()
        class_pipe.pop()
    return type_name

def extract_functions_from_file(file, is_header=False, old_types=None):
    if old_types:
        types_map = old_types
    else:
        types_map = set()
    scope_stack = []
    decls = set()
    print(f"***** trying to extract functions from {file} ******")
    lines = read_file_skip_comments(file)
    brackets_scope_stack = []
    in_macro = False
    for index, line in enumerate(lines):
        line_strip = line.strip()
        if line_strip.startswith("#"):
            # skip compilation macro
            in_macro = True
        if in_macro:
            if not line_strip.endswith('\\'):
                in_macro = False
            lines_joined = ""
            continue

        lines_joined = lines_joined + " " + line_strip
        lines_joined = re.sub(r'\s*::\s*', '::', lines_joined)
        lines_joined = lines_joined.strip()
        #print(f"lines_joined: {lines_joined}")
        ns_matches = namespace_pattern.finditer(lines_joined)
        last_match = None
        types_container_pipe = []
        for ns_match in ns_matches:
            #print(f"add namespace: {ns_match.group(1)}, class: {scope_stack}")
            scope_stack.append(ns_match.group(1))
            brackets_scope_stack.append(f"namespace,{ns_match.group(1)}")
            last_match = ns_match
            types_container_pipe.append(ns_match.group(1))
        if last_match:
            lines_joined = lines_joined[last_match.end():].strip()

        type_match = enum_pattern.search(lines_joined)
        if type_match:
            #print(f"enum: {type_match.group('name')}, class: {scope_stack}")
            types_map = types_map_add(type_match.group('name'), "enum", old_types=types_map, container_pipe=scope_stack)
            brackets_scope_stack.append(f"other")
            lines_joined = lines_joined[type_match.end():].strip()

        cls_match = class_pattern.search(lines_joined)
        if cls_match:
            #print(f"add class: {cls_match.group(1)}, class: {scope_stack}")
            types_map = types_map_add(cls_match.group(1), "class", old_types=types_map, container_pipe=scope_stack)
            scope_stack.append(cls_match.group(1))
            brackets_scope_stack.append(f"class,{cls_match.group(1)}")
            types_container_pipe.append(cls_match.group(1))
            lines_joined = lines_joined[cls_match.end():].strip()

        scope = '::'.join(scope_stack) + ('::' if scope_stack else '')
        type_match = ualias_template_pattern.search(lines_joined)
        if type_match:
            #print(f"template alias: {type_match.group('name')}, class: {scope_stack}")
            types_map = types_map_add(type_match.group('name'), "alias", old_types=types_map, container_pipe=scope_stack)
            lines_joined = lines_joined[type_match.end():].strip()
        type_match = ualias_pattern.search(lines_joined)
        if type_match:
            #print(f"alias: {type_match.group('name')}, class: {scope_stack}")
            types_map = types_map_add(type_match.group('name'), "alias", old_types=types_map, container_pipe=scope_stack)
            lines_joined = lines_joined[type_match.end():].strip()
        type_match = struct_pattern.search(lines_joined)
        if type_match:
            #print(f"struct: {type_match.group('name')}, class: {scope_stack}")
            types_map = types_map_add(type_match.group('name'), "struct", old_types=types_map, container_pipe=scope_stack)
            brackets_scope_stack.append(f"other")
            lines_joined = lines_joined[type_match.end():].strip()

        func_match = None
        if brackets_scope_stack and brackets_scope_stack[-1] != "other":
            func_match = parse_cpp_func_args.search_func_op(lines_joined, is_header=is_header)
            if func_match:
                ret_type = func_match.group('ret_type')
                if ret_type:
                    ret_type = ret_type.strip()
                else:
                    ret_type = ""
                class_name = func_match.group('class')
                if class_name:
                    class_name = class_name.strip()
                else:
                    class_name = ""
                func_name = func_match.group('func').strip()
                args = func_match.group('args').strip()
                props = func_match.group('props').strip()
                props = re.sub(r'override', '', props).strip()
                # in c function, it is possible that even namespace is specified
                # full class path name is used when defining the function
                # only does this checking for function definition, which should be done
                # after scaning all headers, otherwise cannot get full picture of classes
                #print(f"lines: {lines_joined}, class_name: {class_name}, scope: {scope}, {types_map}")
                if not is_header:
                    if not re.sub(r"(.*)::", r"\1", class_name) in types_map:
                        class_name = f"{scope}{class_name}"
                else:
                    class_name = f"{scope}{class_name}"
                full_name = f"{ret_type} {class_name}{func_name}({args}) {props}".strip()
                #print(f"func: {full_name}")
                func_required = 1
                if "inline" in ret_type:
                    func_required = 0
                if "static" in ret_type and class_name == "":
                    func_required = 0
                if func_required == 1:
                    #print(f"Return Type: {ret_type}, Class: {class_name}, Function: {func_name}, Args: ({args}), Properties: {props}")
                    #print(f"{full_name}")
                    decls.add(full_name)
            if func_match and func_match.group(0).strip().endswith('{'):
                brackets_scope_stack.append(f"other")
                lines_joined = lines_joined[func_match.end():].strip()
        left_braceket_matches = [m.group() for m in re.finditer(r'{', lines_joined)]
        for m in left_braceket_matches:
            #print(f"add others: {lines_joined}")
            brackets_scope_stack.append(f"other")
        right_braceket_matches = [m.group() for m in re.finditer(r'}', lines_joined)]
        for m in right_braceket_matches:
            #print(f"pop brackets stack: backets: {brackets_scope_stack[-1]} {lines_joined}")
            if not brackets_scope_stack:
                sys.exit(f"ERROR: Unexpected bracket in line {index}, {line}")
            current_brackets_scope = brackets_scope_stack.pop()
            #print(f"pop - 0 {current_brackets_scope}")
            if re.search(r"class,|namespace,", current_brackets_scope):
                current_scope = scope_stack.pop()
                #print(f"pop {current_brackets_scope}, {current_scope}")
                if current_scope != current_brackets_scope.split(",")[1]:
                    sys.exit(f"Unmatched class/namespace scopes: {current_brackets_scope}, {current_scope}")
        left_braceket_end = 0
        if left_braceket_matches:
            left_braceket_end = list(re.finditer(r'{', lines_joined))[-1].end()
        right_braceket_end = 0
        if right_braceket_matches:
            right_braceket_end = list(re.finditer(r'}', lines_joined))[-1].end()

        line_restart = left_braceket_end
        if right_braceket_end > line_restart:
            line_restart = right_braceket_end
        lines_joined = lines_joined[line_restart:].strip()
        if lines_joined.endswith(";"):
            lines_joined = ""

    return decls, types_map

def update_func_arg_type(func, func_class, type_info: dict, types_map):
    type_i = type_info['type']
    if not type_i:
        return func
    type_r = types_get_path(type_i, types_map, func_class)
    if type_i != type_r:
        func = re.sub(rf"([(<,]\s*){type_i}([\s,<>)&*])", rf"\1{type_r}\2", func + " ").strip()
        func = re.sub(rf"(^\s*){type_i}([\s,<>)&*])", rf"\1{type_r}\2", func + " ").strip()
        func = re.sub(rf"(const\s+|constexpr\s+|volatile\s+){type_i}([\s,<>)&*])", rf"\1{type_r}\2", func + " ").strip()
    if type_info['template_args']:
        for t in type_info['template_args']:
            func = update_func_arg_type(func, func_class, t, types_map)
    return func
    

def update_func_adjust_args(func, types_map):
    #print(f"func: {func}")
    #remove default value from function arguements
    #TODO: currently, it requires to remove default values assignment
    #before injecting argument names if argument names are missing.
    #currently, cannot swap this sequence, due to the inject_func_args_names()
    #implementation
    func = parse_cpp_func_args.rm_func_default_vals(func)

    #inject function names if arugments don't have a name in its declaration
    # parse_cpp_function_declaration() assumes argument names always there
    # and thus, will need to inject arguments names first
    func = parse_cpp_func_args.inject_func_args_names(func)
    func_name, func_class, ret_tinfo, args_tinfo = parse_cpp_func_args.parse_cpp_function_declaration(decl=func)
    # replace each type with class information
    # each type information element is a dictionary item
    if not args_tinfo:
        args_tinfo = []
    if ret_tinfo:
        args_tinfo.append(ret_tinfo)
    #print(f"func: {func}, func_class: {func_class}, args: {args_tinfo}")
    for t in args_tinfo:
        if t:
            func = update_func_arg_type(func, func_class, t, types_map)
    return func

def output_funcs(funcs, ofile=None, append=False, prefix=""):
    if ofile:
        if append:
            oflag = 'a'
        else:
            oflag = 'w'
        with open(ofile, 'w', encoding='utf-8') as out:
            for line in sorted(funcs):
                out.write(prefix + line + '\n')
    else:
        for line in sorted(funcs):
            print(prefix + line)

def main():
    parser = argparse.ArgumentParser(description="Scan cpp files and print functions also declared in header files.")
    parser.add_argument('--cpp_dir', help='Directory containing .cpp files', default=None)
    parser.add_argument('--header_dir', help='Directory containing header files')
    parser.add_argument('--out_header', help='Output file for the captures from header (default: stdout)', default=None)
    parser.add_argument('--out_cpp', help='Output file for the captures from cpp (default: stdout)', default=None)
    parser.add_argument('--diff', nargs='?', const="stdout",
                        help='Output diff between headers and cpps scanning, if no file specified, will output to stdout',
                        default=None)
    parser.add_argument('--gen_func_lookup', help='Generate functions mangled names lookup table.', action="store_true", default=False)
    parser.add_argument('--gen_wrapper', help='Generate wrapper library implementation', action="store_true", default=False)
    parser.add_argument('--xrt_src', help='XRT source root, which is used to locate xrt header. Required for --gen_func_lookup.', default=None)
    parser.add_argument('--bld_dir', help='Build directort, required for generating mangled functions lookup table.', default=None)
    parser.add_argument('--gen_hook', help='Generate hooking functions. Generated functions will be stored in the specified directory', default=None)
    parser.add_argument('--func_mangled_map', help="specified the function mangled name mapping, only used with --gen-hook", default=None)
    args = parser.parse_args()

    header_files = get_header_files(args.header_dir)
    #header_files = ["/proj/xsjhdstaff5/wendlian/XRT/src/runtime_src/core/include/xrt/experimental/xrt_xclbin.h"]
    if args.cpp_dir:
      cpp_files = get_cpp_files(args.cpp_dir)
    else:
      cpp_files = None

    header_funcs = set()
    types_map = dict()
    for h in header_files:
        funcs, types_map = extract_functions_from_file(file=h, is_header=True, old_types=types_map)
        header_funcs.update(funcs)

    cpp_funcs = set()
    for cpp in cpp_files or []:
        funcs, types_map_nouse = extract_functions_from_file(file=cpp, is_header=False, old_types=types_map)
        cpp_funcs.update(funcs)

    # only capture C++ APIs
    cpp_pattern = re.compile(r".*xrt::(?!.*xbtracer::).*")
    output_headers = set(filter(lambda item: cpp_pattern.match(item), header_funcs))
    print(f"**** Outputing functions from Header ****")
    output_funcs(output_headers, ofile=args.out_header, prefix="HH: ")
    output_funcs(types_map, ofile=args.out_header, prefix="HT: ")
    print("**** Outputing adjusted functions from Header ****")
    adjust_funcs_h = set()
    for f in output_headers:
        adjust_funcs_h.add(update_func_adjust_args(f, types_map))
    output_funcs(adjust_funcs_h, ofile=args.out_header, prefix="HU: ")

    output_cpp = set(filter(lambda item: cpp_pattern.match(item), cpp_funcs))
    print(f"**** Outputing functions from Cpp ****")
    output_funcs(output_cpp, ofile=args.out_cpp, prefix="CC: ")
    adjust_funcs_c = set()
    for f in output_cpp:
        adjust_funcs_c.add(update_func_adjust_args(f, types_map))
    output_funcs(adjust_funcs_c, ofile=args.out_cpp, prefix="CU: ")

    if args.diff:
        only_headers = adjust_funcs_h - adjust_funcs_c
        only_cpp = adjust_funcs_c - adjust_funcs_h
        if args.diff == "stdout":
            diff_file = None
        else:
            diff_file = args.diff
        if only_headers:
            print(f"**** Outputing diffs -- headers ****")
            output_funcs(only_headers, ofile=diff_file, append=False, prefix="HO: ")
        if only_cpp:
            print(f"**** Outputing diffs -- cpp ****")
            output_funcs(only_cpp, ofile=diff_file, append=True, prefix="CO: ")

    if args.gen_func_lookup:
        if not args.xrt_src:
            sys.exit("XRT source directory is required to functions mangled names lookup table")
        if not args.bld_dir:
            sys.exit("build directory is required to generate functions mangled names lookup table")
        script_dir = os.path.dirname(os.path.abspath(__file__))
        out_f = args.bld_dir + "/funcs_mangled_lookup.cpp"
        mangled_names_f = cpp_mangled_name_parser.gen_mangled_funcs_names(funcs=adjust_funcs_h, xrt_src_root=args.xrt_src,
            script_dir=script_dir, build_dir=args.bld_dir, out_cpp=out_f)

    if args.gen_hook:
        print(f"**** Generating hooking functions to \"{args.gen_hook}\" ****")
        if not args.func_mangled_map:
            sys.exit("not functions mangled name mapping is specified. please use --func_mangled_map to specify the mapping file.")
        cpp_mangled_name_parser.gen_wrapper_funcs(funcs=adjust_funcs_h, class_dict=types_map, out_cpp_dir=args.gen_hook, func_mangled_map_f=args.func_mangled_map)

if __name__ == '__main__':
    main()
