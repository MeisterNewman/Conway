//An OpenCL/OpenGL implementation of Conway's game of life by Stephen Newman.
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <string.h>
#include <time.h> 

#include <glad/glad.h>

// #ifdef __APPLE__
// #include <OpenCL/opencl.h>
// #define APPLE (1)
// #else
#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_gl_ext.h>
#define APPLE (0)
// #endif

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#endif

#ifdef __linux__
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif

// #define GLEW_STATIC
// #include <GL/glew.h>
//#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


#define MAX_SOURCE_SIZE (0x100000)

//Setup sourced heavily from https://www.eriksmistad.no/getting-started-with-opencl-and-gpu-computing/

cl_platform_id platform_id;
cl_device_id device_id;
cl_uint ret_num_devices;
cl_uint ret_num_platforms;
cl_int ret;

cl_context context;
cl_command_queue command_queue;

cl_program program;

GLFWwindow* window;


cl_kernel calculateAdjacencies;
cl_kernel zeroAdjacencies;
cl_kernel updateState;
cl_kernel writeStateToImage;
cl_kernel initializeState;
cl_kernel flipSquare;

int window_width; int window_height;
int game_width; int game_height;

#define BORDER_WIDTH (25)
//Border is 2, dead is 0, alive is 1.

#define BOARD_TEXTURE_TYPE (GL_TEXTURE_2D)

GLuint board_texture;
cl_mem CL_board_texture;

GLuint vao;
GLuint vbo;
GLuint ebo;

GLFWmonitor* monitor;


GLint texAttrib;
GLint colAttrib;
GLint posAttrib;

cl_mem game_state;
cl_mem adjacencies;


GLuint shaderProgram;


double rawScroll;

int powerOfTwoAbove(int i){
	int p=1;
	while (p<i){
		p*=2;
	}
	return p;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
	rawScroll+=yoffset;
}

int texture_size; //Switching to power-of-two textures


float clip (float val, float min, float max){
	if (val<min){
		return min;
	}
	if (val>max){
		return max;
	}
	return val;
}

