
// GLSL Translation 

#define float2 vec2
#define float3 vec3
#define float4 vec4
#define uint2 uvec2

#define mul(a, b) (a * b)
#define saturate(arg)	clamp(arg, 0.f, 1.f)
#define lerp(a, b, t)	mix(a, b, t)


// We should precompute those terms from resolutions (Or set resolution as #defined constants)
float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f)); }
float fromSubUvsToUnit(float u, float resolution) { return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f)); }

// SkyAtmosphereCommon.hlsl /////////////////////////////////////////

/**
 * Returns near intersection in x, far intersection in y, or both -1 if no intersection.
 * RayDirection does not need to be unit length.
 */
float2 RayIntersectSphere(float3 RayOrigin, float3 RayDirection, float4 Sphere)
{
	float3 LocalPosition = RayOrigin - Sphere.xyz;
	float LocalPositionSqr = dot(LocalPosition, LocalPosition);

	float3 QuadraticCoef;
	QuadraticCoef.x = dot(RayDirection, RayDirection);
	QuadraticCoef.y = 2 * dot(RayDirection, LocalPosition);
	QuadraticCoef.z = LocalPositionSqr - Sphere.w * Sphere.w;

	float Discriminant = QuadraticCoef.y * QuadraticCoef.y - 4 * QuadraticCoef.x * QuadraticCoef.z;

	float2 Intersections = vec2(-1.0f);

	// Only continue if the ray intersects the sphere
	//FLATTEN
	if (Discriminant >= 0)
	{
		float SqrtDiscriminant = sqrt(Discriminant);
		Intersections = (-QuadraticCoef.y + float2(-1, 1) * SqrtDiscriminant) / (2 * QuadraticCoef.x);
	}

	return Intersections;
}

// - r0: ray origin
// - rd: normalized ray direction
// - s0: sphere center
// - sR: sphere radius
// - Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.
float raySphereIntersectNearest(float3 r0, float3 rd, float3 s0, float sR)
{
	float a = dot(rd, rd);
	float3 s0_r0 = r0 - s0;
	float b = 2.0 * dot(rd, s0_r0);
	float c = dot(s0_r0, s0_r0) - (sR * sR);
	float delta = b * b - 4.0*a*c;
	if (delta < 0.0 || a == 0.0)
	{
		return -1.0;
	}
	float sol0 = (-b - sqrt(delta)) / (2.0*a);
	float sol1 = (-b + sqrt(delta)) / (2.0*a);
	if (sol0 < 0.0 && sol1 < 0.0)
	{
		return -1.0;
	}
	if (sol0 < 0.0)
	{
		return max(0.0, sol1);
	}
	else if (sol1 < 0.0)
	{
		return max(0.0, sol0);
	}
	return max(0.0, min(sol0, sol1));
}

// RenderSkyCommon.hlsl /////////////////////////////////////////////


#define RENDER_SUN_DISK 1

#if 1
#define USE_CornetteShanks
#define MIE_PHASE_IMPORTANCE_SAMPLING 0
#else
// Mie importance sampling is only used for multiple scattering. Single scattering is fine and noise only due to sample selection on view ray.
// A bit more expenssive so off for now.
#define MIE_PHASE_IMPORTANCE_SAMPLING 1
#endif


#define PLANET_RADIUS_OFFSET 0.01f
#define T_RAY_MAX 9000000.0f

struct Ray
{
	float3 o;
	float3 d;
};

Ray createRay(in float3 p, in float3 d)
{
	Ray r;
	r.o = p;
	r.d = d;
	return r;
}


// 4th order polynomial approximation
// 4 VGRP, 16 ALU Full Rate
// 7 * 10^-5 radians precision
// Reference : Handbook of Mathematical Functions (chapter : Elementary Transcendental Functions), M. Abramowitz and I.A. Stegun, Ed.
float acosFast4(float inX)
{
	float x1 = abs(inX);
	float x2 = x1 * x1;
	float x3 = x2 * x1;
	float s;

	s = -0.2121144f * x1 + 1.5707288f;
	s = 0.0742610f * x2 + s;
	s = -0.0187293f * x3 + s;
	s = sqrt(1.0f - x1) * s;

	// acos function mirroring
	// check per platform if compiles to a selector - no branch neeeded
	return inX >= 0.0f ? s : PI - s;
}


float atan2Fast( float y, float x )
{
	float t0 = max( abs(x), abs(y) );
	float t1 = min( abs(x), abs(y) );
	float t3 = t1 / t0;
	float t4 = t3 * t3;

	// Same polynomial as atanFastPos
	t0 =         + 0.0872929;
	t0 = t0 * t4 - 0.301895;
	t0 = t0 * t4 + 1.0;
	t3 = t0 * t3;

	t3 = abs(y) > abs(x) ? (0.5 * PI) - t3 : t3;
	t3 = x < 0 ? PI - t3 : t3;
	t3 = y < 0 ? -t3 : t3;

	return t3;
}

float safeSqrt(float x)
{
    return sqrt(max(0, x));
}

