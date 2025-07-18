#!/usr/bin/env python3

import datetime
import subprocess
import os
import re
import shutil
import sys

import parse_cpp_func_args

def is_windows():
    if os.name == 'nt':
        return True
    else:
        return False

def get_class_header(decl: str):
    headers = set()
    headers.add("xrt.h")
    if re.search(r'xrt::ext::bo|xrt::bo', decl):
        headers.add("xrt/xrt_bo.h")
    if re.search(r'xrt::aie|xrt::elf', decl):
        headers.add("xrt/xrt_aie.h")
    if re.search(r'xrt::device', decl):
        headers.add("xrt/xrt_device.h")
    if re.search(r'xrt::xclbin', decl):
        headers.add("xrt/xrt_device.h")
    if re.search(r'xrt::xclbin_repository', decl):
        headers.add("xrt/xrt_device.h")
    if re.search(r'xrt::ip[:\s),&*]', decl):
        headers.add("xrt/experimental/xrt_ip.h")
    if re.search(r'xrt::kernel', decl):
        headers.add("xrt/xrt_kernel.h")
    if re.search(r'xrt::fence', decl):
        headers.add("xrt/xrt_kernel.h")
    if re.search(r'xrt::run', decl):
        headers.add("xrt/xrt_kernel.h")
    if re.search(r'xrt::hw_context', decl):
        headers.add("xrt/xrt_hw_context.h")
    if re.search(r'xrt::uuid', decl):
        headers.add("xrt/xrt_uuid.h")
    if re.search(r'xrt::mailbox', decl):
        headers.add("xrt/experimental/xrt_mailbox.h")
    if re.search(r'xrt::module', decl):
        headers.add("xrt/experimental/xrt_module.h")
    if re.search(r'xrt::runlist', decl):
        headers.add("xrt/experimental/xrt_kernel.h")
    if re.search(r'xrt::profile', decl):
        headers.add("xrt/experimental/xrt_profile.h")
    if re.search(r'xrt::queue', decl):
        headers.add("xrt/experimental/xrt_queue.h")
    if re.search(r'xrt::error', decl):
        headers.add("xrt/experimental/xrt_error.h")
    if re.search(r'xrt::ext::', decl):
        headers.add("xrt/experimental/xrt_ext.h")
    if re.search(r'xrt::ini::', decl):
        headers.add("xrt/experimental/xrt_ini.h")
    if re.search(r'xrt::message::', decl):
        headers.add("xrt/experimental/xrt_message.h")
    if re.search(r'xrt::system::', decl):
        headers.add("xrt/experimental/xrt_system.h")
    if re.search(r'xrt::aie::program', decl):
        headers.add("xrt/experimental/xrt_aie.h")
    if re.search(r'xrt::version', decl):
        headers.add("xrt/experimental/xrt_version.h")
    if re.search(r'xrt_core::fence_handle', decl):
        headers.add("core/common/api/fence_int.h")
    if re.search(r'xrt_core::fence_handle', decl):
        headers.add("core/common/api/fence_int.h")
    return list(headers)

def mem_init_construct(decl: str):
    if re.search(r"aie_error::aie_error\(", decl):
        full_args_list = re.search(r"\((.*)\)", decl).group(1).split(',')
        args_list = []
        for a in full_args_list:
            aname = re.search(r"\s[\*&]*(\w+)$", a).group(1)
            args_list.append(aname)
            args_str = ','.join(args_list)
        return f": command_error({args_str})"
    return None

