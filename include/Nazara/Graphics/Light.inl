// Copyright (C) 2015 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

inline float NzLight::GetAmbientFactor() const
{
	return m_ambientFactor;
}

inline float NzLight::GetAttenuation() const
{
	return m_attenuation;
}

inline NzColor NzLight::GetColor() const
{
	return m_color;
}

inline float NzLight::GetDiffuseFactor() const
{
	return m_diffuseFactor;
}

inline float NzLight::GetInnerAngle() const
{
	return m_innerAngle;
}

inline nzLightType NzLight::GetLightType() const
{
	return m_type;
}

inline float NzLight::GetOuterAngle() const
{
	return m_outerAngle;
}

inline float NzLight::GetRadius() const
{
	return m_radius;
}

inline void NzLight::SetAmbientFactor(float factor)
{
	m_ambientFactor = factor;
}

inline void NzLight::SetAttenuation(float attenuation)
{
	m_attenuation = attenuation;
}

inline void NzLight::SetColor(const NzColor& color)
{
	m_color = color;
}

inline void NzLight::SetDiffuseFactor(float factor)
{
	m_diffuseFactor = factor;
}

inline void NzLight::SetInnerAngle(float innerAngle)
{
	m_innerAngle = innerAngle;
	m_innerAngleCosine = std::cos(NzDegreeToRadian(m_innerAngle));
}

inline void NzLight::SetLightType(nzLightType type)
{
	m_type = type;
}

inline void NzLight::SetOuterAngle(float outerAngle)
{
	m_outerAngle = outerAngle;
	m_outerAngleCosine = std::cos(NzDegreeToRadian(m_outerAngle));
	m_outerAngleTangent = std::tan(NzDegreeToRadian(m_outerAngle));

	InvalidateBoundingVolume();
}

inline void NzLight::SetRadius(float radius)
{
	m_radius = radius;

	m_invRadius = 1.f / m_radius;

	InvalidateBoundingVolume();
}
