


npx = find_program(
    'npx',
    required: false,
)

pnpm = find_program(
    'pnpm',
    required: false,
)


helper_script_depend_files = files('index.ts', 'package.json', 'tsconfig.json')

helper_script_depends = []

if npx.found() and pnpm.found()

    meson_wrapper_sh = find_program(meson.current_source_dir() / 'meson_wrapper.sh')

    install_dependencies = custom_target(
        'install dependencies (tests helper)',
        command: [
            meson_wrapper_sh,
            '--wrapper-generate',
            '@OUTPUT@',
            pnpm,
            '--dir',
            meson.current_source_dir(),
            'exec',
            pnpm,
            'install',
            '--loglevel',
            'error',
        ],
        output: ['install_dependencies_date.txt'],
        depend_files: [helper_script_depend_files, 'pnpm-lock.yaml'],
    )
    helper_script_depends += install_dependencies

    type_check_index_ts = custom_target(
        'typecheck code (tests helper)',
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
        depend_files: helper_script_depend_files,
    )
    helper_script_depends += type_check_index_ts
endif

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
    error('No JS runtime to geneate sources')
endif

generated_test_file_cpp = custom_target(
    'generated_tests.hpp',
    command: [
        node_like_runtime,
        '@CURRENT_SOURCE_DIR@/index.ts',
        '--type',
        'cpp_tests',
        '-o',
        '@OUTPUT0@',
    ],
    output: ['generated_tests.hpp', 'generated_tests.cpp'],
    depend_files: helper_script_depend_files,
    depends: helper_script_depends,
)

generated_cpp_tests += generated_test_file_cpp

generated_sources += alias_target(
    'generated_sources_generic',
    generated_test_file_cpp,
)
