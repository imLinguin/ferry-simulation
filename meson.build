project(
  'ferry-simulation',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

include = [include_directories('include')]

common_sources = []
main_sources = []
ferry_capitan_sources = []
passenger_sources = []
port_capitan_sources = []

subdir('src')
if get_option('enable-tests')
  subdir('tests')
endif

common_lib = library(
  'ferry-common',
  common_sources,
  include_directories: include
)

main_exe = executable(
  'ferry-simulation',
  main_sources,
  include_directories: include,
  install : true,
  link_with: common_lib
)

ferry_capitan_exe = executable(
  'ferry-capitan',
  ferry_capitan_sources,
  include_directories: include,
  install : true,
  link_with: common_lib
)

port_capitan_exe = executable(
  'port-capitan',
  port_capitan_sources,
  include_directories: include,
  install : true,
  link_with: common_lib
)

passenger_exe = executable(
  'passenger',
  passenger_sources,
  include_directories: include,
  install : true,
  link_with: common_lib
)
