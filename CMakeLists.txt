cmake_minimum_required(VERSION 3.4.0)

set(CMAKE_VERBOSE_MAKEFILE ON)

# Set project name and languge.
project(mvdparser C)


######################################################################################################

# Set where sources located.
set(DIR_SRC "src")

# Add sources
set(SRC_COMMON
	"${DIR_SRC}/frag_parser.c"
	"${DIR_SRC}/logger.c"
	"${DIR_SRC}/main.c"
	"${DIR_SRC}/mvd_parser.c"
	"${DIR_SRC}/net_msg.c"
	"${DIR_SRC}/netmsg_parser.c"
	"${DIR_SRC}/qw_protocol.c"
	"${DIR_SRC}/shared.c"
	)

# Check build target, and included sources
if(UNIX)
	list(APPEND SRC_COMMON
		"${DIR_SRC}/sys_linux.c"
	)
else()
	list(APPEND SRC_COMMON
		"${DIR_SRC}/strptime.c"
		"${DIR_SRC}/sys_win.c"
	)
endif()


######################################################################################################

# Set base compiler flags
set(CFLAGS)
set(LFLAGS)


######################################################################################################

# Set target
add_executable(${PROJECT_NAME} ${SRC_COMMON})
set_target_properties(${PROJECT_NAME}
	PROPERTIES #PREFIX "" # Strip lib prefix.
	C_VISIBILITY_PRESET hidden # Hide all symbols unless excplicitly marked to export.
	)


######################################################################################################

# Set include directories
target_include_directories(${PROJECT_NAME} PRIVATE)


######################################################################################################

# Check build target, and included sources and libs
if(UNIX)
	target_link_libraries(${PROJECT_NAME} m)
else()
	target_link_libraries(${PROJECT_NAME} winmm)
	set(CFLAGS --enable-auto-import)
	set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc")
endif()


######################################################################################################

# Assign compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE ${CFLAGS})


######################################################################################################
