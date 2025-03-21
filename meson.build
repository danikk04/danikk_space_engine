project(
	'game', 
	'cpp',
    default_options: [
        'cpp_std=c++20',
    ]
)

danikk_engine_proj = subproject('danikk_engine')

include = [
    danikk_engine_proj.get_variable('include'),
    include_directories('include')
]

subdir('source')

executable(
    'game', 
    sources,
    dependencies : danikk_engine_proj.get_variable('deps'),
    include_directories : include,
    link_args : ['-rdynamic'],
)
