/*
 * Copyright (C) Fuzhou Rockchip Electronics Co.Ltd
 * Author: Jacob Chen <jacob-chen@iotwrt.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */

#include <asm/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <linux/stddef.h>
#include <linux/videodev2.h>

#include "bo.h"
#include "dev.h"
#include "modeset.h"

#define V4L2_CID_RGA_OP (0x00980900 | 0x1001)
#define V4L2_CID_RGA_ALHPA_REG0 (0x00980900 | 0x1002)
#define V4L2_CID_RGA_ALHPA_REG1 (0x00980900 | 0x1003)

/* operation values */
#define OP_COPY 0
#define OP_SOLID_FILL 1
#define OP_ALPHA_BLEND 2

#define NUM_BUFS 4

static char* mem2mem_dev_name = NULL;

static int hflip = 0;
static int vflip = 0;
static int rotate = 0;
static int fill_color = 0;
static int op = 0;
static int num_frames = 100;
static int display = 0;

static size_t SRC_WIDTH = 1024;
static size_t SRC_HEIGHT = 768;

static size_t SRC_CROP_X = 0;
static size_t SRC_CROP_Y = 0;
static size_t SRC_CROP_W = 0;
static size_t SRC_CROP_H = 0;

static size_t DST_WIDTH = 1024;
static size_t DST_HEIGHT = 768;

static size_t DST_CROP_X = 0;
static size_t DST_CROP_Y = 0;
static size_t DST_CROP_W = 0;
static size_t DST_CROP_H = 0;

static int src_format = V4L2_PIX_FMT_NV12;
static int dst_format = V4L2_PIX_FMT_NV12;

static int alpha_ctrl0 = 0;
static int alpha_ctrl1 = 0;

static struct timespec start, end;
static unsigned long long time_consumed;
static int mem2mem_fd;

static void *p_src_buf[NUM_BUFS], *p_dst_buf[NUM_BUFS];
static int src_buf_fd[NUM_BUFS], dst_buf_fd[NUM_BUFS];
static size_t src_buf_size[NUM_BUFS], dst_buf_size[NUM_BUFS];
static unsigned int num_src_bufs = 0, num_dst_bufs = 0;

static struct sp_dev* dev_sp;
static struct sp_plane** plane_sp;
static struct sp_crtc* test_crtc_sp;
static struct sp_plane* test_plane_sp;

static void init_mem2mem_dev()
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_control ctrl;
    int ret;

    mem2mem_fd = open(mem2mem_dev_name, O_RDWR | O_CLOEXEC, 0);
    if (mem2mem_fd < 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("open");
        return;
    }

    if (hflip != 0) {
        ctrl.id = V4L2_CID_HFLIP;
        ctrl.value = 1;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set HFLIP failed\n",
                __func__, __LINE__);
    }

    if (vflip != 0) {
        ctrl.id = V4L2_CID_VFLIP;
        ctrl.value = 1;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set VFLIP failed\n",
                __func__, __LINE__);
    }

    if (rotate != 0) {
        ctrl.id = V4L2_CID_ROTATE;
        ctrl.value = rotate;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set ROTATE failed\n",
                __func__, __LINE__);
    }

    if (fill_color != 0) {
        ctrl.id = V4L2_CID_BG_COLOR;
        ctrl.value = fill_color;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set Fill Color failed\n",
                __func__, __LINE__);
    }

    ctrl.id = V4L2_CID_RGA_OP;
    ctrl.value = op;
    ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
    if (ret != 0)
        fprintf(stderr, "%s:%d: Set OP failed\n",
            __func__, __LINE__);

    if (alpha_ctrl0 != 0) {
        ctrl.id = V4L2_CID_RGA_ALHPA_REG0;
        ctrl.value = alpha_ctrl0;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set Alpha0 failed\n",
                __func__, __LINE__);
    }
    if (alpha_ctrl1 != 0) {
        ctrl.id = V4L2_CID_RGA_ALHPA_REG1;
        ctrl.value = alpha_ctrl1;
        ret = ioctl(mem2mem_fd, VIDIOC_S_CTRL, &ctrl);
        if (ret != 0)
            fprintf(stderr, "%s:%d: Set Alpha1 failed\n",
                __func__, __LINE__);
    }

    if (SRC_CROP_X != 0 || SRC_CROP_Y != 0 || SRC_CROP_W != 0 || SRC_CROP_H != 0) {
        if (SRC_CROP_W == 0 && SRC_CROP_H == 0) {
            SRC_CROP_W = SRC_WIDTH;
            SRC_CROP_W = SRC_HEIGHT;
        }
    }

    if (DST_CROP_X != 0 || DST_CROP_Y != 0 || DST_CROP_W != 0 || DST_CROP_H != 0) {
        if (DST_CROP_W == 0 && DST_CROP_H == 0) {
            DST_CROP_W = DST_WIDTH;
            DST_CROP_W = DST_HEIGHT;
        }
    }

    ret = ioctl(mem2mem_fd, VIDIOC_QUERYCAP, &cap);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M)) {
        fprintf(stderr, "Device does not support m2m\n");
        exit(EXIT_FAILURE);
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        exit(EXIT_FAILURE);
    }

    /* Set format for capture */
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = SRC_WIDTH;
    fmt.fmt.pix.height = SRC_HEIGHT;
    fmt.fmt.pix.pixelformat = src_format;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    ret = ioctl(mem2mem_fd, VIDIOC_S_FMT, &fmt);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }

    /* Set format for output */
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = DST_WIDTH;
    fmt.fmt.pix.height = DST_HEIGHT;
    fmt.fmt.pix.pixelformat = dst_format;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    ret = ioctl(mem2mem_fd, VIDIOC_S_FMT, &fmt);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }
}

