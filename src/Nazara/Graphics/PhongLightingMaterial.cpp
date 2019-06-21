// Copyright (C) 2017 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Graphics/PhongLightingMaterial.hpp>
#include <Nazara/Core/Algorithm.hpp>
#include <Nazara/Core/ErrorFlags.hpp>
#include <Nazara/Renderer/Renderer.hpp>
#include <Nazara/Utility/BufferMapper.hpp>
#include <Nazara/Utility/FieldOffsets.hpp>
#include <Nazara/Utility/MaterialData.hpp>
#include <cassert>
#include <Nazara/Graphics/Debug.hpp>

namespace Nz
{
	namespace
	{
		constexpr std::size_t AlphaMapBinding = 0;
		constexpr std::size_t DiffuseMapBinding = 1;
		constexpr std::size_t EmissiveMapBinding = 2;
		constexpr std::size_t HeightMapBinding = 3;
		constexpr std::size_t NormalMapBinding = 4;
		constexpr std::size_t SpecularMapBinding = 5;
	}

	PhongLightingMaterial::PhongLightingMaterial(Material* material) :
	m_material(material)
	{
		NazaraAssert(material, "Invalid material");

		// Most common case: don't fetch texture indexes as a little optimization
		const std::shared_ptr<const MaterialSettings>& materialSettings = material->GetSettings();
		if (materialSettings == s_materialSettings)
		{
			m_textureIndexes = s_textureIndexes;
			m_phongUniformOffsets = s_phongUniformOffsets;
		}
		else
		{
			m_textureIndexes.alpha = materialSettings->GetTextureIndex("alpha");
			m_textureIndexes.diffuse = materialSettings->GetTextureIndex("diffuse");
			m_textureIndexes.emissive = materialSettings->GetTextureIndex("emissive");
			m_textureIndexes.height = materialSettings->GetTextureIndex("height");
			m_textureIndexes.normal = materialSettings->GetTextureIndex("normal");
			m_textureIndexes.specular = materialSettings->GetTextureIndex("specular");

			m_phongUniformIndex = materialSettings->GetUniformBlockIndex("PhongSettings");

			m_phongUniformOffsets.alphaThreshold = materialSettings->GetUniformBlockVariableOffset(m_phongUniformIndex, "AlphaThreshold");
			m_phongUniformOffsets.ambientColor = materialSettings->GetUniformBlockVariableOffset(m_phongUniformIndex, "AmbientColor");
			m_phongUniformOffsets.diffuseColor = materialSettings->GetUniformBlockVariableOffset(m_phongUniformIndex, "DiffuseColor");
			m_phongUniformOffsets.shininess = materialSettings->GetUniformBlockVariableOffset(m_phongUniformIndex, "Shininess");
			m_phongUniformOffsets.specularColor = materialSettings->GetUniformBlockVariableOffset(m_phongUniformIndex, "SpecularColor");
		}
	}

