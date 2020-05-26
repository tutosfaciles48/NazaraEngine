#include <ShaderNode/DataModels/ShaderNode.hpp>


inline ShaderGraph& ShaderNode::GetGraph()
{
	return m_graph;
}

inline const ShaderGraph& ShaderNode::GetGraph() const
{
	return m_graph;
}

inline void ShaderNode::SetPreviewSize(const Nz::Vector2i& size)
{
	m_previewSize = size;
	if (m_isPreviewEnabled)
		UpdatePreview();
}