void glInit(){
	//Initialization taken from http://www.opengl-tutorial.org/beginners-tutorials/tutorial-1-opening-a-window/
	if (glfwInit() != GLFW_TRUE){
		printf("GLFW init failed!");
		exit(-1);
	}
	

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 2.1
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	if (APPLE){
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	}
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* monitorInfo = glfwGetVideoMode(monitor);
	window_width = monitorInfo->width; window_height = monitorInfo->height; //Grab the width of the screen.
	window = glfwCreateWindow(window_width, window_height, "Conway", NULL, NULL);
	texture_size = (powerOfTwoAbove(window_width)>powerOfTwoAbove(window_height))?powerOfTwoAbove(window_width):powerOfTwoAbove(window_height); //Make the texture big enough to hold the game
	game_width = window_width; game_height = window_height;

	//glfwWindowHint(GLFW_REFRESH_RATE,2000);



	if( window == NULL ){
	    fprintf(stderr, "Failed to open GLFW window.\n" );
	    glfwTerminate();
	    exit(-1);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        printf("Failed to initialize OpenGL context\n");
        exit(-1);
    }
    unsigned const char* device_string = glGetString(GL_RENDERER);
	printf("OpenGL device: %s\n",device_string);
	glDisable(GL_DEPTH_TEST); glDisable(GL_MULTISAMPLE);




	//Take a moment to initialize the OpenCL context as well. This must be done before any memory is allocated
	ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	printf("Num platforms: %i\n", ret_num_platforms);
	ret = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);
	printf("Num devices: %i\n", ret_num_devices);

	size_t value_size;
	clGetDeviceInfo(device_id, CL_DEVICE_NAME, 0, NULL, &value_size);
	char *value = malloc(value_size);
	clGetDeviceInfo(device_id, CL_DEVICE_NAME, value_size, value, NULL);
	printf("OpenCL Device: %s\n", value);
	free(value);
	#ifdef __linux__
	    cl_context_properties cps[] = {
			CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetGLXContext(window),
			CL_GLX_DISPLAY_KHR, (cl_context_properties) glfwGetX11Display(),
	        CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id,
	        0
	    };
	#elif _WIN32
	    cl_context_properties cps[] = {
            CL_GL_CONTEXT_KHR, (cl_context_properties)glfwGetWGLContext(window),
            CL_WGL_HDC_KHR, (cl_context_properties)GetDC(glfwGetWin32Window(window)),
            CL_CONTEXT_PLATFORM, (cl_context_properties)platform_id,
            0
		};
	#endif
	context = clCreateContextFromType(cps, CL_DEVICE_TYPE_GPU, NULL, NULL, &ret);
	printf("Context return: %i\n", ret);


 //    glewExperimental = GL_TRUE;
	// glewInit();

	float board_vertices[32] = {
	//  Position      Color             		 Texcoords
	    -1.0f,  1.0f, 0.0f, 255.f, 255.f, 255.f, 0.0f, 0.0f, // Top-left
	     1.0f,  1.0f, 0.0f, 255.f, 255.f, 255.f, ((float)game_width)/texture_size, 0.0f, // Top-right
	     1.0f, -1.0f, 0.0f, 255.f, 255.f, 255.f, ((float)game_width)/texture_size, ((float)game_height)/texture_size, // Bottom-right
	    -1.0f, -1.0f, 0.0f, 255.f, 255.f, 255.f, 0.0f, ((float)game_height)/texture_size  // Bottom-left
	};
	GLuint board_elements[6] = { //The component triangles of the board
        0, 1, 2,
        2, 3, 0
    };
    
    glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(board_vertices), board_vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(board_elements), board_elements, GL_STATIC_DRAW);


	int status;

    FILE *fp; fp = fopen("vertexshader","r");
	if (fp==NULL){
		printf("Open failed.\n");
		exit(-1);
	}
	char *vertex_source = (char*)malloc(MAX_SOURCE_SIZE);
	const size_t vertex_source_length = fread(vertex_source, 1, MAX_SOURCE_SIZE, fp);
	vertex_source=realloc(vertex_source,(vertex_source_length+1)*sizeof(char)); vertex_source[vertex_source_length]=0;
	fclose(fp);

	fp = fopen("fragmentshader","r");
	if (fp==NULL){
		printf("Open failed.\n");
		exit(-1);
	}
	char *fragment_source = (char*)malloc(MAX_SOURCE_SIZE);
	const size_t fragment_source_length = fread(fragment_source, 1, MAX_SOURCE_SIZE, fp);
	fragment_source=realloc(fragment_source,(fragment_source_length+1)*sizeof(char)); fragment_source[fragment_source_length]=0;
	//printf("Code: %s\n", fragment_source);
	fclose(fp);

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, (const char**)&vertex_source, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    if (status!=GL_TRUE){
    	printf("Vertex shader compilation failed.\n");
    	glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &status);
		if ( status > 0 ){
			char VertexShaderErrorMessage[status+1];
			glGetShaderInfoLog(vertexShader, status, NULL, VertexShaderErrorMessage);
			printf("%s\n", VertexShaderErrorMessage);
		}
    	exit(0);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, (const char**)&fragment_source, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
    if (status!=GL_TRUE){
    	printf("Fragment shader compilation failed.\n");
    	glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &status);
		if ( status > 0 ){
			char FragmentShaderErrorMessage[status+1];
			glGetShaderInfoLog(fragmentShader, status, NULL, FragmentShaderErrorMessage);
			printf("%s\n", FragmentShaderErrorMessage);
		}
    	exit(0);
    }

    // Link the vertex and fragment shader into a shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
    if (status!=GL_TRUE){
    	printf("Program link failed.\n");
    	glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &status);
		if ( status > 0 ){
			char ProgramErrorMessage[status+1];
			glGetProgramInfoLog(shaderProgram, status, NULL, ProgramErrorMessage);
			printf("%s\n", ProgramErrorMessage);
		}
    }


    

    // Specify the layout of the vertex data
    posAttrib = glGetAttribLocation(shaderProgram, "pos");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), 0);

    colAttrib = glGetAttribLocation(shaderProgram, "inColor");
    glEnableVertexAttribArray(colAttrib);
    glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));

    texAttrib = glGetAttribLocation(shaderProgram, "texCoord");
    glEnableVertexAttribArray(texAttrib);
    glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));


    glGenTextures(1,&board_texture); //allocate the memory for texture
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(BOARD_TEXTURE_TYPE, board_texture);
	glUniform1i(glGetUniformLocation(shaderProgram, "board_sampler"), 0);//This is important for the fragmentShader

	glTexParameteri(BOARD_TEXTURE_TYPE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(BOARD_TEXTURE_TYPE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(BOARD_TEXTURE_TYPE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(BOARD_TEXTURE_TYPE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	char *texInit=malloc(texture_size*texture_size*4); memset(texInit,0,texture_size*texture_size*4);
	glTexImage2D(BOARD_TEXTURE_TYPE, 0, GL_RGBA8, texture_size, texture_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, texInit);
	free(texInit);
	glFinish();
}

void clInit(){
	//Initialize the CL context to be the same as the GL context
	
	

	command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &ret);
	printf("Command queue return: %i\n", ret);

	FILE *fp; fp = fopen("cl_kernel.cl","r");
	if (fp==NULL){
		printf("Open failed.\n");
		exit(-1);
	}
	char *code_str = (char*)malloc(MAX_SOURCE_SIZE);
	const size_t code_length = fread(code_str, 1, MAX_SOURCE_SIZE, fp);
	code_str=realloc(code_str,(code_length+1)*sizeof(char)); code_str[code_length]=0;
	// printf("Code length: %i\n",(int)code_length);
	// printf("Code: %s\n", code_str);
	fclose(fp);

	program = clCreateProgramWithSource(context, 1, (const char **)&code_str, &code_length, &ret);
	printf("Program create return: %i\n", ret);
	ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	printf("Program build return: %i\n", ret);
	free(code_str);

	if(ret != CL_SUCCESS){
	    size_t len = 0;
	    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
	    char *buffer = calloc(len+1, sizeof(char));
	    memset(buffer,0,len+1);
	    ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, len, buffer, NULL);
	    printf("Build info length: %li. Build info: %s\n", len, &buffer[1]);
	    // printf("%i %i %i %i %i %i %i %i\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
	    free(buffer);
	    exit(1);
	}



	calculateAdjacencies = clCreateKernel(program, "compute_adjacencies", &ret);
	zeroAdjacencies = clCreateKernel(program, "zero_adjacencies", &ret);
	updateState = clCreateKernel(program, "compute_state", &ret);
	writeStateToImage = clCreateKernel(program, "write_state_to_image", &ret);
	//printf("Write state kernel return: %i\n",ret);
	initializeState = clCreateKernel(program, "initialize_state", &ret);
	//printf ("Initialize state kernel return %i\n", ret);
	flipSquare = clCreateKernel(program, "flip_square", &ret);

	CL_board_texture=clCreateFromGLTexture(context, CL_MEM_READ_WRITE, BOARD_TEXTURE_TYPE, 0, board_texture, &ret);  //Should be able to change this to WRITE_ONLY later -- just READ_WRITE for debug
	printf("Texture grab return: %i\n", ret);
}	

int main(){
	glInit();
	clInit();

	//printf("Hi!\n");
	// int view_x=0; //Coordinates of the top left corner on the game board
	// int view_y=0;
	int current_screen_width = game_width; int current_screen_height = game_height;
	// int view_center_x; int view_center_y;
	// int zoom = 1;

	

	size_t game_pixels = game_width * game_height;
	game_state = clCreateBuffer(context, CL_MEM_READ_WRITE, game_pixels, NULL, &ret);
	printf("Game state buffer creation: %i\n", ret);
	adjacencies = clCreateBuffer(context, CL_MEM_READ_WRITE, game_pixels, NULL, &ret);
	printf("Adjacencies buffer creation: %i\n", ret);

	int border_width = BORDER_WIDTH;
	printf("\n");
	//Set up arguments for initializeState kernel
	ret = clSetKernelArg(initializeState, 0, sizeof(game_state), &game_state);// Maybe (void *)&game_state, and likewise for other memory objects
	printf("Kernel setup 0 return: %i\n", ret);
	ret = clSetKernelArg(initializeState, 1, sizeof(border_width), &border_width);
	printf("Kernel setup 1 return: %i\n", ret);
	ret = clSetKernelArg(initializeState, 2, sizeof(game_width), &game_width);
	printf("Kernel setup 2 return: %i\n", ret);
	ret = clSetKernelArg(initializeState, 3, sizeof(game_height), &game_height);
	printf("Kernel setup 3 return: %i\n", ret);

	//Set up arguments for writeStateToImage kernel
	ret = clSetKernelArg(writeStateToImage, 0, sizeof(game_state), &game_state);
	printf("Kernel setup 0 return: %i\n", ret);
	ret = clSetKernelArg(writeStateToImage, 1, sizeof(CL_board_texture), &CL_board_texture);
	printf("Kernel setup 1 return: %i\n", ret);
	ret = clSetKernelArg(writeStateToImage, 2, sizeof(game_width), &game_width);
	printf("Kernel setup 2 return: %i\n", ret);

	//Set up arguments for computeAdjacencies kernel
	ret = clSetKernelArg(calculateAdjacencies, 0, sizeof(game_state), &game_state);
	printf("Kernel setup 0 return: %i\n", ret);
	ret = clSetKernelArg(calculateAdjacencies, 1, sizeof(adjacencies), &adjacencies);
	printf("Kernel setup 1 return: %i\n", ret);
	ret = clSetKernelArg(calculateAdjacencies, 2, sizeof(border_width), &border_width);
	printf("Kernel setup 2 return: %i\n", ret);
	ret = clSetKernelArg(calculateAdjacencies, 3, sizeof(game_width), &game_width);
	printf("Kernel setup 3 return: %i\n", ret);

	//Set up arguments for computeState kernel
	ret = clSetKernelArg(updateState, 0, sizeof(adjacencies), &adjacencies);
	printf("Kernel setup 0 return: %i\n", ret);
	ret = clSetKernelArg(updateState, 1, sizeof(game_state), &game_state);
	printf("Kernel setup 1 return: %i\n", ret);

	//Set up arguments for flipSquare kernel
	ret = clSetKernelArg(flipSquare, 0, sizeof(game_state), &game_state);
	printf("Kernel setup 0 return: %i\n", ret);
	ret = clSetKernelArg(flipSquare, 3, sizeof(game_width), &game_width);
	printf("Kernel setup 3 return: %i\n", ret);

	//Set up arguments for zeroAdjacencies kernel
	ret = clSetKernelArg(zeroAdjacencies, 0, sizeof(adjacencies), &adjacencies);
	printf("Kernel setup 0 return: %i\n", ret);

	size_t work_group_size = 256; //Batch size


	glFinish();

	ret = clEnqueueAcquireGLObjects(command_queue, 1, &CL_board_texture, 0, NULL, NULL);
	clFinish(command_queue);
	printf("Acquire return: %i\n",ret);
	ret = clEnqueueNDRangeKernel(command_queue, initializeState, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
	printf("Initialize state return: %i\n",ret);
	ret = clEnqueueNDRangeKernel(command_queue, writeStateToImage, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
	printf("Write state return: %i\n",ret);
	ret = clEnqueueReleaseGLObjects(command_queue, 1, &CL_board_texture, 0, NULL, NULL);
	clFinish(command_queue);

	glfwSwapBuffers(window);


	bool paused=false;
	double temp_cursor_x; double temp_cursor_y;
	int cursor_x; int cursor_y;
	int square_x; int square_y;
	float corner_x; float corner_y;
	int prev_square_x; int prev_square_y;


	clock_t t=clock();
	clock_t frame_clock=clock();
	clock_t refresh_clock=clock();
	clock_t screenshift_clock=clock();
	
	float refresh_rate = 144.0f; //Sets frame rate
	float game_frame_rate = 144.0f; //Sets game frame rate
	bool space_pressed = false;
	GLfloat camera_pos[2]={0.0,0.0};
	int zoom=1;
	GLfloat old_camera_pos[2]={-1.0,-1.0};
	int old_zoom=0;

	const size_t one[1]={1};//For flipping single pixels

	glfwSetScrollCallback(window, scroll_callback); //This should maintain rawScroll as up-to-date

	GLint camera_pos_shader_loc = glGetUniformLocation(shaderProgram, "cameraPos");
	GLint zoom_shader_loc = glGetUniformLocation(shaderProgram, "zoom");

	glUniform1i(glGetUniformLocation(shaderProgram, "game_width"),game_width);
	glUniform1i(glGetUniformLocation(shaderProgram, "game_height"),game_height);


	glEnable(GL_DEBUG_OUTPUT);
	bool speed_adjust_pressed=false;

	while (glfwWindowShouldClose(window) == false){
		//printf("Clocks per second: %li\n", CLOCKS_PER_SEC);
		printf("Frame time: %li, FPS: %f\n", clock()-t, CLOCKS_PER_SEC/((float)clock()-t));
		t=clock();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
			glfwSetWindowShouldClose(window, true);
		}
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !space_pressed){
			space_pressed=true;
			paused=!paused;
			printf("Paused: %i\n", paused);
		}
		if (glfwGetKey(window, GLFW_KEY_SPACE) != GLFW_PRESS){
			space_pressed=false;
		}

		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS){
			camera_pos[0]-=(clock()-screenshift_clock)*500.0f/CLOCKS_PER_SEC/(float)zoom;
		}
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS){
			camera_pos[0]+=(clock()-screenshift_clock)*500.0f/CLOCKS_PER_SEC/(float)zoom;
		}
		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS){
			camera_pos[1]+=(clock()-screenshift_clock)*500.0f/CLOCKS_PER_SEC/(float)zoom;
		}
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS){
			camera_pos[1]-=(clock()-screenshift_clock)*500.0f/CLOCKS_PER_SEC/(float)zoom;
		}
		screenshift_clock=clock();

		if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS && !speed_adjust_pressed){
			game_frame_rate/=1.5f;
			speed_adjust_pressed=true;
		}
		if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS && !speed_adjust_pressed){
			game_frame_rate*=1.5f;
			speed_adjust_pressed=true;
		}
		if (glfwGetKey(window, GLFW_KEY_EQUAL) != GLFW_PRESS && glfwGetKey(window, GLFW_KEY_MINUS) != GLFW_PRESS){
			speed_adjust_pressed=false;
		}
		printf("Game frame rate: %f\n", game_frame_rate);
		if (game_frame_rate<1){
			game_frame_rate=1;
		}
		if (game_frame_rate>1000){
			game_frame_rate=1000;
		}


		if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS){
			ret = clEnqueueNDRangeKernel(command_queue, initializeState, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
		}
		glfwGetWindowSize(window, &current_screen_width, &current_screen_height);
		glfwGetCursorPos(window, &temp_cursor_x, &temp_cursor_y);
		cursor_x=temp_cursor_x; cursor_y=temp_cursor_y;


		if ((glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS)){ //Flip cursor square
			corner_x=camera_pos[0]/2-((float)game_width)/2/zoom+((float)game_width)/2; corner_y=-camera_pos[1]/2-((float)current_screen_height*2-game_height)/2/zoom+((float)game_height)/2;
			square_x=corner_x+((float)cursor_x)/zoom; square_y=corner_y+((float)cursor_y)/zoom;
			if (prev_square_x != square_x || prev_square_y != square_y){
				prev_square_x=square_x; prev_square_y=square_y;
				if (square_x>=0 && square_x<game_width && square_y>=0 && square_y<game_height){
					ret = clSetKernelArg(flipSquare, 1, sizeof(square_x), &square_x);//May not need to do this every time, but I think I do.
					ret = clSetKernelArg(flipSquare, 2, sizeof(square_y), &square_y);
					ret = clEnqueueNDRangeKernel(command_queue, flipSquare, 1, NULL, one, one, 0, NULL, NULL);
				}
				//printf("Flip square enqueue: %i\n", ret);
			}
		}
		if ((glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS)){ //Randomly flip squares around the cursor
			corner_x=camera_pos[0]/2-((float)game_width)/2/zoom+((float)game_width)/2; corner_y=-camera_pos[1]/2-((float)current_screen_height*2-game_height)/2/zoom+((float)game_height)/2;
			square_x=corner_x+((float)cursor_x)/zoom; square_y=corner_y+((float)cursor_y)/zoom;
			int off_square_x; int off_square_y;
			for (int i=-5;i<=5;i++){
				for (int j=-5;j<=5;j++){
					off_square_x=square_x+i; off_square_y=square_y+j;
					if (off_square_x>=0 && off_square_x<game_width && off_square_y>=0 && off_square_y<game_height && rand()%2==1){
						ret = clSetKernelArg(flipSquare, 1, sizeof(off_square_x), &off_square_x);//May not need to do this every time, but I think I do.
						ret = clSetKernelArg(flipSquare, 2, sizeof(off_square_y), &off_square_y);
						ret = clEnqueueNDRangeKernel(command_queue, flipSquare, 1, NULL, one, one, 0, NULL, NULL);
					}
				}
			}

		}
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)!=GLFW_PRESS){
			prev_square_x = -1; prev_square_y = -1;
		}
		if (rawScroll>0){
			rawScroll=0;
			if (zoom<64){
				camera_pos[0]+=2*(cursor_x-game_width/2)/zoom;
				camera_pos[1]-=2*(cursor_y-((float)current_screen_height*2-game_height)/2)/zoom;
				zoom*=2;
				camera_pos[0]-=2*(cursor_x-game_width/2)/zoom;
				camera_pos[1]+=2*(cursor_y-((float)current_screen_height*2-game_height)/2)/zoom;

				//camera_pos[0]+=(cursor_x-game_width/2)/zoom; camera_pos[1]-=(cursor_y-game_width/2)/zoom;
			}
		}
		if (rawScroll<0){
			rawScroll=0;
			if (zoom>1){
				camera_pos[0]+=2*(cursor_x-game_width/2)/zoom;
				camera_pos[1]-=2*(cursor_y-((float)current_screen_height*2-game_height)/2)/zoom;
				zoom/=2;
				camera_pos[0]-=2*(cursor_x-game_width/2)/zoom;
				camera_pos[1]+=2*(cursor_y-((float)current_screen_height*2-game_height)/2)/zoom;
			}
		}
		//printf("Left bound: %f, Right bound: %f\n",-(game_width-((float)current_screen_width)/zoom),game_width-((float)current_screen_width)/zoom);
		//printf("Camera_x: %f, Screen width: %i, Min: %f, Max: %f\n",camera_pos[0],current_screen_width,-(game_width-((float)game_width)/zoom),-(game_width-((float)game_width)/zoom)+(game_width-((float)current_screen_width)/zoom));
		camera_pos[0]=clip(camera_pos[0],-(game_width-((float)game_width)/zoom),-(game_width-((float)game_width)/zoom)+(game_width-((float)current_screen_width)/zoom)*2);//(game_width-((float)current_screen_width)/zoom));
		camera_pos[1]=clip(camera_pos[1],-(game_height-((float)game_height)/zoom),-(game_height-((float)game_height)/zoom)+(game_height-((float)current_screen_height)/zoom)*2);




		glFinish();
		//printf("Time pre-uniform: %li\n", clock()-t);
		//glUseProgram(shaderProgram); //Probably not needed
		if (zoom!=old_zoom){
			glUniform1i(zoom_shader_loc,zoom);
			old_zoom=zoom;
		}
		if (camera_pos[0]!=old_camera_pos[0] || camera_pos[1]!=old_camera_pos[1]){
			glUniform2fv(camera_pos_shader_loc,1,camera_pos);
			old_camera_pos[0]=camera_pos[0]; old_camera_pos[1]=camera_pos[1];
		}
		glFinish();
		//printf("Time pre-acquire: %li\n", clock()-t);
		//Acquire the board image. Then update the board state if needed, and write it to the image
		ret = clEnqueueAcquireGLObjects(command_queue, 1, &CL_board_texture, 0, NULL, NULL);
		//printf("%li\n",(clock()-t)/CLOCKS_PER_SEC);
		if ((!paused)){
			//printf("Time pre-adjacencies: %li\n", clock()-t);
			ret = clEnqueueNDRangeKernel(command_queue, zeroAdjacencies, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
			int offset;
			for (int i=-1;i<=1;i++){
				for (int j=-1;j<=1;j++){
					if (i!=0 || j!=0){
						offset = i*game_width+j;
						ret = clSetKernelArg(calculateAdjacencies, 4, sizeof(offset), &offset);
						ret = clEnqueueNDRangeKernel(command_queue, calculateAdjacencies, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
					}
				}
			}
			ret = clFinish(command_queue);
			//printf("Time post-adjacencies: %li\n", clock()-t);
			ret = clEnqueueNDRangeKernel(command_queue, updateState, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
		}
		ret = clEnqueueNDRangeKernel(command_queue, writeStateToImage, 1, NULL, &game_pixels, &work_group_size, 0, NULL, NULL);
		ret = clEnqueueReleaseGLObjects(command_queue, 1, &CL_board_texture, 0, NULL, NULL);
		ret = clFinish(command_queue);
		//printf("Time post-release: %li\n", clock()-t);


		// view_center_x = view_x + current_screen_width; view_center_x = view_y + current_screen_height;

	
		if (((float)clock()-refresh_clock)/CLOCKS_PER_SEC>1.0f/refresh_rate){ // Change for different displays
			glClearColor(0, 0, 0, 255);
			glClear(GL_COLOR_BUFFER_BIT);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glFinish();
			//printf("Time post-draw: %li\n", clock()-t);
			glfwSwapBuffers(window);
			refresh_clock=clock();
		}
		if (!paused){
			while (((float)clock()-frame_clock)/CLOCKS_PER_SEC<1.0f/game_frame_rate){};
		}

		frame_clock=clock();
		glFinish();
		//printf("Time post-swap: %li\n", clock()-t);
		glfwPollEvents();
		glFinish();
	}
}