#!/usr/bin/env python3

import re
import sys
from typing import List, Tuple

def inject_func_args_names(decl: str):
    # Find argument list
    match = re.search(r'\((.*)\)', decl)
    if not match:
        return decl  # Not a function declaration

    # Split arguments by commas, but not inside parentheses or angle brackets
    args = []
    paren_depth = 0
    angle_depth = 0
    current = ''
    args_str = match.group(1).strip()
    if not args_str or args_str == 'void':
        return decl
    for c in args_str:
        if c == ',' and paren_depth == 0 and angle_depth == 0:
            args.append(current.strip())
            current = ''
        else:
            if c == '(':
                paren_depth += 1
            elif c == ')':
                paren_depth -= 1
            elif c == '<':
                angle_depth += 1
            elif c == '>':
                angle_depth -= 1
            current += c
    if current.strip():
        args.append(current.strip())
    new_args = []
    i = 0
    for arg in args:
        amatch = re.search(r'[*&>]\s*,$', arg.strip() + ",")
        if not amatch:
            amatch = re.search(r'^\w+\s*,$', arg.strip() + ",")
        if not amatch:
            amatch = re.search(r'(const|constexpr|__restrict__|restrct)\s*,$', arg.strip() + ",")
        # no argument pattern
        if amatch:
            arg_name = f'arg{i+1}'
            arg_type = arg
            new_args.append(f'{arg_type} {arg_name}')
        else:
            new_args.append(arg)
        i = i + 1
    # Reconstruct declaration
    new_decl = decl[:match.start(1)] + ', '.join(new_args) + decl[match.end(1):]
    return new_decl

def rm_func_default_vals(func_decl: str):
    # Pattern to match default values in function arguments
    pattern = re.compile(r'(\w[\w\s\*&:<>]*\w)\s*=\s*[^,)\n]+')
    # Find the argument list
    match = re.search(r'\((.*)\)', func_decl, re.DOTALL)
    if not match:
        return func_decl  # Not a function declaration
    args = match.group(1)
    # Remove default values
    new_args = pattern.sub(r'\1', args)
    # Reconstruct the function declaration
    return func_decl[:match.start(1)] + new_args + func_decl[match.end(1):]

def remove_constraints(type_str: str) -> str:
    # Remove C++ type constraints (const, volatile, restrict, &, &&, *, etc.)
    type_str = re.sub(r'\b(constexpr|const|volatile|restrict)\b', '', type_str)
    type_str = re.sub(r'[\s&*]+$', '', type_str)  # Remove trailing &, *, spaces
    type_str = re.sub(r'^\s*', '', type_str)      # Remove leading spaces
    type_str = re.sub(r'\s+', ' ', type_str)      # Normalize spaces
    return type_str.strip()

# TODO: there can be better way to merge func and operator header/C pattern
func_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT|XCL_DRIVER_DLLESPEC\s+)?  # export disclaimer
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+[&*]?)?    # return type (may be empty for ctor/dtor)
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<func>[~]?[a-zA-Z_]\w*)\s*               # function name (may start with ~ for dtor)
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
        (?P<mem_init>:\s*[a-zA-Z]\w+\s*\(.+\))?\s*  # member initialization of contructor
    ''', re.VERBOSE | re.DOTALL
)
operator_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT\s+)?  # export disclaimer
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+)?    # return type (may be empty for ctor/dtor)
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<op>operator\s*.*)\s*               # operator name
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
    ''', re.VERBOSE | re.DOTALL
)

