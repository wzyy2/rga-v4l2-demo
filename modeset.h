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

#ifndef __MODESET_H_INCLUDED__
#define __MODESET_H_INCLUDED__

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct sp_dev;
struct sp_crtc;

int initialize_screens(struct sp_dev *dev);


struct sp_plane* get_sp_plane(struct sp_dev *dev, struct sp_crtc *crtc);
void put_sp_plane(struct sp_plane *plane);

int set_sp_plane(struct sp_dev *dev, struct sp_plane *plane,
		 struct sp_crtc *crtc, int x, int y);

#ifdef USE_ATOMIC_API
int set_sp_plane_pset(struct sp_dev *dev, struct sp_plane *plane,
		      drmModePropertySetPtr pset, struct sp_crtc *crtc, int x, int y);
#endif

#endif /* __MODESET_H_INCLUDED__ */