////////////////////////////////////////////////////////////
// LUT functions
////////////////////////////////////////////////////////////



// Transmittance LUT function parameterisation from Bruneton 2017 https://github.com/ebruneton/precomputed_atmospheric_scattering
// uv in [0,1]
// viewZenithCosAngle in [-1,1]
// viewHeight in [bottom_radius, top_radius]

void LutTransmittanceParamsToUv(AtmosphereParameters Atmosphere, in float viewHeight, in float viewZenithCosAngle, out float2 uv)
{
	float H = sqrt(max(0.0f, Atmosphere.top_radius * Atmosphere.top_radius - Atmosphere.bottom_radius * Atmosphere.bottom_radius));
	float rho = sqrt(max(0.0f, viewHeight * viewHeight - Atmosphere.bottom_radius * Atmosphere.bottom_radius));

	float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + Atmosphere.top_radius * Atmosphere.top_radius;
	float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

	float d_min = Atmosphere.top_radius - viewHeight;
	float d_max = rho + H;
	float x_mu = (d - d_min) / (d_max - d_min);
	float x_r = rho / H;

	uv = float2(x_mu, x_r);
	//uv = float2(fromUnitToSubUvs(uv.x, 192.0f), fromUnitToSubUvs(uv.y, 108.f)); // No real impact so off
}

