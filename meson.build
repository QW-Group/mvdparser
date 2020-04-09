project('mvdparser', 'c')

mvdparser_sources = [
    'frag_parser.c',
    'logger.c',
    'main.c',
    'mvd_parser.c',
    'net_msg.c',
    'netmsg_parser.c',
    'qw_protocol.c',
    'shared.c',
    'strptime.c',
]

if host_machine.system() == 'windows'
    mvdparser_sources += 'sys_win.c'
elif host_machine.system() == 'linux'
    mvdparser_sources += 'sys_linux.c'
else
    # FIXME macOS / other platform support
endif

executable('mvdparser', mvdparser_sources,
    dependencies : [
        meson.get_compiler('c').find_library('m', required : false),
        meson.get_compiler('c').find_library('winmm', required : false),
    ],
)
