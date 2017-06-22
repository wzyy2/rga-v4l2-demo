/*
 * Copyright 2016 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef __DEV_H_INCLUDED__
#define __DEV_H_INCLUDED__

#include <stdint.h>
#include <xf86drmMode.h>

struct sp_bo;
struct sp_dev;

struct sp_plane {
	struct sp_dev *dev;
	drmModePlanePtr plane;
	struct sp_bo *bo;
	int in_use;
	uint32_t format;

	/* Property ID's */
	uint32_t crtc_pid;
	uint32_t fb_pid;
	uint32_t zpos_pid;
	uint32_t crtc_x_pid;
	uint32_t crtc_y_pid;
	uint32_t crtc_w_pid;
	uint32_t crtc_h_pid;
	uint32_t src_x_pid;
	uint32_t src_y_pid;
	uint32_t src_w_pid;
	uint32_t src_h_pid;
};

struct sp_crtc {
	drmModeCrtcPtr crtc;
	int pipe;
	int num_planes;
	struct sp_bo *scanout;
};

struct sp_dev {
	int fd;

	int num_connectors;
	drmModeConnectorPtr *connectors;

	int num_encoders;
	drmModeEncoderPtr *encoders;

	int num_crtcs;
	struct sp_crtc *crtcs;

	int num_planes;
	struct sp_plane *planes;
};

int is_supported_format(struct sp_plane *plane, uint32_t format);
struct sp_dev* create_sp_dev(void);
void destroy_sp_dev(struct sp_dev *dev);

#endif /* __DEV_H_INCLUDED__ */