void UvToLutTransmittanceParams(AtmosphereParameters Atmosphere, out float viewHeight, out float viewZenithCosAngle, in float2 uv)
{
	//uv = float2(fromSubUvsToUnit(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromSubUvsToUnit(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
	float x_mu = uv.x;
	float x_r = uv.y;

	float H = safeSqrt(Atmosphere.top_radius * Atmosphere.top_radius - Atmosphere.bottom_radius * Atmosphere.bottom_radius);
	float rho = H * x_r;
	viewHeight = safeSqrt(rho * rho + Atmosphere.bottom_radius * Atmosphere.bottom_radius);

	float d_min = Atmosphere.top_radius - viewHeight;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
	viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

#define NONLINEARSKYVIEWLUT 1
void UvToSkyViewLutParams(AtmosphereParameters Atmosphere, out float viewZenithCosAngle, out float lightViewCosAngle, in float viewHeight, in float2 uv)
{
	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	//uv = uv.yx;
	uv = float2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

	float Vhorizon = safeSqrt(viewHeight * viewHeight - Atmosphere.bottom_radius * Atmosphere.bottom_radius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acosFast4(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (uv.y < 0.5f)
	{
		float coord = 2.0*uv.y;
		coord = 1.0 - coord;
#if NONLINEARSKYVIEWLUT
		coord *= coord;
#endif
		coord = 1.0 - coord;
		viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
	}
	else
	{
		float coord = uv.y*2.0 - 1.0;
#if NONLINEARSKYVIEWLUT
		coord *= coord;
#endif
		viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
	}

	float coord = uv.x;
	coord *= coord;
	lightViewCosAngle = -(coord*2.0 - 1.0);
}


// SkyViewLut is a new texture used for fast sky rendering.
// It is low resolution of the sky rendering around the camera,
// basically a lat/long parameterisation with more texel close to the horizon for more accuracy during sun set.

void UvToSkyViewLutParams2(AtmosphereParameters Atmosphere, out float3 ViewDir, in float ViewHeight, in float2 uv)
{
	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	//UV = FromSubUvsToUnit(UV, SkyAtmosphere.SkyViewLutSizeAndInvSize);
	uv = float2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

	float Vhorizon = safeSqrt(ViewHeight * ViewHeight - Atmosphere.bottom_radius * Atmosphere.bottom_radius);
	float CosBeta = Vhorizon / ViewHeight;				// cos of zenith angle from horizon to zeniht
	float Beta = acosFast4(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	float ViewZenithAngle;
	if (uv.y < 0.5f)
	{
		float Coord = 2.0f * uv.y;
		Coord = 1.0f - Coord;
		Coord *= Coord;
		Coord = 1.0f - Coord;
		ViewZenithAngle = ZenithHorizonAngle * Coord;
	}
	else
	{
		float Coord = uv.y * 2.0f - 1.0f;
		Coord *= Coord;
		ViewZenithAngle = ZenithHorizonAngle + Beta * Coord;
	}
	float CosViewZenithAngle = cos(ViewZenithAngle);
	float SinViewZenithAngle = safeSqrt(1.0 - CosViewZenithAngle * CosViewZenithAngle) * (ViewZenithAngle > 0.0f ? 1.0f : -1.0f); // Equivalent to sin(ViewZenithAngle)

	float LongitudeViewCosAngle = uv.x * 2.0f * PI;

	// Make sure those values are in range as it could disrupt other math done later such as sqrt(1.0-c*c)
	float CosLongitudeViewCosAngle = cos(LongitudeViewCosAngle);
	float SinLongitudeViewCosAngle = safeSqrt(1.0 - CosLongitudeViewCosAngle * CosLongitudeViewCosAngle) * (LongitudeViewCosAngle <= PI ? 1.0f : -1.0f); // Equivalent to sin(LongitudeViewCosAngle)
	ViewDir = float3(
		SinViewZenithAngle * CosLongitudeViewCosAngle,
		SinViewZenithAngle * SinLongitudeViewCosAngle,
		CosViewZenithAngle
		);
}

void SkyViewLutParamsToUv(AtmosphereParameters Atmosphere, in vec3 view_direction, in bool IntersectGround, in float viewZenithCosAngle, in float lightViewCosAngle, in float viewHeight, out float2 uv)
{
	float Vhorizon = safeSqrt(viewHeight * viewHeight - Atmosphere.bottom_radius * Atmosphere.bottom_radius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acosFast4(CosBeta);
	float ZenithHorizonAngle = PI - Beta;
	float ViewZenithAngle = acosFast4(viewZenithCosAngle);

	if (!IntersectGround)
	{
		float coord = ViewZenithAngle / ZenithHorizonAngle;
		coord = 1.0 - coord;
#if NONLINEARSKYVIEWLUT
		coord = safeSqrt(coord);
#endif
		coord = 1.0 - coord;
		uv.y = coord * 0.5f;
	}
	else
	{
		float coord = (ViewZenithAngle - ZenithHorizonAngle) / Beta;
#if NONLINEARSKYVIEWLUT
		coord = safeSqrt(coord);
#endif
		uv.y = coord * 0.5f + 0.5f;
	}

	{
		float coord = -lightViewCosAngle * 0.5f + 0.5f;
		coord = safeSqrt(coord);
		uv.x = coord;
		//uv.x = (atan2Fast(-view_direction.y, -view_direction.x) + PI) / (2.0f * PI);
	}

	//uv = uv.yx;

	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = float2(fromUnitToSubUvs(uv.x, 192.0f), fromUnitToSubUvs(uv.y, 108.0f));
	uv = clamp( uv, vec2(0.01), vec2(0.99));
}



////////////////////////////////////////////////////////////
// Participating media
////////////////////////////////////////////////////////////



float getAlbedo(float scattering, float extinction)
{
	return scattering / max(0.001, extinction);
}
float3 getAlbedo(float3 scattering, float3 extinction)
{
	return scattering / max(vec3(0.001), extinction);
}


struct MediumSampleRGB
{
	float3 scattering;
	float3 absorption;
	float3 extinction;

	float3 scatteringMie;
	float3 absorptionMie;
	float3 extinctionMie;

	float3 scatteringRay;
	float3 absorptionRay;
	float3 extinctionRay;

	float3 scatteringOzo;
	float3 absorptionOzo;
	float3 extinctionOzo;

	float3 albedo;
};

MediumSampleRGB sampleMediumRGB(in float3 WorldPos, in AtmosphereParameters Atmosphere)
{
	const float viewHeight = max(length(WorldPos) - Atmosphere.bottom_radius, 0.0001);

	const float MieDensityExpScale = Atmosphere.mie_density[1].w;
	const float RayleighDensityExpScale = Atmosphere.rayleigh_density[1].w;
	const float densityMie = exp(MieDensityExpScale * viewHeight);
	const float densityRay = exp(RayleighDensityExpScale * viewHeight);

	const float AbsorptionDensity0LayerWidth = Atmosphere.absorption_density[0].x;
	const float AbsorptionDensity0LinearTerm = Atmosphere.absorption_density[0].w;
	const float AbsorptionDensity0ConstantTerm = Atmosphere.absorption_density[1].x;
	const float AbsorptionDensity1LinearTerm = Atmosphere.absorption_density[2].x;
	const float AbsorptionDensity1ConstantTerm = Atmosphere.absorption_density[2].y;

	const float densityOzo = saturate(viewHeight < AbsorptionDensity0LayerWidth ?
		AbsorptionDensity0LinearTerm * viewHeight + AbsorptionDensity0ConstantTerm :
		AbsorptionDensity1LinearTerm * viewHeight + AbsorptionDensity1ConstantTerm);
	MediumSampleRGB s;

	s.scatteringMie = densityMie * Atmosphere.mie_scattering;
	s.absorptionMie = densityMie * Atmosphere.mie_absorption;
	s.extinctionMie = densityMie * Atmosphere.mie_extinction;

	s.scatteringRay = densityRay * Atmosphere.rayleigh_scattering;
	s.absorptionRay = vec3(0.0f);
	s.extinctionRay = s.scatteringRay + s.absorptionRay;

	s.scatteringOzo = vec3(0.0);
	s.absorptionOzo = densityOzo * Atmosphere.absorption_extinction;
	s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

	s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
	s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
	s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
	s.albedo = getAlbedo(s.scattering, s.extinction);

	return s;
}



////////////////////////////////////////////////////////////
// Sampling functions
////////////////////////////////////////////////////////////



// Generates a uniform distribution of directions over a sphere.
// Random zetaX and zetaY values must be in [0, 1].
// Top and bottom sphere pole (+-zenith) are along the Y axis.
float3 getUniformSphereSample(float zetaX, float zetaY)
{
	float phi = 2.0f * 3.14159f * zetaX;
	float theta = 2.0f * acos(sqrt(1.0f - zetaY));
	float3 dir = float3(sin(theta)*cos(phi), cos(theta), sin(theta)*sin(phi));
	return dir;
}

// Generate a sample (using importance sampling) along an infinitely long path with a given constant extinction.
// Zeta is a random number in [0,1]
float infiniteTransmittanceIS(float extinction, float zeta)
{
	return -log(1.0f - zeta) / extinction;
}
// Normalized PDF from a sample on an infinitely long path according to transmittance and extinction.
float infiniteTransmittancePDF(float extinction, float transmittance)
{
	return extinction * transmittance;
}

// Same as above but a sample is generated constrained within a range t,
// where transmittance = exp(-extinction*t) over that range.
float rangedTransmittanceIS(float extinction, float transmittance, float zeta)
{
	return -log(1.0f - zeta * (1.0f - transmittance)) / extinction;
}



float RayleighPhase(float cosTheta)
{
	float factor = 3.0f / (16.0f * PI);
	return factor * (1.0f + cosTheta * cosTheta);
}

float CornetteShanksMiePhaseFunction(float g, float cosTheta)
{
	float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta)
{
#ifdef USE_CornetteShanks
	return CornetteShanksMiePhaseFunction(g, cosTheta);
#else
	// Reference implementation (i.e. not schlick approximation). 
	// See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
	float numer = 1.0f - g * g;
	float denom = 1.0f + g * g + 2.0f * g * cosTheta;
	return numer / (4.0f * PI * denom * sqrt(denom));
#endif
}

float dualLobPhase(float g0, float g1, float w, float cosTheta)
{
	return lerp(hgPhase(g0, cosTheta), hgPhase(g1, cosTheta), w);
}

float uniformPhase()
{
	return 1.0f / (4.0f * PI);
}



////////////////////////////////////////////////////////////
// Misc functions
////////////////////////////////////////////////////////////



// From http://jcgt.org/published/0006/01/01/
void CreateOrthonormalBasis(in float3 n, out float3 b1, out float3 b2)
{
	float sign = n.z >= 0.0f ? 1.0f : -1.0f; // copysignf(1.0f, n.z);
	const float a = -1.0f / (sign + n.z);
	const float b = n.x * n.y * a;
	b1 = float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
	b2 = float3(b, sign + n.y * n.y * a, -n.y);
}

float mean(float3 v)
{
	return dot(v, float3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f));
}

float whangHashNoise(uint u, uint v, uint s)
{
	uint seed = (u * 1664525u + v) + s;
	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> 15u);
	float value = float(seed) / (4294967296.0);
	return value;
}

bool MoveToTopAtmosphere(inout float3 WorldPos, in float3 WorldDir, in float Atmospheretop_radius)
{
	float viewHeight = length(WorldPos);
	if (viewHeight > Atmospheretop_radius)
	{
		float tTop = raySphereIntersectNearest(WorldPos, WorldDir, float3(0.0f, 0.0f, 0.0f), Atmospheretop_radius);
		if (tTop >= 0.0f)
		{
			float3 UpVector = WorldPos / viewHeight;
			float3 UpOffset = UpVector * -PLANET_RADIUS_OFFSET;
			WorldPos = WorldPos + WorldDir * tTop + UpOffset;
		}
		else
		{
			// Ray is not intersecting the atmosphere
			return false;
		}
	}
	return true; // ok to start tracing
}


float3 GetAtmosphereTransmittance(AtmosphereParameters atmosphere_params, float3 PlanetCenterToWorldPos, float3 WorldDir, uint transmittance_lut_index)
{
	// For each view height entry, transmittance is only stored from zenith to horizon. Earth shadow is not accounted for.
	// It does not contain earth shadow in order to avoid texel linear interpolation artefact when LUT is low resolution.
	// As such, at the most shadowed point of the LUT when close to horizon, pure black with earth shadow is never hit.
	// That is why we analytically compute the virtual planet shadow here.
	const float2 Sol = RayIntersectSphere(PlanetCenterToWorldPos, WorldDir, float4(float3(0.0f, 0.0f, 0.0f), atmosphere_params.bottom_radius));
	if (Sol.x > 0.0f || Sol.y > 0.0f)
	{
		return vec3(0.0f);
	}

	const float PHeight = length(PlanetCenterToWorldPos);
	const float3 UpVector = PlanetCenterToWorldPos / PHeight;
	const float LightZenithCosAngle = dot(WorldDir, UpVector);
	float2 TransmittanceLutUv;
	LutTransmittanceParamsToUv(atmosphere_params, PHeight, LightZenithCosAngle, TransmittanceLutUv);
	//const float3 TransmittanceToLight = Texture2DSampleLevel(TransmittanceLutTexture, TransmittanceLutTextureSampler, TransmittanceLutUv, 0.0f).rgb;
	const float3 TransmittanceToLight = texture_bindless_2d(transmittance_lut_index, TransmittanceLutUv).rgb;
	return TransmittanceToLight;
}

// Taken from :https://github.com/MatejSakmary/atmosphere-bac/blob/main/shaders/draw_far_sky.frag
vec3 sunWithBloom(vec3 worldDir, vec3 sunDir)
{
    const float sunSolidAngle = 1.0 * PI / 180.0;
    const float minSunCosTheta = cos(sunSolidAngle);

    float cosTheta = dot(worldDir, sunDir);
    if(cosTheta >= minSunCosTheta) {return vec3(0.5);}
    float offset = minSunCosTheta - cosTheta;
    float gaussianBloom = exp(-offset * 50000.0) * 0.5;
    float invBloom = 1.0/(0.02 + offset * 300.0) * 0.01;
    return vec3(gaussianBloom + invBloom);
}

float3 GetSunLuminance2(AtmosphereParameters atmosphere_params, float3 WorldPos, float3 WorldDir, float PlanetRadius, float3 sun_direction)
{
	const float3 SunLuminance = sunWithBloom(WorldDir, sun_direction);
	//if (length(SunLuminance) > 0.1) 
	{
		const float3 transmittance_to_light = GetAtmosphereTransmittance( atmosphere_params, WorldPos, sun_direction, atmosphere_params.transmittance_lut_texture_index);
		return SunLuminance * transmittance_to_light;
	}
	
	return SunLuminance;
}

float3 GetSunLuminance(float3 WorldPos, float3 WorldDir, float PlanetRadius, float3 sun_direction)
{
#if RENDER_SUN_DISK
	if (dot(WorldDir, sun_direction) > cos(0.5*0.505*3.14159 / 180.0))
	{
		float t = raySphereIntersectNearest(WorldPos, WorldDir, float3(0.0f, 0.0f, 0.0f), PlanetRadius);
		if (t < 0.0f) // no intersection
		{
			const float3 SunLuminance = vec3(1000000.0); // arbitrary. But fine, not use when comparing the models
			return SunLuminance;// * (1.0 - gScreenshotCaptureActive);
		}
	}
#endif
	return vec3(0);
}


float3 GetMultipleScattering(AtmosphereParameters Atmosphere, float3 scattering, float3 extinction, float3 worlPos, float viewZenithCosAngle)
{
	vec2 uv = saturate(vec2(viewZenithCosAngle*0.5f + 0.5f, (length(worlPos) - Atmosphere.bottom_radius) / (Atmosphere.top_radius - Atmosphere.bottom_radius)));
	uv = vec2(fromUnitToSubUvs(uv.x, 32.0f), fromUnitToSubUvs(uv.y, 32.0f));

	//gstodo
	//vec3 multiScatteredLuminance = MultiScatTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;
	vec3 multiScatteredLuminance = texture_bindless_2d( Atmosphere.multiscattering_texture_index, uv ).rgb;
	return multiScatteredLuminance;
}

float getShadow(in AtmosphereParameters Atmosphere, float3 P)
{
	// TODO
	// First evaluate opaque shadow
	/*float4 shadowUv = mul(gShadowmapViewProjMat, float4(P + float3(0.0, 0.0, -Atmosphere.bottom_radius), 1.0));
	//shadowUv /= shadowUv.w;	// not be needed as it is an ortho projection
	shadowUv.x = shadowUv.x*0.5 + 0.5;
	shadowUv.y = -shadowUv.y*0.5 + 0.5;
	if (all(shadowUv.xyz >= 0.0) && all(shadowUv.xyz < 1.0))
	{
		return ShadowmapTexture.SampleCmpLevelZero(samplerShadow, shadowUv.xy, shadowUv.z);
	}*/
	return 1.0f;
}

/////////////////////////////////////////////////////////////////////
// RenderSkyRayMarching.hlsl
/////////////////////////////////////////////////////////////////////


struct SingleScatteringResult
{
	float3 L;						// Scattered light (luminance)
	float3 OpticalDepth;			// Optical depth (1/m)
	float3 Transmittance;			// Transmittance in [0,1] (unitless)
	float3 MultiScatAs1;

	float3 NewMultiScatStep0Out;
	float3 NewMultiScatStep1Out;
};

SingleScatteringResult IntegrateScatteredLuminance(
	in float2 pixPos, in float3 WorldPos, in float3 WorldDir, in float3 SunDir, in AtmosphereParameters Atmosphere,
	in bool ground, in float SampleCountIni, in float DepthBufferValue, in bool VariableSampleCount,
	in bool MieRayPhase, in float tMaxMax, in float2 gResolution, in uint transmittance_lut)// = 9000000.0f)
{
	const bool debugEnabled = false;//all(uint2(pixPos.xx) == gMouseLastDownPos.xx) && uint(pixPos.y) % 10 == 0 && DepthBufferValue != -1.0f;
	SingleScatteringResult result;// = (SingleScatteringResult)0;
	result.L = vec3(0);
	result.Transmittance = vec3(1.0f);
	result.OpticalDepth = vec3(0);
	result.MultiScatAs1 = vec3(0);
	result.NewMultiScatStep0Out = vec3(0);
	result.NewMultiScatStep1Out = vec3(0);

	// Compute next intersection with atmosphere or ground 
	float3 earthO = float3(0.0f, 0.0f, 0.0f);
	float tMax = 0.0f;

#define HILLAIRE_ORIGINAL
#if defined (HILLAIRE_ORIGINAL)
	float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.bottom_radius);
	float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.top_radius);
	if (tBottom < 0.0f)
	{
		if (tTop < 0.0f)
		{
			tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
			return result;
		}
		else
		{
			tMax = tTop;
		}
	}
	else
	{
		if (tTop > 0.0f)
		{
			tMax = min(tTop, tBottom);
		}
	}
#else
	float tBottom = 0.0f;
	float2 SolB = RayIntersectSphere(WorldPos, WorldDir, float4(earthO, Atmosphere.bottom_radius));
	float2 SolT = RayIntersectSphere(WorldPos, WorldDir, float4(earthO, Atmosphere.top_radius));

	const bool bNoBotIntersection = all(lessThan(SolB, vec2(0.0f)));//all(SolB < 0.0f);
	const bool bNoTopIntersection = all(lessThan(SolT, vec2(0.0f)));//all(SolT < 0.0f);
	if (bNoTopIntersection)
	{
		// No intersection with planet or its atmosphere.
		tMax = 0.0f;
		return result;
	}
	else if (bNoBotIntersection)
	{
		// No intersection with planet, so we trace up to the far end of the top atmosphere 
		// (looking up from ground or edges when see from afar in space).
		tMax = max(SolT.x, SolT.y);
	}
	else
	{
		// Interesection with planet and atmospehre: we simply trace up to the planet ground.
		// We know there is at least one intersection thanks to bNoBotIntersection.
		// If one of the solution is invalid=-1, that means we are inside the planet: we stop tracing by setting tBottom=0.
		tBottom = max(0.0f, min(SolB.x, SolB.y));
		tMax = tBottom;
	}

#endif // HILLAIRE_ORIGINAL

	if (DepthBufferValue >= 0.0f)
	{
		//vec4 H = vec4( uv.x * 2 - 1, uv.y * -2 + 1, raw_depth * 2 - 1, 1 );

		if (DepthBufferValue < 1.0f)
		{
			float3 ClipSpace = float3((pixPos / float2(gResolution))*float2(2.0, -2.0) - float2(1.0, -1.0), DepthBufferValue);
			float4 DepthBufferWorldPos = mul(Atmosphere.inverse_view_projection, float4(ClipSpace, 1.0));
			DepthBufferWorldPos /= DepthBufferWorldPos.w;

			vec3 trace_start_world_position = WorldPos + float3(0.0, -Atmosphere.bottom_radius, 0.0);
			//trace_start_world_position = Atmosphere.camera_position;
			// apply earth offset to go back to origin as top of earth mode. 
			vec3 trace_start = DepthBufferWorldPos.xyz - trace_start_world_position;

			float tDepth = length(trace_start);
			if (tDepth < tMax) {
				tMax = tDepth;
			}

			//if the ray intersects with the atmosphere boundary, make sure we do not apply atmosphere on surfaces are front of it. 
			if (dot(WorldDir, trace_start) <= 0.0) {
				return result;
			}
		}

		// if (VariableSampleCount && ClipSpace.z == 1.0f) {
		// 	return result;
		// }
	}
	tMax = min(tMax, tMaxMax);

	// Sample count 
	float SampleCount = SampleCountIni;
	float SampleCountFloor = SampleCountIni;
	float tMaxFloor = tMax;

	// TODO:gs
	const vec2 RayMarchMinMaxSPP = vec2(4, 14);
	if (VariableSampleCount)
	{
		SampleCount = lerp(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, saturate(tMax*0.01));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const float3 wi = SunDir;
	const float3 wo = WorldDir;
	float cosTheta = dot(wi, wo);
	float MiePhaseValue = hgPhase(Atmosphere.mie_phase_function_g, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction. 
	float RayleighPhaseValue = RayleighPhase(cosTheta);

#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	float3 globalL = vec3(1.0f);
#else
	float3 globalL = vec3(1);//gSunIlluminance;
#endif

	// Ray march the atmosphere to integrate optical depth
	float3 L = vec3(0.0f);
	float3 throughput = vec3(1.0);
	float3 OpticalDepth = vec3(0.0);
	float t = 0.0f;
	float tPrev = 0.0;
	const float SampleSegmentT = 0.3f;

	for (float s = 0.0f; s < SampleCount; s += 1.0f)
	{
		if (VariableSampleCount)
		{
			// More expenssive but artefact free
			float t0 = (s) / SampleCountFloor;
			float t1 = (s + 1.0f) / SampleCountFloor;
			// Non linear distribution of sample within the range.
			t0 = t0 * t0;
			t1 = t1 * t1;
			// Make t0 and t1 world space distances.
			t0 = tMaxFloor * t0;
			if (t1 > 1.0)
			{
				t1 = tMax;
				//	t1 = tMaxFloor;	// this reveal depth slices
			}
			else
			{
				t1 = tMaxFloor * t1;
			}
			//t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); // With dithering required to hide some sampling artefact relying on TAA later? This may even allow volumetric shadow?
			t = t0 + (t1 - t0)*SampleSegmentT;
			dt = t1 - t0;
		}
		else
		{
			//t = tMax * (s + SampleSegmentT) / SampleCount;
			// Exact difference, important for accuracy of multiple scattering
			float NewT = tMax * (s + SampleSegmentT) / SampleCount;
			dt = NewT - t;
			t = NewT;
		}
		float3 P = WorldPos + t * WorldDir;

#if DEBUGENABLED 
		if (debugEnabled)
		{
			float3 Pprev = WorldPos + tPrev * WorldDir;
			float3 TxToDebugWorld = float3(0, 0, -Atmosphere.bottom_radius);
			addGpuDebugLine(TxToDebugWorld + Pprev, TxToDebugWorld + P, float3(0.2, 1, 0.2));
			addGpuDebugCross(TxToDebugWorld + P, float3(0.2, 0.2, 1.0), 0.2);
		}
#endif

		MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
		const float3 SampleOpticalDepth = medium.extinction * dt;
		const float3 SampleTransmittance = exp(-SampleOpticalDepth);
		OpticalDepth += SampleOpticalDepth;

		float pHeight = length(P);
		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
		// TODO:gs
		float3 TransmittanceToSun = texture_bindless_2d(transmittance_lut, uv).rgb;//TransmittanceLutTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;

		float3 PhaseTimesScattering;
		if (MieRayPhase)
		{
			PhaseTimesScattering = medium.scatteringMie * MiePhaseValue + medium.scatteringRay * RayleighPhaseValue;
		}
		else
		{
			PhaseTimesScattering = medium.scattering * uniformPhase;
		}

		// Earth shadow 
		float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.bottom_radius);
		float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

		// Dual scattering for multi scattering 

		float3 multiScatteredLuminance = vec3(0.0f);
#if defined (MULTISCATAPPROX_ENABLED)
		//multiScatteredLuminance = GetMultipleScattering(Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle);
#endif

		float shadow = 1.0f;
/*#if SHADOWMAP_ENABLED
		// First evaluate opaque shadow
		shadow = getShadow(Atmosphere, P);
#endif*/

		float3 S = globalL * (earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering + multiScatteredLuminance * medium.scattering);

		// When using the power serie to accumulate all sattering order, serie r must be <1 for a serie to converge.
		// Under extreme coefficient, MultiScatAs1 can grow larger and thus result in broken visuals.
		// The way to fix that is to use a proper analytical integration as proposed in slide 28 of http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
		// However, it is possible to disable as it can also work using simple power serie sum unroll up to 5th order. The rest of the orders has a really low contribution.
#define MULTI_SCATTERING_POWER_SERIE 1

#if MULTI_SCATTERING_POWER_SERIE==0
		// 1 is the integration of luminance over the 4pi of a sphere, and assuming an isotropic phase function of 1.0/(4*PI)
		result.MultiScatAs1 += throughput * medium.scattering * 1 * dt;
#else
		float3 MS = medium.scattering * 1;
		float3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
		result.MultiScatAs1 += throughput * MSint;
#endif

		// Evaluate input to multi scattering 
		{
			float3 newMS;

			newMS = earthShadow * TransmittanceToSun * medium.scattering * uniformPhase * 1;
			result.NewMultiScatStep0Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

			newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
			result.NewMultiScatStep1Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
			//	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
		}

#if 0
		L += throughput * S * dt;
		throughput *= SampleTransmittance;
#else
		// See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/ 
		float3 Sint = (S - S * SampleTransmittance) / medium.extinction;	// integrate along the current step segment 
		L += throughput * Sint;														// accumulate and also take into account the transmittance from previous steps
		throughput *= SampleTransmittance;
#endif

		tPrev = t;
	}

	if (ground && tMax == tBottom && tBottom > 0.0)
	{
		// Account for bounced light off the earth
		float3 P = WorldPos + tBottom * WorldDir;
		float pHeight = length(P);

		const float3 UpVector = P / pHeight;
		float SunZenithCosAngle = dot(SunDir, UpVector);
		float2 uv;
		LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle, uv);
		// TODO:gs
		float3 TransmittanceToSun = texture_bindless_2d(transmittance_lut, uv).rgb;//TransmittanceLutTexture.SampleLevel(samplerLinearClamp, uv, 0).rgb;

		const float NdotL = saturate(dot(normalize(UpVector), normalize(SunDir)));
		L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.ground_albedo / PI;
	}

	result.L = L;
	result.OpticalDepth = OpticalDepth;
	result.Transmittance = throughput;
	return result;
}


vec3 IntegrateOpticalDepth(
	in float3 WorldPos, in float3 WorldDir, in float3 SunDir, in AtmosphereParameters Atmosphere,
	in bool ground, in float SampleCountIni, in float tMaxMax, in bool VariableSampleCount )
{
	// Compute next intersection with atmosphere or ground 
	// TODO:gs another empirical finding. This removes a white pixel stripe in the Transmittance LUT.
	float3 earthO = float3(0.0f, 0.0f, -0.001f);
	float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.bottom_radius);
	float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.top_radius);
	float tMax = 0.0f;
	if (tBottom < 0.0f)
	{
		if (tTop < 0.0f)
		{
			tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
			return vec3(0);
		}
		else
		{
			tMax = tTop;
		}
	}
	else
	{
		if (tTop > 0.0f)
		{
			tMax = min(tTop, tBottom);
		}
	}
	
	tMax = min(tMax, tMaxMax);

	// Sample count 
	float SampleCount = SampleCountIni;
	float SampleCountFloor = SampleCountIni;
	float tMaxFloor = tMax;

	// TODO:gs
	const vec2 RayMarchMinMaxSPP = vec2(4, 14);
	if (VariableSampleCount)
	{
		SampleCount = lerp(RayMarchMinMaxSPP.x, RayMarchMinMaxSPP.y, saturate(tMax*0.01));
		SampleCountFloor = floor(SampleCount);
		tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
	}
	float dt = tMax / SampleCount;

	// Phase functions
	const float uniformPhase = 1.0 / (4.0 * PI);
	const float3 wi = SunDir;
	const float3 wo = WorldDir;
	float cosTheta = dot(wi, wo);
	float MiePhaseValue = hgPhase(Atmosphere.mie_phase_function_g, -cosTheta);	// negate cosTheta because due to WorldDir being a "in" direction. 
	float RayleighPhaseValue = RayleighPhase(cosTheta);

