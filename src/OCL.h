#pragma once
#include <CL/cl.h>
#include <Windows.h>
#include "opengl.h"

typedef float Vector4[4];

class OCL
{
public:
	OCL(void);
	~OCL(void);

	bool InitializeContext();
	bool LoadProgram(const char* file);
	bool LoadData(Vector4* pos, Vector4* vel, Vector4* col, int size);
	bool CreateKernel();
	bool Run();

	// Static buffers
	cl_mem cl_static_pos, cl_static_vel;

	// Dynamic buffers
	cl_mem cl_velocities;
	GLuint vbo_pos, vbo_color;
	cl_mem cl_glReferances[2];
	bool initialized;

private:
	bool BuildExecutable();

	cl_platform_id platformId;
	cl_device_id deviceId;
	cl_context context;
	cl_command_queue commandQueue;
	cl_program program;
	cl_kernel kernel;

	int buffersSize;
	
};

