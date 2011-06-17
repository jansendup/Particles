#pragma once
#include <CL/cl.h>
#include <Windows.h>
#include <GL/gl.h>

typedef struct vector4_type
{
	float x,y,z,w;

	vector4_type(){};
	vector4_type(float _x, float _y, float _z, float _w = 1.0f)
	{
		x = _x;
		y = _y;
		z = _z;
		w = _w;
	}
	
} Vector4;

class OCL
{
public:
	OCL(void);
	~OCL(void);

	bool InitializeContext();
	bool LoadProgram(const char* file);
	bool LoadData(Vector4* pos, Vector4* vel, Vector4* col, int size);
	bool Run();
private:
	bool BuildExecutable();

	cl_platform_id platformId;
	cl_device_id deviceId;
	cl_context context;
	cl_command_queue commandQueue;
	cl_program program;
	cl_kernel kernel;

	// Static buffers
	cl_mem cl_static_pos, cl_static_vel;

	// Dynamic buffers
	cl_mem cl_velocities;
	GLuint vbo_pos, vbo_color;
	cl_mem cl_glReferances[2];
	bool initialized;
};

