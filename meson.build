project('mvdparser', 'c')

mvdparser_sources = [
    'src/frag_parser.c',
    'src/logger.c',
    'src/main.c',
    'src/mvd_parser.c',
    'src/net_msg.c',
    'src/netmsg_parser.c',
    'src/qw_protocol.c',
    'src/shared.c',
    'src/strptime.c',
]

if host_machine.system() == 'windows'
    mvdparser_sources += 'src/sys_win.c'
elif host_machine.system() == 'linux'
    mvdparser_sources += 'src/sys_linux.c'
else
    # FIXME macOS / other platform support
endif

executable('mvdparser', mvdparser_sources,
    dependencies : [
        meson.get_compiler('c').find_library('m', required : false),
        meson.get_compiler('c').find_library('winmm', required : false),
    ],
)