static void process_mem2mem_frame()
{
    struct v4l2_buffer buf;
    int ret;

    // buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    // buf.memory = V4L2_MEMORY_MMAP;

    // ret = ioctl(mem2mem_fd, VIDIOC_DQBUF, &buf);
    // printf("Dequeued source buffer, index: %d\n", buf.index);

    // /* Enqueue back the buffer (note that the index is preserved) */
    // if (1)
    // {
    //     buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    //     buf.memory = V4L2_MEMORY_MMAP;
    //     ret = ioctl(mem2mem_fd, VIDIOC_QBUF, &buf);
    //     if (ret != 0) {
    //         fprintf(stderr, "%s:%d: ", __func__, __LINE__);
    //         perror("ioctl");
    //         return;
    //     }
    // }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_DMABUF;
    ret = ioctl(mem2mem_fd, VIDIOC_DQBUF, &buf);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }
}

static void fill_buf()
{
}

static void start_mem2mem()
{
    int ret, i;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers reqbuf;
    enum v4l2_buf_type type;

    init_mem2mem_dev();

    memset(&(buf), 0, sizeof(buf));
    reqbuf.count = 1;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbuf.memory = V4L2_MEMORY_DMABUF;
    ret = ioctl(mem2mem_fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }
    num_src_bufs = reqbuf.count;
    printf("Got %d src buffers\n", num_src_bufs);

    reqbuf.count = NUM_BUFS;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(mem2mem_fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }
    num_dst_bufs = reqbuf.count;
    printf("Got %d dst buffers\n", num_dst_bufs);

    {
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = 0;
        ret = ioctl(mem2mem_fd, VIDIOC_QUERYBUF, &buf);
        if (ret != 0) {
            fprintf(stderr, "%s:%d: ", __func__, __LINE__);
            perror("ioctl");
            return;
        }

        src_buf_size[0] = buf.length;

        struct sp_bo* bo
            = create_sp_bo(dev_sp, 1, buf.length, 8, 8, DRM_FORMAT_NV12, 0);
        if (!bo) {
            printf("Failed to create gem buf\n");
            exit(-1);
        }

        drmPrimeHandleToFD(dev_sp->fd, bo->handle, 0, &src_buf_fd[0]);

        memset(&(buf), 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.bytesused = 1;
        buf.index = 0;
        buf.m.fd = src_buf_fd[0];
        ret = ioctl(mem2mem_fd, VIDIOC_QBUF, &buf);
        if (ret != 0) {
            fprintf(stderr, "%s:%d: ", __func__, __LINE__);
            perror("ioctl");
            return;
        }
    }

    for (i = 0; i < num_dst_bufs; ++i) {
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = i;
        ret = ioctl(mem2mem_fd, VIDIOC_QUERYBUF, &buf);
        if (ret != 0) {
            fprintf(stderr, "%s:%d: ", __func__, __LINE__);
            perror("ioctl");
            return;
        }

        dst_buf_size[i] = buf.length;

        struct sp_bo* bo
            = create_sp_bo(dev_sp, 1, buf.length, 8, 8, DRM_FORMAT_NV12, 0);
        if (!bo) {
            printf("Failed to create gem buf\n");
            exit(-1);
        }

        drmPrimeHandleToFD(dev_sp->fd, bo->handle, 0, &dst_buf_fd[i]);

        memset(&(buf), 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = i;
        buf.m.fd = dst_buf_fd[i];
        ret = ioctl(mem2mem_fd, VIDIOC_QBUF, &buf);
        if (ret != 0) {
            fprintf(stderr, "%s:%d: ", __func__, __LINE__);
            perror("ioctl");
            return;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(mem2mem_fd, VIDIOC_STREAMON, &type);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(mem2mem_fd, VIDIOC_STREAMON, &type);
    if (ret != 0) {
        fprintf(stderr, "%s:%d: ", __func__, __LINE__);
        perror("ioctl");
        return;
    }

    process_mem2mem_frame();

    close(mem2mem_fd);

    // munmap(p_src_buf[0], src_buf_size[0]);

    // for (i = 0; i < num_dst_bufs; ++i)
    //     munmap(p_dst_buf[i], dst_buf_size[i]);
}

void init_drm_context()
{
    int ret, i;
    dev_sp = create_sp_dev();
    if (!dev_sp) {
        printf("create_sp_dev failed\n");
        exit(-1);
    }

    ret = initialize_screens(dev_sp);
    if (ret) {
        printf("initialize_screens failed\n");
        printf("please close display server for test!\n");
        exit(-1);
    }
    plane_sp = (struct sp_plane**)calloc(dev_sp->num_planes, sizeof(*plane_sp));
    if (!plane_sp) {
        printf("calloc plane array failed\n");
        exit(-1);
        ;
    }

    test_crtc_sp = &dev_sp->crtcs[0];
    for (i = 0; i < test_crtc_sp->num_planes; i++) {
        plane_sp[i] = get_sp_plane(dev_sp, test_crtc_sp);
        if (is_supported_format(plane_sp[i], DRM_FORMAT_NV12))
            test_plane_sp = plane_sp[i];
    }
    if (!test_plane_sp) {
        printf("test_plane is NULL\n");
        exit(-1);
    }
}

static void usage(FILE* fp, int argc, char** argv)
{
    fprintf(fp,
        "Usage: %s [options]\n\n"
        "Options:\n"
        "--device                   mem2mem device name [/dev/video0]\n"
        "--hel                      Print this message\n"
        "--src-fmt                  Source video format, 0 = NV12, 1 = ARGB32, 2 = RGB565\n"
        "--src-width                Source video width\n"
        "--src-height               Source video height\n"
        "--src-crop-x               Source video crop X offset [0]\n"
        "--src-crop-y               Source video crop Y offset [0]\n"
        "--src-crop-width           Source video crop width [width]\n"
        "--src-crop-height          Source video crop height [height]\n"
        "--dst-fmt                  Destination video format, 0 = NV12, 1 = ARGB32, 2 = RGB565\n"
        "--dst-width                Destination video width\n"
        "--dst-height               Destination video height\n"
        "--dst-crop-x               Destination video crop X offset [0]\n"
        "--dst-crop-y               Destination video crop Y offset [0]\n"
        "--dst-crop-width           Destination video crop width [width]\n"
        "--dst-crop-height          Destination video crop height [height]\n"
        "--op                       Transform operations\n"
        "--fill-color               Solid fill color\n"
        "--rotate                   Rotate\n"
        "--hflip                    Horizontal Mirror\n"
        "--vflip                    Vertical Mirror\n"
        "--num-frames               Number of frames to process [100]\n"
        "--display                  Display\n"
        "--alpha0                   Alpha reg 0\n"
        "--alpha1                   Alpha reg 1\n"
        "",
        argv[0]);
}

static const struct option long_options[] = {
    { "device", required_argument, NULL, 0 },
    { "help", no_argument, NULL, 0 },
    { "src-fmt", required_argument, NULL, 0 },
    { "src-width", required_argument, NULL, 0 },
    { "src-height", required_argument, NULL, 0 },
    { "src-crop-x", required_argument, NULL, 0 },
    { "src-crop-y", required_argument, NULL, 0 },
    { "src-crop-width", required_argument, NULL, 0 },
    { "src-crop-height", required_argument, NULL, 0 },
    { "dst-fmt", required_argument, NULL, 0 },
    { "dst-width", required_argument, NULL, 0 },
    { "dst-height", required_argument, NULL, 0 },
    { "dst-crop-x", required_argument, NULL, 0 },
    { "dst-crop-y", required_argument, NULL, 0 },
    { "dst-crop-width", required_argument, NULL, 0 },
    { "dst-crop-height", required_argument, NULL, 0 },
    { "op", required_argument, NULL, 0 },
    { "fill-color", required_argument, NULL, 0 },
    { "rotate", required_argument, NULL, 0 },
    { "hflip", required_argument, NULL, 0 },
    { "vflip", required_argument, NULL, 0 },
    { "num-frames", required_argument, NULL, 0 },
    { "display", required_argument, NULL, 0 },
    { "alpha0", required_argument, NULL, 0 },
    { "alpha1", required_argument, NULL, 0 },
    { 0, 0, 0, 0 }
};

int main(int argc, char** argv)
{
    mem2mem_dev_name = (char*)"/dev/video0";

    for (;;) {
        int index;
        int c;

        c = getopt_long(argc, argv, "", long_options, &index);

        if (-1 == c)
            break;

        switch (index) {
        case 0: /* getopt_long() flag */
            mem2mem_dev_name = optarg;
            break;

        case 1:
            usage(stdout, argc, argv);
            exit(EXIT_SUCCESS);

        case 2:
            c = atoi(optarg);
            if (c == 0) {
                src_format = V4L2_PIX_FMT_NV12;
            } else if (c == 1) {
                src_format = V4L2_PIX_FMT_ARGB32;
            } else if (c == 2) {
                src_format = V4L2_PIX_FMT_RGB565X;
            }
            break;
        case 3:
            SRC_WIDTH = atoi(optarg);
            break;
        case 4:
            SRC_HEIGHT = atoi(optarg);
            break;
        case 5:
            SRC_CROP_X = atoi(optarg);
            break;
        case 6:
            SRC_CROP_Y = atoi(optarg);
            break;
        case 7:
            SRC_CROP_W = atoi(optarg);
            break;
        case 8:
            SRC_CROP_H = atoi(optarg);
            break;
        case 9:
            c = atoi(optarg);
            if (c == 0) {
                dst_format = V4L2_PIX_FMT_NV12;
            } else if (c == 1) {
                dst_format = V4L2_PIX_FMT_ARGB32;
            } else if (c == 2) {
                dst_format = V4L2_PIX_FMT_RGB565X;
            }
            break;
        case 10:
            DST_WIDTH = atoi(optarg);
            break;
        case 11:
            DST_HEIGHT = atoi(optarg);
            break;
        case 12:
            DST_CROP_X = atoi(optarg);
            break;
        case 13:
            DST_CROP_Y = atoi(optarg);
            break;
        case 14:
            DST_CROP_W = atoi(optarg);
            break;
        case 15:
            DST_CROP_H = atoi(optarg);
            break;
        case 16:
            op = atoi(optarg);
            break;
        case 17:
            fill_color = atoi(optarg);
            break;
        case 18:
            rotate = atoi(optarg);
            break;
        case 19:
            hflip = atoi(optarg);
            break;
        case 20:
            vflip = atoi(optarg);
            break;
        case 21:
            num_frames = atoi(optarg);
            break;
        case 22:
            display = atoi(optarg);
            break;
        case 23:
            alpha_ctrl0 = atoi(optarg);
            break;
        case 24:
            alpha_ctrl1 = atoi(optarg);
            break;
        default:
            usage(stderr, argc, argv);
            exit(EXIT_FAILURE);
        }
    }

    init_drm_context();

    clock_gettime(CLOCK_MONOTONIC, &start);

    start_mem2mem();

    clock_gettime(CLOCK_MONOTONIC, &end);

    time_consumed = (end.tv_sec - start.tv_sec) * 1000000000ULL;
    time_consumed += (end.tv_nsec - start.tv_nsec);
    time_consumed /= 10000;

    printf("*[RGA]* : use %llu usecs\n", time_consumed);

    return 0;
}