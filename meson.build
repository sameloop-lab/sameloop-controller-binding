project('sameloop-controller-binding', 'c', version: '0.1.0', license: 'MIT',
        meson_version: '>=0.61.0', default_options: ['c_std=c11', 'warning_level=3', 'werror=true'])
cc = meson.get_compiler('c')
afb = dependency('afb-binding', version: '>=4.2.0', include_type: 'system')
jsonc = dependency('json-c', version: '>=0.13')
threads = dependency('threads')
math = cc.find_library('m', required: true)
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')
shared_module('sameloop-controller-binding', 'src/binding.c',
    name_prefix: '', dependencies: [afb, jsonc, threads, math],
    install: true, install_dir: get_option('libdir') / 'sameloop')
install_data('README.md', 'LICENSE', install_dir: get_option('datadir') / 'doc' / meson.project_name())

model_test = executable('test-model', 'tests/test-model.c', include_directories: include_directories('src'), dependencies: math)
test('model', model_test)