def gen_decl_cpp(decl: str, cpp_file: str):
    print(f"generate cpp for {decl} to file {cpp_file}")
    sfunc_return = dict()
    sfunc_return[r"operator xrt_core::hwctx_handle"] = "return nullptr;"
    sfunc_return[r"operator xclDeviceHandle"] = "return nullptr;"
    sfunc_return[r'xrt::xclbin_repository::iterator\s*&*\s*[\w:&+->*]+\s*\(.*\)'] = "throw std::runtime_error(\"unsupported\");"
    sfunc_return[r'xrt::bo::async_handle\s*&*\s*[\w+:&+->]+\s*\(.*\)'] = "throw std::runtime_error(\"unsupported\");"
    sfunc_return[r'xrt::ip::interrupt\s*&*\s*[\w+:&+->]+\s*\(.*\)'] = "throw std::runtime_error(\"unsupported\");"
    with open(cpp_file, 'w') as cpp:
        cpp.write("#include <stdexcept>\n")
        headers = get_class_header(decl)
        for h in headers:
            cpp.write(f"#include \"{h}\"\n")
            if "boost::any" in decl:
                cpp.write("#include <boost/any.hpp>\n")
        cpp.write(f"\n{decl}\n")
        mem_init = mem_init_construct(decl)
        if mem_init:
            cpp.write(f"{mem_init}")
        match = re.search(r'(.*[\w>&*]\s+)?\s*[\s\w:]+operator[\s\w&*=+->]+\s*\(.*\)(\s*[\w\s]+)?$', decl)
        if not match:
            match = re.search(r'(.*[\w>&*]\s+)?\s*[~\w:]+\(.*\)(\s*[\w\s]+)?$', decl)
        # special handling
        has_special_handling = False
        for key, val in sfunc_return.items():
            if re.search(key, decl):
                cpp.write(f"{{\n{val}\n}}\n")
                has_special_handling = True
                break
        if not has_special_handling:
            ret_type = match.group(1)
            if not ret_type or ret_type.strip() == "void":
                if not has_special_handling:
                    # no return type
                    cpp.write("{}\n")
            else:
                ret_type = ret_type.strip()
                # TODO: will add return type with & support if there is such as case
                if ret_type.endswith("&"):
                    ret_type = re.sub(r'const\s', '', ret_type)
                    ret_type = re.sub(r'constexpr\s', '', ret_type).strip()
                    cpp.write(f"{{\n  static {ret_type[:-1]} dummy;\n  return dummy;\n}}\n")
                elif ret_type.endswith("*"):
                    cpp.write("{return nullptr;}\n")
                else:
                    cpp.write("{return {};}\n")
        cpp.close()

def build_cpp_get_mangled(build_dir, script_dir, xrt_src_root):
    bdir = build_dir
    sdir = script_dir + "/ch_mangled"
    shutil.copyfile(f"{sdir}/CMakeLists.txt", f"{bdir}/CMakeLists.txt")
    # Needs to create xrt/detail to hold the generated version.h from XRT CMake
    os.makedirs(f"{bdir}/xrt/detail", exist_ok=True)
    if is_windows():
        xrt_boost_root="C:/Xilinx/XRT/ext.new"
        subprocess.run(['cmake', '-B', bdir, '-S', bdir, f"-DXRT_BOOST_INSTALL={xrt_boost_root}", f"-DXRT_SOURCE_DIR={xrt_src_root}"], check=True)
    else:
        subprocess.run(['cmake', '-B', bdir, '-S', bdir, f"-DXRT_SOURCE_DIR={xrt_src_root}"], check=True)
    subprocess.run(['cmake', '--build', bdir, '-j'], check=True)

def get_obj_mangled_name_win(obj_f, func_match_name):
    # one object file contains only one function
    result = subprocess.run(
        ["dumpbin", "/SYMBOLS", obj_f], capture_output=True, text=True, check=True)
    for line in result.stdout.splitlines():
        if "External" in line and func_match_name in line:
            mangled_name = line.split('|')[1].split()[0]
            return mangled_name
    return None

def get_obj_mangled_name_linux(obj_f, func_match_name):
    # one object file contains only one function
    result = subprocess.run(
        f"nm {obj_f} | grep \" T \"", shell=True, capture_output=True, text=True, check=True)
    cppfilt = subprocess.run(f"nm {obj_f} | grep \" T \" | c++filt", shell=True, capture_output=True, text=True, check=True)

    mangled_names = []
    func_addr = []
    for line, readable in zip(result.stdout.splitlines(), cppfilt.stdout.splitlines()):
        if func_match_name in readable:
            mangled_names.append(line.split(' T ')[1])
            func_addr.append(line.split(' T ')[0])
    if not mangled_names:
        return None
    if len(mangled_names) > 1:
        if len(set(func_addr)) > 1:
            # in case of constructor, there can be base object constructor and complete object constructor
            # For now, we only use base object constructor
            # For Linux, we use preload library, should be fine to call the base object constructor
            sys.exit(f"{func_match_name} has more than one different address in {obj_f}")
        return sorted(mangled_names)[1]
    return mangled_names[0]