#ifdef ILLUMINANCE_IS_ONE
	// When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
	// This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
	float3 globalL = vec3(1.0f);
#else
	float3 globalL = vec3(1);//gSunIlluminance;
#endif

	// Ray march the atmosphere to integrate optical depth
	float3 L = vec3(0.0f);
	float3 throughput = vec3(1.0);
	float3 OpticalDepth = vec3(0.0);
	float t = 0.0f;
	float tPrev = 0.0;
	const float SampleSegmentT = 0.3f;

	for (float s = 0.0f; s < SampleCount; s += 1.0f)
	{
		if (VariableSampleCount)
		{
			// More expenssive but artefact free
			float t0 = (s) / SampleCountFloor;
			float t1 = (s + 1.0f) / SampleCountFloor;
			// Non linear distribution of sample within the range.
			t0 = t0 * t0;
			t1 = t1 * t1;
			// Make t0 and t1 world space distances.
			t0 = tMaxFloor * t0;
			if (t1 > 1.0)
			{
				t1 = tMax;
				//	t1 = tMaxFloor;	// this reveal depth slices
			}
			else
			{
				t1 = tMaxFloor * t1;
			}
			//t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); // With dithering required to hide some sampling artefact relying on TAA later? This may even allow volumetric shadow?
			t = t0 + (t1 - t0)*SampleSegmentT;
			dt = t1 - t0;
		}
		else
		{
			//t = tMax * (s + SampleSegmentT) / SampleCount;
			// Exact difference, important for accuracy of multiple scattering
			float NewT = tMax * (s + SampleSegmentT) / SampleCount;
			dt = NewT - t;
			t = NewT;
		}
		float3 P = WorldPos + t * WorldDir;

		MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
		const float3 SampleOpticalDepth = medium.extinction * dt;
		OpticalDepth += SampleOpticalDepth;

		tPrev = t;
	}

	return OpticalDepth;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Earth - World conversions
