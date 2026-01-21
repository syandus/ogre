// This file is part of the OGRE project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at https://www.ogre3d.org/licensing.
// SPDX-License-Identifier: MIT

#ifndef RTSS_OUTPUT_GAMMA
#define RTSS_OUTPUT_GAMMA 2.2
#endif

#ifdef USE_LINEAR_COLOURS
#define ENABLE_LINEAR_COLOUR(colour) colour.rgb = pow(colour.rgb, vec3_splat(2.2))
#else
#define ENABLE_LINEAR_COLOUR(colour)
#endif

#if defined(USE_LINEAR_COLOURS) && !defined(TARGET_CONSUMES_LINEAR)
#define COLOUR_TRANSFER(colour) colour.rgb = pow(colour.rgb, vec3_splat(1.0/RTSS_OUTPUT_GAMMA))
#else
#define COLOUR_TRANSFER(colour)
#endif

void RTSS_DESATURATE(inout vec4 colour, float amount)
{
    float luma = dot(colour.rgb, vec3(0.299, 0.587, 0.114));
    colour.rgb = mix(colour.rgb, vec3(luma), clamp(amount, 0.0, 1.0));
}
