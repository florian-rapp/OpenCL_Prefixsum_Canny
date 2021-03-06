/**********************************************************************
Copyright ©2013 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

•	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
•	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/

// For clarity,error checking has been omitted.

#include <CL/cl.hpp>
#include <CL/cl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>

using namespace std;

#include "OpenCLMgr.h"

constexpr cl_int zeroInt = 0;	// Used to fill up buffer

// eigener code
int praefixsumme_own(cl_mem input_buffer_a, cl_mem output_buffer_b_e, int size, OpenCLMgr& mgr)
{
	
	cl_int status;
	int workgroup_size = 256;
	cl_int new_size = (size + (256 - (size % 256)));

	// Create new buffer for blocksums
	cl_mem blocksum_buffer_c = clCreateBuffer(mgr.context, CL_MEM_READ_WRITE, (new_size / workgroup_size) * sizeof(cl_int), NULL, NULL);

	// Set kernel arguments.
	status = clSetKernelArg(mgr.praefixsumme256_kernel, 0, sizeof(cl_mem), (void*)&input_buffer_a);
	CHECK_SUCCESS("Error: setting kernel1 argument 1!")
	status = clSetKernelArg(mgr.praefixsumme256_kernel, 1, sizeof(cl_mem), (void*)&output_buffer_b_e);
	CHECK_SUCCESS("Error: setting kernel1 argument 2!")
	status = clSetKernelArg(mgr.praefixsumme256_kernel, 2, sizeof(cl_mem), (void*)&blocksum_buffer_c);
	CHECK_SUCCESS("Error: setting kernel1 argument 3!")

	// Run the kernel.
	size_t global_work_size[1] = { new_size };
	size_t local_work_size[1] = { workgroup_size };
	status = clEnqueueNDRangeKernel(mgr.commandQueue, mgr.praefixsumme256_kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, NULL);
	CHECK_SUCCESS(status)



	// recursive call to calculate prefix sums in blocksums_buffer_c, if size > 256
	if (size > 256) {
		// Create Buffer D to write the praefix sums of blocksums
		cl_mem helper_buffer_d = clCreateBuffer(mgr.context, CL_MEM_READ_WRITE, new_size / 256 * sizeof(cl_int), NULL, NULL);

		//recursive call to calculate praefix of blocksums array
		praefixsumme_own(blocksum_buffer_c, helper_buffer_d, new_size / 256 * sizeof(cl_int), mgr);

		// === use second kernel to add B and D buffer ===
		// set kernel arguments for second kernel
		status = clSetKernelArg(mgr.final_prefixsum, 0, sizeof(cl_mem), (void*)&output_buffer_b_e);
		CHECK_SUCCESS("Error: setting kernel2 argument 1!")
		status = clSetKernelArg(mgr.final_prefixsum, 1, sizeof(cl_mem), (void*)&helper_buffer_d);
		CHECK_SUCCESS("Error: setting kernel2 argument 2!")

		size_t global_work_size[1] = { (size + ( 256 - (size % 256))) };
		size_t local_work_size[1] = { workgroup_size };

		// run the final kernel to add blockwise prefix sums to blocksums prefix array
		status = clEnqueueNDRangeKernel(mgr.commandQueue, mgr.final_prefixsum, 1, NULL, global_work_size, local_work_size, 0, NULL, NULL);
		CHECK_SUCCESS("Error: enqueuing the final kernel!")

		// release buffers
		status = clReleaseMemObject(blocksum_buffer_c);
		CHECK_SUCCESS("Error: releasing buffer!")
		status = clReleaseMemObject(helper_buffer_d);
		CHECK_SUCCESS("Error: releasing buffer!")
	}

	return SUCCESS;
}

int main(int argc, char* argv[])
{
	OpenCLMgr mgr;

	// Initial input for the host and create memory objects for the kernel
	int size = 20000;
	cl_int* input = new cl_int[size];
	for (int i = 0; i < size; i++) input[i] = 1;

	cl_int status;


	// calculate size of inputbuffer; must be multiple of 256 (local work size)
	int new_size = (size + (256 - (size % 256)));


	// creating inputBuffer
	cl_mem input_buffer_a = clCreateBuffer(mgr.context, CL_MEM_READ_ONLY, new_size * sizeof(cl_int), NULL, NULL);
	// filling inputBuffer with values from input array
	status = clEnqueueWriteBuffer(mgr.commandQueue, input_buffer_a, CL_TRUE, 0, size * sizeof(cl_int), input, 0, NULL, NULL);
	// fill empty elements in input buffer with zero
	status = clEnqueueFillBuffer(mgr.commandQueue, input_buffer_a, &zeroInt, sizeof(cl_int), size * sizeof(cl_int), (new_size - size) * sizeof(cl_int), 0, NULL, NULL);
	// create outputBuffer 
	cl_mem output_buffer_b_e = clCreateBuffer(mgr.context, CL_MEM_READ_WRITE, new_size * sizeof(cl_int), NULL, NULL);
	
	// call function to calculate prefix sums
	praefixsumme_own(input_buffer_a, output_buffer_b_e, size, mgr);

	// create array to read from buffer to cpu
	cl_int* final = new cl_int[new_size];

	// get resulting array
	status = clEnqueueReadBuffer(mgr.commandQueue, output_buffer_b_e, CL_TRUE, 0, new_size * sizeof(cl_int), final, 0, NULL, NULL);
	CHECK_SUCCESS("Error: reading buffer!")

	// output array values to console (uncomment if you want to see every value)
	//std::cout << "in main: finalarray" << "\n";
	//for (int i = 0; i < new_size; i++) {
	//	std::cout << "stelle " << i << ": " << final[i] << "\n";
	//}

	// debug code
	//std::cout << "stelle " << 5 << ": " << final[5] << "\n";
	//std::cout << "stelle " << 262145 << ": " << final[262145] << "\n";
	//std::cout << "stelle " << 262200 << ": " << final[262200] << "\n";


	// release buffers
	status = clReleaseMemObject(input_buffer_a);
	CHECK_SUCCESS("Error: releasing buffer!")
	status = clReleaseMemObject(output_buffer_b_e);
	CHECK_SUCCESS("Error: releasing buffer!")



	delete[] input;
	delete[] final;

	std::cout << "Passed!\n";
	return SUCCESS;
}
