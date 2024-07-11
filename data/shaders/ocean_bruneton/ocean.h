/**
 * Real-time Realistic Ocean Lighting using Seamless Transitions from Geometry to BRDF
 * Copyright (c) 2009 INRIA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Author: Eric Bruneton
 */

// TODO(marco): move these to specialization or push constants
#define SEA_CONTRIB
#define SUN_CONTRIB
#define SKY_CONTRIB
#define HARDWARE_ANISTROPIC_FILTERING

layout (std140, set = 1, binding = 0) uniform Local {
    mat4 screenToCamera; // screen space to camera space
    mat4 cameraToWorld; // camera space to world space
    mat4 worldToScreen; // world space to screen space
    mat2 worldToWind; // world space to wind space
    mat2 windToWorld; // wind space to world space

    vec3 worldCamera; // camera position in world space
    float nbWaves; // number of waves

    vec3 worldSunDir; // sun direction in world space
    float heightOffset; // so that surface height is centered around z = 0

    vec2 sigmaSqTotal; // total x and y variance in wind space
    float time; // current time
    float nyquistMin; // Nmin parameter

    // grid cell size in pixels, angle under which a grid cell is seen,
    // and parameters of the geometric series used for wavelengths
    vec4 lods;

    vec3 seaColor; // sea bottom color
    float nyquistMax; // Nmax parameter

    float hdrExposure;
    vec3  padding_;
};

layout ( set = 1, binding = 1 ) uniform sampler1D wavesSampler; // waves parameters (h, omega, kx, ky) in wind space
layout ( set = 1, binding = 2 ) uniform sampler2D skySampler;
layout ( set = 1, binding = 3 ) uniform sampler2D skyIrradianceSampler;
layout ( set = 1, binding = 4 ) uniform sampler2D transmittanceSampler;