def get_obj_mangled_name(obj_f, func_name):
    # we have this opeorator override: xrt::device::operator xclDeviceHandle
    # in this case, function name detected by script will be xclDeviceHandle, but
    # what's come out from compiler can be the type aliased by the `xclDeviceHandle`
    # in this case, just check the operator
    omatch = re.search(r'(\w[\w:]*::operator)\s', func_name)
    if omatch:
        func_match_name = omatch.group(1)
    else:
        func_match_name = func_name
    if is_windows():
        return get_obj_mangled_name_win(obj_f, func_match_name)
    else:
        return get_obj_mangled_name_linux(obj_f, func_match_name)

def find_file(root_dir, base_name):
    for dirpath, _, files in os.walk(root_dir):
        for file in files:
            if file == base_name:
                return os.path.join(dirpath, file)
    return None

def sort_funcs(funcs: set):
    func_info_list = []
    for f in funcs:
        finfo = parse_cpp_func_args.get_func_info(f)
        finfo['decl'] = f
        func_info_list.append(finfo)
    return sorted(func_info_list, key=lambda item: item['func'])

def gen_mangled_funcs_names(funcs: set, xrt_src_root, script_dir: str, build_dir: str, out_cpp: str):
    ffile_map = dict()
    i = 0
    bdir = build_dir + "/ch_mangled"
    os.makedirs(f"{bdir}/src", exist_ok=True)
    func_info_list = sort_funcs(funcs)
    #print(f"get mangled functions names:\n {func_info_list}")
    for finfo in func_info_list:
        fname=f"gen_temp_{i}"
        cpp_file = f"{bdir}/src/{fname}.cpp"
        ffile_map[finfo['decl']] = fname
        gen_decl_cpp(finfo['decl'], cpp_file)
        i = i + 1

    # Compile CPP
    build_cpp_get_mangled(build_dir=bdir, script_dir=script_dir, xrt_src_root=xrt_src_root)
    # get mangled name from generated result
    fmangled_map = dict()
    for decl, cpp in ffile_map.items():
        if is_windows():
            obj_name = cpp + ".obj"
        else:
            obj_name = cpp + ".cpp.o"
        obj_file = find_file(bdir, obj_name)
        if not obj_file:
            sys.exit("failed to locate " + obj_name)
        func_info = parse_cpp_func_args.get_func_info(decl)
        func_s = func_info['func'] + "("
        if 'arg' in func_info:
            args_types_list = []
            for a in func_info['arg']:
                args_types_list.append(a[0])
            args_str = ', '.join(args_types_list)
        else:
            args_str = "void"
        func_s = func_s + args_str + ")"
        func_n = func_info['func']
        mname = get_obj_mangled_name(obj_file, func_n)
        if not mname:
            sys.exit(f"not find mangled name in {obj_file} for function: {decl}, func: {func_n}")
        fmangled_map[func_s] = mname

    print(f"wrting function name to mangled name mapping to \'{out_cpp}\'.")
    # output the demangled name and the mangled name to a cpp array
    with open(out_cpp, 'w', newline='\n') as out:
        lines = ""
        if is_windows():
          lines = lines + "#ifdef _WIN32"
        else:
          lines = lines + "#ifdef __linux__"
        lines = lines + """
#include <cstring>

extern "C" {
const char * func_mangled_map[] = {
"""
        fmangled_map = dict(sorted(fmangled_map.items()))
        for k, m in fmangled_map.items():
            lines = lines + f"  \"{k}\", \"{m}\",\n"
        lines = lines + f"""}};
}};

size_t
get_size_of_func_mangled_map(void)
{{
  return sizeof(func_mangled_map)/sizeof(func_mangled_map[0]);
}}
#endif
"""
        out.write(lines)

