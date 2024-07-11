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

layout( location = 0 ) in vec2 aPosition;

layout (location = 0) out vec2 u; // coordinates in wind space used to compute P(u)
layout (location = 1) out vec3 P; // wave point P(u) in world space
layout (location = 2) out vec3 _dPdu; // dPdu in wind space, used to compute N
layout (location = 3) out vec3 _dPdv; // dPdv in wind space, used to compute N
layout (location = 4) out vec2 _sigmaSq; // variance of unresolved waves in wind space

layout (location = 5) out float lod;

void main() {
    vec4 position = vec4( aPosition.xy, 0, 1 );

    vec3 cameraDir = normalize((screenToCamera * position).xyz);
    vec3 worldDir = (cameraToWorld * vec4(cameraDir, 0.0)).xyz;
    float t = (heightOffset - 2.0f) / worldDir.z;

    u = worldToWind * (worldCamera.xy + t * worldDir.xy);
    vec3 dPdu = vec3(1.0, 0.0, 0.0);
    vec3 dPdv = vec3(0.0, 1.0, 0.0);
    vec2 sigmaSq = sigmaSqTotal;

    lod = - t / worldDir.z * lods.y; // size in meters of one grid cell, projected on the sea surface

    vec3 dP = vec3(0.0, 0.0, heightOffset);
    float iMin = max(0.0, floor((log2(nyquistMin * lod) - lods.z) * lods.w));
    for (float i = iMin; i < nbWaves; i += 1.0) {
        vec4 wt = textureLod(wavesSampler, (i + 0.5) / nbWaves, 0.0);
        float phase = wt.y * time - dot(wt.zw, u);
        float s = sin(phase);
        float c = cos(phase);
        float overk = g / (wt.y * wt.y);

        float wp = smoothstep(nyquistMin, nyquistMax, (2.0 * M_PI) * overk / lod);

        vec3 factor = wp * wt.x * vec3(wt.zw * overk, 1.0);
        dP += factor * vec3(s, s, c);

        vec3 dPd = factor * vec3(c, c, -s);
        dPdu -= dPd * wt.z;
        dPdv -= dPd * wt.w;

        wt.zw *= overk;
        float kh = wt.x / overk;
        sigmaSq -= vec2(wt.z * wt.z, wt.w * wt.w) * (1.0 - sqrt(1.0 - kh * kh));
    }

    P = vec3(windToWorld * (u + dP.xy), dP.z);

    if (t > 0.0) {
        position = worldToScreen * vec4(P, 1.0);
    }

    gl_Position = position;

    _dPdu = dPdu;
    _dPdv = dPdv;
    _sigmaSq = sigmaSq;
}
