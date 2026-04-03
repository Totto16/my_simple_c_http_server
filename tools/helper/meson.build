pnpm = find_program(
    'pnpm',
    required: true,
)

helper_script_depend_files = files(
    'index.ts',
    'src/all_cases.ts',
    'src/compression.ts',
    'src/generator.ts',
    'src/hpack.ts',
    'src/log.ts',
    'src/subcommands.ts',
    'src/utils.ts',
    'src/variants/base.ts',
    'src/variants/data.ts',
    'src/variants/index.ts',
    'src/ws_tests.ts',
)

helper_script_depends = []

meson_wrapper_sh = find_program(meson.current_source_dir() / 'meson_wrapper.sh')

install_dependencies = custom_target(
    'install dependencies (helper script)',
    command: [
        meson_wrapper_sh,
        '--wrapper-generate',
        '@OUTPUT0@',
        '--link-file',
        '@CURRENT_SOURCE_DIR@/pnpm-lock.yaml',
        '@OUTPUT1@',
        pnpm,
        '--dir',
        meson.current_source_dir(),
        'exec',
        pnpm,
        'install',
        '--loglevel',
        'error',
    ],
    output: ['install_dependencies_date.txt', 'pnpm-lock-link.yaml'],
    depend_files: ['package.json'],
)
helper_script_depends += install_dependencies

type_check_helper_script = custom_target(
    'typecheck code (helper script)',
    command: [
        meson_wrapper_sh,
        '--wrapper-generate',
        '@OUTPUT@',
        pnpm,
        '--dir',
        meson.current_source_dir(),
        'exec',
        'tsc',
        '-p',
        'tsconfig.json',
    ],
    output: ['typecheck_code_date.txt'],
    depend_files: ['tsconfig.json'],
    depends: [install_dependencies],
)
helper_script_depends += type_check_helper_script

lint_helper_script = custom_target(
    'lint code (helper script)',
    command: [
        meson_wrapper_sh,
        '--wrapper-generate',
        '@OUTPUT@',
        pnpm,
        '--dir',
        meson.current_source_dir(),
        'run',
        'lint',
    ],
    output: ['lint_code_date.txt'],
    depend_files: helper_script_depend_files,
    depends: [install_dependencies],
)
helper_script_depends += lint_helper_script

build_helper_script = custom_target(
    'build typescript code (helper script)',
    command: [
        meson_wrapper_sh,
        '--wrapper-generate',
        '@OUTPUT@',
        pnpm,
        '--dir',
        meson.current_source_dir(),
        'run',
        'build',
    ],
    output: ['build_code_date.txt'],
    depend_files: helper_script_depend_files,
    depends: [install_dependencies],
)

helper_script_depends += build_helper_script

node = find_program(
    'node',
    required: false,
    version: '>=24.0.0',  # for .ts execution
)

deno = find_program(
    'deno',
    required: false,
)

bun = find_program(
    'bun',
    required: false,
)

node_like_runtime = disabler()

if node.found()
    node_like_runtime = node
elif deno.found()
    node_like_runtime = deno
elif bun.found()
    node_like_runtime = bun
else
    error('No JS runtime to generate sources')
endif


variants_generated = custom_target(
    'variants',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@',
        'generator',
        '--type',
        'c_variants',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['variants.h'],
    depend_files: helper_script_depend_files,
    depends: helper_script_depends,
)

src_files += variants_generated


generated_hpack_huffman_file_c = custom_target(
    'generated_hpack_huffman.c',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@',
        'generator',
        '--type',
        'c_hpack_huffman',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack_huffman.h', 'generated_hpack_huffman.c'],
    depend_files: helper_script_depend_files,
    depends: [helper_script_depends, variants_generated],
)

src_files += generated_hpack_huffman_file_c

generated_hpack_header_h = custom_target(
    'generated_hpack.h',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@',
        'generator',
        '--type',
        'c_header_table',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack.h', 'generated_hpack.c'],
    depend_files: helper_script_depend_files,
    depends: [helper_script_depends, variants_generated],
)

src_files += generated_hpack_header_h

generated_hpack_tests_files = custom_target(
    'generated_hpack_tests.hpp',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@',
        'generator',
        '--type',
        'cpp_tests',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_hpack_tests.hpp', 'generated_hpack_tests.cpp', 'generated_hpack_tests.c'],
    depend_files: helper_script_depend_files,
    depends: helper_script_depends,
)

generated_cpp_tests = {
    'impl': [generated_hpack_tests_files[1], generated_hpack_tests_files[2]],
    'header': [generated_hpack_tests_files[0]],
}

generated_sources = alias_target(
    'generated_sources',
    generated_hpack_huffman_file_c,
    generated_hpack_header_h,
    generated_hpack_tests_files,
    variants_generated,
)
