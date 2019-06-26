conway: main.c
	gcc -o conway -g3 -Wall -std=c99 main.c glad.c -l OpenCL -l OpenGL -l glfw -l dl