	float PhongLightingMaterial::GetAlphaThreshold() const
	{
		NazaraAssert(HasAlphaThreshold(), "Material has no alpha threshold uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_ReadOnly);
		return *AccessByOffset<const float>(mapper.GetPointer(), m_phongUniformOffsets.alphaThreshold);
	}

	Color PhongLightingMaterial::GetAmbientColor() const
	{
		NazaraAssert(HasAmbientColor(), "Material has no ambient color uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_ReadOnly);

		const float* colorPtr = AccessByOffset<const float>(mapper.GetPointer(), m_phongUniformOffsets.ambientColor);
		return Color(colorPtr[0] * 255, colorPtr[1] * 255, colorPtr[2] * 255, colorPtr[3] * 255); //< TODO: Make color able to use float
	}

	Color PhongLightingMaterial::GetDiffuseColor() const
	{
		NazaraAssert(HasDiffuseColor(), "Material has no diffuse color uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_ReadOnly);

		const float* colorPtr = AccessByOffset<const float>(mapper.GetPointer(), m_phongUniformOffsets.diffuseColor);
		return Color(colorPtr[0] * 255, colorPtr[1] * 255, colorPtr[2] * 255, colorPtr[3] * 255); //< TODO: Make color able to use float
	}

	float Nz::PhongLightingMaterial::GetShininess() const
	{
		NazaraAssert(HasShininess(), "Material has no shininess uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_ReadOnly);
		return *AccessByOffset<const float>(mapper.GetPointer(), m_phongUniformOffsets.shininess);
	}

	Color PhongLightingMaterial::GetSpecularColor() const
	{
		NazaraAssert(HasSpecularColor(), "Material has no specular color uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_ReadOnly);

		const float* colorPtr = AccessByOffset<const float>(mapper.GetPointer(), m_phongUniformOffsets.specularColor);
		return Color(colorPtr[0] * 255, colorPtr[1] * 255, colorPtr[2] * 255, colorPtr[3] * 255); //< TODO: Make color able to use float
	}

	void PhongLightingMaterial::SetAlphaThreshold(float alphaThreshold)
	{
		NazaraAssert(HasAlphaThreshold(), "Material has no alpha threshold uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_WriteOnly);
		*AccessByOffset<float>(mapper.GetPointer(), m_phongUniformOffsets.alphaThreshold) = alphaThreshold;
	}

	void PhongLightingMaterial::SetAmbientColor(const Color& ambient)
	{
		NazaraAssert(HasAmbientColor(), "Material has no ambient color uniform");

		BufferMapper<UniformBuffer> mapper(m_material->GetUniformBuffer(m_phongUniformIndex), BufferAccess_WriteOnly);
		float* colorPtr = AccessByOffset<float>(mapper.GetPointer(), m_phongUniformOffsets.ambientColor);
		colorPtr[0] = ambient.r / 255.f;
		colorPtr[1] = ambient.g / 255.f;
		colorPtr[2] = ambient.b / 255.f;
		colorPtr[3] = ambient.a / 255.f;
	}

	const std::shared_ptr<MaterialSettings>& PhongLightingMaterial::GetSettings()
	{
		return s_materialSettings;
	}

	bool PhongLightingMaterial::Initialize()
	{
		RenderPipelineLayoutInfo info;
		info.bindings.assign({
			{
				"MaterialAlphaMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				AlphaMapBinding
			},
			{
				"MaterialDiffuseMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				DiffuseMapBinding
			},
			{
				"MaterialEmissiveMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				EmissiveMapBinding
			},
			{
				"MaterialHeightMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				HeightMapBinding
			},
			{
				"MaterialNormalMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				NormalMapBinding
			},
			{
				"MaterialSpecularMap",
				ShaderBindingType_Texture,
				ShaderStageType_Fragment,
				SpecularMapBinding
			}
		});

		s_renderPipelineLayout = RenderPipelineLayout::New();
		s_renderPipelineLayout->Create(info);

		s_materialSettings = std::make_shared<MaterialSettings>();

		FieldOffsets fieldOffsets(StructLayout_Std140);

		s_phongUniformOffsets.alphaThreshold = fieldOffsets.AddField(StructFieldType_Float1);
		s_phongUniformOffsets.shininess = fieldOffsets.AddField(StructFieldType_Float1);
		s_phongUniformOffsets.ambientColor = fieldOffsets.AddField(StructFieldType_Float4);
		s_phongUniformOffsets.diffuseColor = fieldOffsets.AddField(StructFieldType_Float4);
		s_phongUniformOffsets.specularColor = fieldOffsets.AddField(StructFieldType_Float4);

		std::vector<MaterialSettings::UniformVariable> variables;
		variables.assign({
			{
				"AlphaThreshold",
				s_phongUniformOffsets.alphaThreshold
			},
			{
				"Shininess",
				s_phongUniformOffsets.shininess
			},
			{
				"AmbientColor",
				s_phongUniformOffsets.ambientColor
			},
			{
				"DiffuseColor",
				s_phongUniformOffsets.diffuseColor
			},
			{
				"SpecularColor",
				s_phongUniformOffsets.specularColor
			}
		});

		s_materialSettings->uniformBlocks.assign({
			{
				"PhongSettings",
				fieldOffsets.GetSize(),
				"MaterialPhongSettings",
				std::move(variables)
			}
		});

		s_textureIndexes.alpha = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Alpha",
			ImageType_2D,
			"MaterialAlphaMap"
		});
		
		s_textureIndexes.diffuse = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Diffuse",
			ImageType_2D,
			"MaterialDiffuseMap"
		});

		s_textureIndexes.emissive = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Emissive",
			ImageType_2D,
			"MaterialEmissiveMap"
		});

		s_textureIndexes.height = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Height",
			ImageType_2D,
			"MaterialHeightMap"
		});

		s_textureIndexes.normal = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Normal",
			ImageType_2D,
			"MaterialNormalMap"
		});

		s_textureIndexes.specular = s_materialSettings->textures.size();
		s_materialSettings->textures.push_back({
			"Specular",
			ImageType_2D,
			"MaterialSpecularMap"
		});

		return true;
	}

	void PhongLightingMaterial::Uninitialize()
	{
		s_renderPipelineLayout.Reset();
		s_materialSettings.reset();
	}

	std::shared_ptr<MaterialSettings> PhongLightingMaterial::s_materialSettings;
	RenderPipelineLayoutRef PhongLightingMaterial::s_renderPipelineLayout;
	PhongLightingMaterial::TextureIndexes PhongLightingMaterial::s_textureIndexes;
	PhongLightingMaterial::UniformOffsets PhongLightingMaterial::s_phongUniformOffsets;
}