///////////////////////////////////////////////////////////////////////////////////////////////////

// Earth position is in Kilometers, as the other atmosphere parameters!
vec3 earth_position_from_world( in vec3 world_position, AtmosphereParameters atmosphere_params ) {

	vec3 earth_position = world_position + vec3(0, atmosphere_params.bottom_radius, 0);
    earth_position.y = max(earth_position.y, atmosphere_params.bottom_radius + 0.001);

    return earth_position;
}

vec3 earth_position_from_uv_depth( in vec2 uv, in float depth, AtmosphereParameters atmosphere_params, out vec3 world_direction ) {

	vec3 pixel_world_position = world_position_from_depth( uv, depth, atmosphere_params.inverse_view_projection );
    vec3 camera_world_position = atmosphere_params.camera_position;

    world_direction = normalize(pixel_world_position - camera_world_position);
    vec3 earth_position = earth_position_from_world( camera_world_position, atmosphere_params );

    return earth_position;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Aerial Perspective
///////////////////////////////////////////////////////////////////////////////////////////////////
#define AP_TEXTURE_SIZE 32.0f
#define AP_SLICE_COUNT 32.0f
#define AP_KM_PER_SLICE 4.0f

float AerialPerspectiveDepthToSlice(float depth)
{
    return depth * (1.0f / AP_KM_PER_SLICE);
}
float AerialPerspectiveSliceToDepth(float slice)
{
    return slice * AP_KM_PER_SLICE;
}
