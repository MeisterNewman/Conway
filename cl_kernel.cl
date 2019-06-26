__kernel void compute_adjacencies(__global const char *state, __global char *adj, int border_width, int width, int offset){
	size_t dim = get_global_id(0);
	size_t x = dim % width; size_t y = dim / width; //Get 
	int max = get_global_size(0);
	int height= max/width;
	if (x<border_width || y<border_width || x>=width-border_width || y>= height-border_width){
		adj[dim]=0;
	}
	else{
		adj[dim]+=state[dim+offset]&1;
	}
}
__kernel void zero_adjacencies(__global char *adj){
	size_t dim = get_global_id(0);
	adj[dim]=0;
}

__kernel void compute_state(__global const char *adj, __global char *state){
	size_t i = get_global_id(0);
	if (state[i]==1){
		state[i]=(adj[i]==2)|(adj[i]==3);
	}
	if (state[i]==0){
		state[i]=(adj[i]==3);
	}
}

__kernel void write_state_to_image(__global const char *state, __write_only image2d_t output, int width){
	size_t index = get_global_id(0);
	//uint4 color;
	float4 color;
	if (state[index]==0){
		//color = (uint4)(255,0,0,255);
		color=(float4)(0.0,0.0,0.0,1.0);
	}
	else if (state[index]==1){
		//color = (uint4)(255,255,255,255);
		color=(float4)(1.0,1.0,1.0,1.0);
	}
	else{
		//color = (uint4)(0,0,255,255);
		color=(float4)(0.0,0.0,1.0,1.0);
	}
	int2 coord = (int2)(index % width, index / width);
	write_imagef(output, coord, color);
}

__kernel void initialize_state(__global char *state, int border_width, int width, int height){
	size_t index = get_global_id(0);
	if (index/width < border_width || index/width>=height-border_width || index%width < border_width || index%width>=width-border_width){
		state[index]=2;
	}
	else{
		state[index]=0;
	}
}


__kernel void flip_square(__global char *state, int square_x, int square_y, int width){
	if (state[width*square_y+square_x]!=2){
		state[width*square_y+square_x]=1-state[width*square_y+square_x];
	}
}