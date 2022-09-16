cmake_minimum_required(VERSION 2.8)
project(assignment2-rtp)

set(CMAKE_CXX_STANDARD 11)


#添加头文件搜索路径
include_directories(/usr/local/include)

#添加库文件搜索路径
link_directories(/usr/local/lib)

add_executable(sender
		# if you use recommended template, you need to cancel the comment of the next line
		#src/rtp.c
        src/util.c
        src/sender.c)
target_link_libraries(sender)

add_executable(receiver
		# if you use recommended template, you need to cancel the comment of the next line
		#src/rtp.c
		src/util.c
		src/receiver.c)
target_link_libraries(receiver)