def get_lib_exports_linux(lib_file):
    lib_exports = subprocess.run(
        f"nm {lib_file} | grep \" T \"", shell=True, capture_output=True, text=True, check=True)
    return lib_exports.stdout
    

def get_lib_exports_win(lib_file):
    lib_exports = subprocess.run(
        ["dumpbin", "/EXPORTS", lib_file], capture_output=True, text=True, check=True)
    return lib_exports.stdout

def compare_lib_mangled_names(mangled_names_file, lib_file):
    if is_windows():
        lib_exports = get_lib_exports_win(lib_file)
    else:
        lib_exports = get_lib_exports_linux(lib_file)

    with open(mangled_names_file, 'r', encoding='utf-8') as mfile:
        lines = mfile.readlines()
        for line in lines:
            if ", \"" not in line:
                continue
            mname = re.search(r".*, \"(.*)\",", line)
            if not mname:
                sys.exit(f"compared mangled name failed, failed to get mangled name from {line}")
            mname = mname.group(1)
            if not re.search(r"\s{mname}\s|$", lib_exports):
                #sys.exit(f"manged name \'{mname}\' not in \'{lib_file}\'")
                print(f"manged name \'{mname}\' not in \'{lib_file}\'")
    print(f"compare mangled names from \'{mangled_names_file}\' to \'{lib_file}\' done.")

