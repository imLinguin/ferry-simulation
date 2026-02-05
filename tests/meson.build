# Tests configuration

# Find Python3 interpreter for pytest-based tests
python3 = find_program('python3', required: true)

# Test 1: IPC unit tests (C)
test_ipc = executable('test_ipc',
    'test_ipc.c',
    include_directories: include,
    link_with: [common_lib]
)

# Test 2: Integration tests (Python)
# The Python tests will be run via pytest
pytest_integration = custom_target('pytest_integration',
    output: 'pytest_integration.dummy',
    build_by_default: false,
    command: [
        python3, '-m', 'pytest',
        meson.current_source_dir() / 'test_integration.py',
        '-v',
        '--tb=short'
    ]
)

# Register tests with meson
test('IPC Unit Tests', test_ipc,
    timeout: 60,
    is_parallel: false,
    priority: 100
)

# Python integration tests
test('Integration Tests (Python)', python3,
    args: [
        '-m', 'pytest',
        meson.current_source_dir() / 'test_integration.py',
        '-v',
        '--tb=short',
        '-m', 'not stress'  # Skip stress tests by default
    ],
    timeout: 300,
    is_parallel: false,
    priority: 50,
    workdir: meson.project_build_root()
)


message('Test suite enabled')
message('  - test_ipc: Unit tests for IPC functions')
message('  - test_integration.py: Integration tests for simulation (Python)')
message('  - Python requirements: tests/requirements.txt')
