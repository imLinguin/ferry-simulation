project(
  'ferry-simulation',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

include = [include_directories('include')]

subdir('src')
if get_option('enable-tests')
  subdir('tests')
endif

exe = executable(
  'ferry-simulation',
  main_sources,
  include_directories: include,
  install : true,
)