func_h_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT)\s+  # export disclaimer
        (?P<explicit>\s*explicit\s+)?                  # explicit property
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+[&*]?)?    # return type (may be empty for ctor/dtor)
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<func>[~]?[a-zA-Z_]\w*)\s*               # function name (may start with ~ for dtor)
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
        (?P<mem_init>:\s*[a-zA-Z]\w+\s*\(.+\))?\s*  # member initialization of contructor
        (?:\{|;)                                    # function body or declaration
    ''', re.VERBOSE | re.DOTALL
)
func_c_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT|XCL_DRIVER_DLLESPEC\s+)?  # export disclaimer
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+[&*]?)?    # return type (may be empty for ctor/dtor)
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<func>[~]?[a-zA-Z_]\w*)\s*               # function name (may start with ~ for dtor)
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
        (?P<mem_init>:\s*[a-zA-Z]\w+\s*\(.+\))?\s*  # member initialization of contructor
        (?:\{)                                    # function body or declaration
    ''', re.VERBOSE | re.DOTALL
)
operator_h_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT)\s+  # export disclaimer
        (?P<explicit>\s*explicit\s+)?                  # explicit property
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+)?    # return type
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<func>operator\s*.*)\s*               # operator name
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
        (?:\{|;)                                    # function body or declaration
    ''', re.VERBOSE | re.DOTALL
)
operator_c_pattern = re.compile(
    r'''(?P<export>\s*XCL_DRIVER_DLLESPEC|XRT_API_EXPORT\s+)?  # export disclaimer
        (?P<ret_type>[a-zA-Z_][\w:<>\s*&~\*,]*\s+)?    # return type (may be empty for ctor/dtor)
        (?P<class>[a-zA-Z_][\w:]*::)?               # optional class name
        (?P<func>operator\s*.*)\s*               # operator name
        \((?P<args>[\(\)\w\*&,:<>\s=.{}]*)\)\s*                     # arguments
        (?P<props>(?:const)?\s*(?:noexcept)?\s*(?:override)?)\s*    # properties
        (?:\{)                                    # function body or declaration
    ''', re.VERBOSE | re.DOTALL
)

def search_func_op(line, is_header=False):
    if is_header:
        lfunc_pattern = func_h_pattern
        loperator_pattern = operator_h_pattern
    else:
        lfunc_pattern = func_c_pattern
        loperator_pattern = operator_c_pattern
    match = loperator_pattern.search(line)
    if not match:
        match = lfunc_pattern.search(line)
    return match

def extract_types(type_str: str) -> dict:
    # Recursively extract nested types
    type_str = remove_constraints(type_str)
    result = {"type": None, "template_args": []}
    match = re.match(r'([a-zA-Z_][\w:]*)\s*<(.+)>', type_str)
    if match:
        result["type"] = match.group(1)
        args_str = match.group(2)
        args = []
        depth = 0
        current = ''
        for c in args_str + ',':
            if c == '<':
                depth += 1
                current += c
            elif c == '>':
                depth -= 1
                current += c
            elif c == ',' and depth == 0:
                args.append(current.strip())
                current = ''
            else:
                current += c
        result["template_args"] = [extract_types(arg) for arg in args if arg]
    else:
        result["type"] = type_str
    return result

def get_args_list(args_str):
    args_list = []
    depth = 0
    current = ''
    for c in args_str + ',':
        if c == '(':
            depth += 1
            current += c
        elif c == ')':
            depth -= 1
            current += c
        elif c == '<':
            depth += 1
            current += c
        elif c == '>':
            depth -= 1
            current += c
        elif c == ',' and depth == 0:
            args_list.append(current.strip())
            current = ''
        else:
            current += c
    return args_list

def parse_cpp_function_declaration(decl: str):
    # Remove trailing semicolon and extra spaces
    decl = decl.strip().rstrip(';')
    # Find return type and function name
    #m = re.match(r'(.+?)\s+([a-zA-Z_][\w:]*)\s*\((.*)\)', decl)
    #func_regex = r'(.+?)?\s+([a-zA-Z_][\w:]*|operator\s*[^\s(]*)\s*\((.*)\)'
    m = operator_pattern.match(decl)
    if m:
        func_name = m.group('op').strip()
    else:
        m = func_pattern.match(decl)
        if not m:
            print("Invalid function declaration.")
            return
        func_name = m.group('func').strip()
        
    #m = re.match(func_regex, decl)
    return_type = m.group('ret_type')
    class_name = m.group('class')
    if not return_type:
        return_type = ""
    args_str = m.group('args')
    if return_type:
        return_type = return_type.strip()
    else:
        return_type = ""
    if class_name:
        class_name = re.sub(r"::$", "", class_name).strip()
    if args_str:
        args_str = args_str.strip()
    else:
        args_str = ""
    # Parse arguments
    args = []
    if args_str:
        arg_list = get_args_list(args_str)
        for arg in arg_list:
            if not arg:
                continue
            # Remove default values and argument names
            arg_type = re.split(r'=[^,]+', arg)[0].strip()
            arg_type = re.sub(r'\s+\w+$', '', arg_type)
            args.append(arg_type)
    # Print details
    ret_types = extract_types(return_type)
    if not ret_types:
        ret_types = {}
    #print("Return type:")
    #print_type_details(ret_types, indent=2)
    args_types = []
    #print("Argument types:")
    for i, arg_type in enumerate(args):
        args_types.append(extract_types(arg_type))
        #print(f"  Arg {i+1}:")
        #print_type_details(extract_types(arg_type), indent=4)
    #print(f"{ret_types}")
    #print(f"{args_types}")
    return func_name, class_name, ret_types, args_types

def print_type_details(type_info: dict, indent=0):
    prefix = ' ' * indent
    print(f"{prefix}Type: {type_info['type']}")
    if type_info['template_args']:
        print(f"{prefix}Template arguments:")
        for t in type_info['template_args']:
            print_type_details(t, indent=indent+2)

def get_func_info(decl: str):
    match = re.search(r'(?P<ret>[a-zA-Z_][\w&\*\s<>,:]*\s+[&\*]?)?\s*(?P<func>\w[\w:]+operator\s+[\w:*&]+)\s*\((?P<arg>.*)\)(?P<props>[\s\w]+)?$', decl)
    if not match:
        match = re.search(r'(?P<ret>[a-zA-Z_][\w&\*\s<>,:]*\s+[&\*]?)?\s*(?P<func>\w[\w:\-\+=\~]*[&\->\+=\*]?)\s*\((?P<arg>.*)\)(?P<props>[\s\w]+)?$', decl)
    if not match:
        sys.exit(f"failed to get function info \"{decl}\" is invalid")
    result = dict()
    if match.group('ret'):
        result['return'] = match.group('ret').strip()
    result['func'] = match.group('func').strip()
    if 'return' in result and re.search(r'::operator$', result['return']):
        result['func'] = result['return'] + " " + result['func']
        del result['return']
    if match.group('arg'):
        args = get_args_list(match.group('arg').strip())
        args_list = []
        for a in args:
            amatch = re.search(r'(.*)\s+(\w+)$', a)
            if not amatch:
                amatch = re.search(r'(.*\s+[&*])\s*(\w+)$', a)
            args_list.append([amatch.group(1).strip(), amatch.group(2).strip()])
        result['arg'] = args_list
    if match.group('props'):
        result['props'] = match.group('props')
    return result