def gen_wrapper_funcs(funcs: set, class_dict: dict, out_cpp_dir: str, func_mangled_map_f):
    os.makedirs(f"{out_cpp_dir}", exist_ok=True)
    func_info_list = sort_funcs(funcs)
    class_file_map = dict()
    class_file_map['xrt'] = "hook_xrt.cpp"
    class_file_map['xrt::aie'] = "hook_xrt_aie.cpp"
    class_file_map['xrt::bo'] = "hook_xrt_bo.cpp"
    class_file_map['xrt::device'] = "hook_xrt_device.cpp"
    class_file_map['xrt::elf'] = "hook_xrt_elf.cpp"
    class_file_map['xrt::error'] = "hook_xrt_error.cpp"
    class_file_map['xrt::ext::bo'] = "hook_xrt_ext_bo.cpp"
    class_file_map['xrt::ext::kernel'] = "hook_xrt_ext_kernel.cpp"
    class_file_map['xrt::fence'] = "hook_xrt_fence.cpp"
    class_file_map['xrt::hw_context'] = "hook_xrt_hw_context.cpp"
    class_file_map['xrt::ini'] = "hook_xrt_ini.cpp"
    class_file_map['xrt::ip'] = "hook_xrt_ip.cpp"
    class_file_map['xrt::kernel'] = "hook_xrt_kernel.cpp"
    class_file_map['xrt::mailbox'] = "hook_xrt_mailbox.cpp"
    class_file_map['xrt::module'] = "hook_xrt_module.cpp"
    class_file_map['xrt::message'] = "hook_xrt_message.cpp"
    class_file_map['xrt::profile'] = "hook_xrt_profile.cpp"
    class_file_map['xrt::queue'] = "hook_xrt_queue.cpp"
    class_file_map['xrt::run'] = "hook_xrt_run.cpp"
    class_file_map['xrt::runlist'] = "hook_xrt_runlist.cpp"
    class_file_map['xrt::system'] = "hook_xrt_system.cpp"
    class_file_map['xrt::version'] = "hook_xrt_version.cpp"
    class_file_map['xrt::xclbin'] = "hook_xrt_xclbin.cpp"
    class_file_map['xrt::xclbin_repository'] = "hook_xrt_xclbin.cpp"

    func_file_map = dict()
    func_file_map["xrt::operator==(const xrt::device&, const xrt::device&)"] = "hook_xrt_device.cpp"
    func_file_map["xrt::set_read_range(const xrt::kernel&, uint32_t, uint32_t)"] = "hook_xrt_kernel.cpp"

    # get mangled name from generated result
    mangled_names_file = func_mangled_map_f

    fmangled_map = dict()
    with open(mangled_names_file, 'r', encoding='utf-8') as mfile:
        lines = mfile.readlines()
        for line in lines:
            if "\", \"" not in line:
                continue
            match = re.search(r"\"(.*)\", \"(.*)\",", line)
            if not match:
                sys.exit(f"compared mangled name failed, failed to get mangled name from {line}")
            fsignature = match.group(1)
            fmname = match.group(2)
            fmangled_map[fsignature] = fmname

    hook_xrt_h_f = out_cpp_dir + "/hook_xrt.h"
    xrt_headers = []
    xrt_headers.append("chrono")
    xrt_headers.append("typeinfo")
    xrt_headers.append("xrt.h")
    xrt_headers.append("xrt/xrt_bo.h")
    xrt_headers.append("xrt/xrt_aie.h")
    xrt_headers.append("xrt/xrt_device.h")
    xrt_headers.append("xrt/xrt_hw_context.h")
    xrt_headers.append("xrt/xrt_kernel.h")
    xrt_headers.append("xrt/xrt_uuid.h")
    xrt_headers.append("xrt/experimental/xrt_ip.h")
    xrt_headers.append("xrt/experimental/xrt_mailbox.h")
    xrt_headers.append("xrt/experimental/xrt_module.h")
    xrt_headers.append("xrt/experimental/xrt_kernel.h")
    xrt_headers.append("xrt/experimental/xrt_profile.h")
    xrt_headers.append("xrt/experimental/xrt_queue.h")
    xrt_headers.append("xrt/experimental/xrt_error.h")
    xrt_headers.append("xrt/experimental/xrt_ext.h")
    xrt_headers.append("xrt/experimental/xrt_ini.h")
    xrt_headers.append("xrt/experimental/xrt_message.h")
    xrt_headers.append("xrt/experimental/xrt_system.h")
    xrt_headers.append("xrt/experimental/xrt_aie.h")
    xrt_headers.append("xrt/experimental/xrt_version.h")
    xrt_headers.append("core/common/api/fence_int.h")
    xrt_headers.append("google/protobuf/timestamp.pb.h")
    xrt_headers.append("func.pb.h")
    xrt_headers.append("wrapper/tracer.h")
    with open(hook_xrt_h_f, 'w', newline='\n') as out:
        lines = """
#define XCL_DRIVER_DLL_EXPORT
#define XRT_API_SOURCE
#include "xrt/detail/config.h"
"""
        for h in xrt_headers:
            lines = lines + f"#include <{h}>\n"
        out.write(f"{lines}")

    for decl in sorted(funcs):
        is_destructor = False
        if "::~" in decl:
          is_destructor = True
          # skip destructors
          continue
        func_info = parse_cpp_func_args.get_func_info(decl)
        if 'return' in func_info:
            func_ret = func_info['return']
        else:
            func_ret = None
        if 'props' in func_info:
            func_p = f" {func_info['props'].strip()}"
        else:
            func_p = ""
        func_s = func_info['func'] + "("
        if 'arg' in func_info:
            args_types_list = []
            args_names_list = []
            for a in func_info['arg']:
                args_types_list.append(a[0])
                if "std::unique_ptr" in a[0] or "&&" in a[0]:
                    args_names_list.append(f"std::move({a[1]})")
                else:
                    args_names_list.append(a[1])
            args_type_str = ', '.join(args_types_list)
            args_name_str = ', '.join(args_names_list)
        else:
            args_type_str = "void"
            args_name_str = ""
        func_s = func_s + args_type_str + ")"
        func_name = func_info['func']
        mname = fmangled_map[func_s]
        if not mname:
            sys.exit(f"failed to get mangled name for \'{func_s}\'.")
        func_c_match = re.search(r"([\w:]+)::(operator\s+[\w:*&\s]+)$", func_name);
        if not func_c_match:
            func_c_match = re.search(r"([\w:]+)::([\w=\*&\-\+<>\s~]+)$", func_name)
        func_f = None
        if func_c_match:
            func_c = func_c_match.group(1)
            func_n = func_c_match.group(2)
        else:
            sys.exit(f"function\'{decl}\', func_name: \'{func_name}\', doesn't have class information.")
        if func_c_match:
            func_c_tmp = func_c_match.group(1).split("::")
            while func_c_tmp:
                func_c_tmp_str = "::".join(func_c_tmp)
                if func_c_tmp_str in class_file_map:
                    func_f = class_file_map[func_c_tmp_str]
                    break
                func_c_tmp.pop()
        if not func_f:
            if func_s in func_file_map:
                func_f = func_file_map[func_s]
        if not func_f:
            sys.exit(f"failed to get generated cpp file for \'{func_n}\', class: {func_c}.")
        if not os.path.exists(func_f):
            with open(func_f, 'w', newline='\n') as out:
                out.write("#include <wrapper/hook_xrt.h>\n")
        args_list = []
        if 'arg' in func_info:
            for a in func_info['arg']:
                args_list.append(' '.join(a))
        args_str = ', '.join(args_list)
        is_constructor = False
        if not func_ret and not re.search(r"~", func_info['func']) and not re.search(r"operator[\s\-\+=>&\*]+", func_n):
            if not func_c_match:
                sys.exit(f"failed to define function type for constructor \'{decl}\', no class detected.")
            func_type_ret = f"{func_c}*"
            is_constructor = True
        elif re.search(r"~", func_info['func']):
            func_type_ret = "void"
        else:
            func_type_ret = f"{func_ret}"
        operator_special_ret = re.search(r"operator\s([\w\s:]+[\*]*)\s*", func_n)
        if operator_special_ret:
            func_type_ret = operator_special_ret.group(1)
        is_class_member = False
        if func_c in class_dict:
            # TODO: how about static class function, we need detection in future too
            if "static" not in decl and class_dict[func_c] == "class":
                if not re.search(r"operator\s*==]", decl):
                    is_class_member = True
        if is_constructor:
            if args_type_str != "" and args_type_str != "void":
                func_type_def = f"typedef {func_type_ret} (*func_t)(void*, {args_type_str})"
            else:
                func_type_def = f"typedef {func_type_ret} (*func_t)(void*)"
        elif is_destructor:
            func_type_def = f"typedef {func_type_ret} (*func_t)(void*)"
        elif is_class_member:
            if args_type_str != "" and args_type_str != "void":
                func_type_def = f"typedef {func_type_ret} ({func_c}::*func_t)({args_type_str})"
            else:
                func_type_def = f"typedef {func_type_ret} ({func_c}::*func_t)(void)"
            if func_p and "const" in func_p:
                func_type_def = f"{func_type_def} const"
        else:
            func_type_def = f"typedef {func_type_ret} (*func_t)({args_type_str})"

        constructor_mem_init = dict()
        aie_error_lambda = lambda arg_names_str: (
                "command_error(" + re.sub(r",[^,]+$", "", arg_names_str) + ", \"\")")
        constructor_mem_init["xrt::runlist::aie_error"] = aie_error_lambda
        constructor_mem_init["xrt::run::aie_error"] = aie_error_lambda

        with open(func_f, 'a', newline='\n') as out:
            print(f"gen hook: \"{func_f}\"")
            if "boost::any" in decl:
                lines = "#include <boost/any.hpp>\n\n"
            else:
                lines = "\n"
            if func_ret:
                lines = lines + f"{func_ret}\n"
            lines = lines + f"""{func_c}::
{func_n}({args_str}){func_p}"""
            if is_constructor and func_c in constructor_mem_init:
                mem_init_str = constructor_mem_init[func_c](args_name_str)
                lines = lines + f"\n:{mem_init_str}\n"
            lines = lines + f"""
{{
  const char* func_s = \"{func_s}\";
  {func_type_def};
  xbtracer_proto::Func func_entry;
  proc_addr_type paddr_ptr = nullptr;
  func_t ofunc = nullptr;
  void **ofunc_ptr = (void **)&ofunc;
  bool need_trace = false;
"""
            # there are classes don't have get_handle(), for these functions
            # do not call xbtracer_init_member_func_entry()
            class_no_get_handle = []
            class_no_get_handle.append("xrt::error")
            class_no_get_handle.append("xrt::profile::user_event")
            class_no_get_handle.append("xrt::profile::user_range")
            class_special_handle = dict()
            class_special_handle["xrt::queue"] = "m_impl"
            func_init_entry_str = "xbtracer_init_func_entry(func_entry, need_trace, func_s, paddr_ptr);"
            if is_destructor:
                if func_c in class_special_handle:
                    handle_str = "this->" + class_special_handle[func_c]
                    func_init_entry_str = f"xbtracer_init_destructor_entry({handle_str}, func_entry, need_trace, func_s, paddr_ptr);"
                elif func_c not in class_no_get_handle:
                    func_init_entry_str = f"xbtracer_init_destructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);"
            elif is_constructor:
                if func_c in class_special_handle:
                    handle_str = "this->" + class_special_handle[func_c]
                    func_init_entry_str = f"xbtracer_init_constructor_entry({handle_str}, func_entry, need_trace, func_s, paddr_ptr);"
                elif func_c not in class_no_get_handle:
                    func_init_entry_str = f"xbtracer_init_constructor_entry_handle(func_entry, need_trace, func_s, paddr_ptr);"
            elif is_class_member and func_c:
                if func_c in class_special_handle:
                    handle_str = "this->" + class_special_handle[func_c]
                    func_init_entry_str = f"xbtracer_init_member_func_entry({handle_str}, func_entry, need_trace, func_s, paddr_ptr);"
                elif func_c not in class_no_get_handle:
                    func_init_entry_str = f"xbtracer_init_member_func_entry_handle(func_entry, need_trace, func_s, paddr_ptr);"

            lines = lines + f"""
  {func_init_entry_str}
  xbtracer_write_protobuf_msg(func_entry, need_trace);
  *ofunc_ptr = (void*)paddr_ptr;
"""
            if is_constructor:
                if args_name_str != "" and args_name_str != "void":
                    ofunc_args_str = f"(void*)this, {args_name_str}"
                else:
                    ofunc_args_str = "(void*)this"
            elif is_destructor:
                ofunc_args_str = "(void*)this"
            else:
                ofunc_args_str = f"{args_name_str}"
            if is_constructor or is_destructor:
                lines = lines + f"""
  ofunc({ofunc_args_str});
"""
            elif is_class_member and func_type_ret and not re.search(r"void$", func_type_ret):
                lines = lines + f"""
  {func_type_ret} ret_o = (this->*ofunc)({ofunc_args_str});
"""
            elif is_class_member:
                lines = lines + f"""
  (this->*ofunc)({ofunc_args_str});
"""
            elif func_type_ret and not re.search(r"void$", func_type_ret):
                lines = lines + f"""
  {func_type_ret} ret_o = ofunc({ofunc_args_str});
"""
            else:
                lines = lines + f"""
  ofunc({ofunc_args_str});
"""
            lines = lines + """
  xbtracer_proto::Func func_exit;"""
            func_init_exit_str = "xbtracer_init_func_exit(func_exit, need_trace, func_s);"
            if is_destructor:
                func_init_exit_str = "xbtracer_init_destructor_exit(func_exit, need_trace, func_s);"
            elif is_constructor:
                if func_c in class_special_handle:
                    handle_str = "this->" + class_special_handle[func_c]
                    func_init_exit_str = f"xbtracer_init_constructor_exit({handle_str}, func_exit, need_trace, func_s);"
                elif func_c not in class_no_get_handle:
                    func_init_exit_str = f"xbtracer_init_constructor_exit_handle(func_exit, need_trace, func_s);"
            else:
                if is_class_member and func_c:
                    if func_c in class_special_handle:
                        handle_str = "this->" + class_special_handle[func_c]
                        func_init_exit_str = f"xbtracer_init_member_func_exit({handle_str}, func_exit, need_trace, func_s);"
                    elif func_c not in class_no_get_handle:
                        func_init_exit_str = f"xbtracer_init_member_func_exit_handle(func_exit, need_trace, func_s);"

            lines = lines + f"""
  {func_init_exit_str}
  xbtracer_write_protobuf_msg(func_exit, need_trace);
"""
            if not is_constructor and func_type_ret and not re.search(r"void$", func_type_ret):
                lines = lines + f"""
  return ret_o;
"""
            lines = lines + """}
"""
            out.write(lines)
