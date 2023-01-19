#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/timeb.h>
#include <cuda_runtime.h>
#include "util.h"
#include "device_launch_parameters.h"

__device__ void set_pixel_device(unsigned char* image, int width, int x, int y, unsigned char* c) {
    image[4 * width * y + 4 * x + 0] = c[0];
    image[4 * width * y + 4 * x + 1] = c[1];
    image[4 * width * y + 4 * x + 2] = c[2];
    image[4 * width * y + 4 * x + 3] = 255;
}


//VERY BAD DON'T DO THIS
__device__ unsigned char color2byte_device(float v) {
    float c = v * 255;
    if (c < 0) {
        c = 0;
    }
    if (c > 255) {
        c = 255;
    }
    return (unsigned char)c;
}

__device__ void hsv2rgb_device(float h, float s, float v, unsigned char* rgb)
{
    int i;
    float f, p, q, t, r, g, b;

    if (s == 0) {
        r = g = b = v;
        return;
    }

    h /= 60;
    i = (int)floor(h);
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch (i) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    default:
        r = v;
        g = p;
        b = q;
        break;
    }

    rgb[0] = color2byte_device(r);
    rgb[1] = color2byte_device(g);
    rgb[2] = color2byte_device(b);
}


__device__ void init_colormap_device(int len, unsigned char* map) {
    int i;
    for (i = 0; i < len; i++) {
        hsv2rgb_device(i / 4.0f, 1.0f, i / (i + 8.0f), &map[i * 3]);
    }
    map[3 * len + 0] = 0;
    map[3 * len + 1] = 0;
    map[3 * len + 2] = 0;

}


/* This should be conveted into a GPU kernel */
__global__ void generate_image(unsigned char* image, unsigned char* colormap, int width, int height, int max) {
    int row, col, index, iteration;
    double c_re, c_im, x, y, x_new;

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;
    int id = idy * width + idx;

    if (idx >= width || idy >= height) {
        return;
    }
    c_re = (idx - width / 2.0) * 4.0 / width;
    c_im = (idy - height / 2.0) * 4.0 / width;
    x = 0, y = 0;
    iteration = 0;
    while (x * x + y * y <= 4 && iteration < max) {
        x_new = x * x - y * y + c_re;
        y = 2 * x * y + c_im;
        x = x_new;
        iteration++;
    }
    if (iteration > max) {
        iteration = max;
    }
    set_pixel_device(image, width, idx, idy, &colormap[iteration * 3]);
}


//logicly bad why would you want to do this but it is what is is
//__global__ void generate_image_shared(unsigned char* image, int width, int height, int max) {
//    int row, col, index, iteration;
//    double c_re, c_im, x, y, x_new;
//
//    
//    ////declare it as shared
//    __shared__ unsigned char colormap[(MAX_ITERATION + 1) * 3];
//
//     // ? ? ? ? NOT OK
//    ////init on device side
//    init_colormap_device(max, colormap);
//
//    //make sure that the threads are synched after colormap init and reached the barrier
//    __syncthreads();
//
//    int idx = blockIdx.x * blockDim.x + threadIdx.x;
//    int idy = blockIdx.y * blockDim.y + threadIdx.y;
//    int id = idy * width + idx;
//
//    if (idx >= width || idy >= height) {
//        return;
//    }
//
//
//    c_re = (idx - width / 2.0) * 4.0 / width;
//    c_im = (idy - height / 2.0) * 4.0 / width;
//    x = 0, y = 0;
//    iteration = 0;
//    while (x * x + y * y <= 4 && iteration < max) {
//        x_new = x * x - y * y + c_re;
//        y = 2 * x * y + c_im;
//        x = x_new;
//        iteration++;
//    }
//    if (iteration > max) {
//        iteration = max;
//    }
//    set_pixel_device(image, width, idx, idy, &colormap[iteration * 3]);
//}

//declare colormap on constant mem
__constant__ unsigned char colormap_on_constant[(MAX_ITERATION + 1) * 3];

__global__ void generate_image_constant(unsigned char* image, int width, int height, int max) {
    int row, col, index, iteration;
    double c_re, c_im, x, y, x_new;

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int idy = blockIdx.y * blockDim.y + threadIdx.y;
    int id = idy * width + idx;

    if (idx >= width || idy >= height) {
        return;
    }


    c_re = (idx - width / 2.0) * 4.0 / width;
    c_im = (idy - height / 2.0) * 4.0 / width;
    x = 0, y = 0;
    iteration = 0;
    while (x * x + y * y <= 4 && iteration < max) {
        x_new = x * x - y * y + c_re;
        y = 2 * x * y + c_im;
        x = x_new;
        iteration++;
    }
    if (iteration > max) {
        iteration = max;
    }
    set_pixel_device(image, width, idx, idy, &colormap_on_constant[iteration * 3]);
}

int main(int argc, char** argv) {
    double times[REPEAT];
    struct timeb start, end;
    int i, r;
    char path[255];

    unsigned char* colormap = (unsigned char*)malloc((MAX_ITERATION + 1) * 3);
    unsigned char* image = (unsigned char*)malloc(WIDTH * HEIGHT * 4);

    unsigned char* device_colormap;
    unsigned char* device_image;

    cudaMalloc(&device_colormap, (MAX_ITERATION + 1) * 3);
    cudaMalloc(&device_image, WIDTH * HEIGHT * 4);

    init_colormap(MAX_ITERATION, colormap);

    //only for global memory kernel
    /*cudaMemcpy(device_colormap, colormap, (MAX_ITERATION + 1) * 3, cudaMemcpyHostToDevice);*/

    //copy it to the constant memory
    cudaMemcpyToSymbol(colormap_on_constant, colormap, (MAX_ITERATION + 1) * 3);

    int BLOCK_SIZE = 16;

    dim3 block(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid((WIDTH + BLOCK_SIZE - 1) / BLOCK_SIZE, (HEIGHT + BLOCK_SIZE - 1) / BLOCK_SIZE);

    for (r = 0; r < REPEAT; r++) {
        cudaMemset(device_image, 0, WIDTH * HEIGHT * 4);

        ftime(&start);

        /* BEGIN: GPU implementation */

        ////kernel with normal colormap transfered to device
        //generate_image << <grid, block >> > (device_image, device_colormap, WIDTH, HEIGHT, MAX_ITERATION);

        // kernel with color map held in shared memory
        /*  generate_image_shared << <grid, block >> > (device_image, WIDTH, HEIGHT, MAX_ITERATION);
        cudaDeviceSynchronize();*/

        generate_image_constant << <grid, block >> > (device_image, WIDTH, HEIGHT, MAX_ITERATION);


        /* END: GPU implementation */

        ftime(&end);
        times[r] = end.time - start.time + ((double)end.millitm - (double)start.millitm) / 1000.0;

        cudaMemcpy(image, device_image, WIDTH * HEIGHT * 4, cudaMemcpyDeviceToHost);
        sprintf(path, IMAGE, "gpu", r);
        save_image(path, image, WIDTH, HEIGHT);
        progress("gpu", r, times[r]);
    }
    report("gpu", times);

    cudaFree(device_colormap);
    cudaFree(device_image);
    free(image);
    free(colormap);
    return 0;

}