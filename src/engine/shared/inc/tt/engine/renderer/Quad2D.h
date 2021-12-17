#if !defined(INC_TT_ENGINE_RENDERER_QUAD2D_H)
#define INC_TT_ENGINE_RENDERER_QUAD2D_H


#include <tt/engine/renderer/ColorRGB.h>
#include <tt/engine/renderer/ColorRGBA.h>
#include <tt/engine/fwd.h>
#include <tt/math/Vector2.h>
#include <tt/platform/tt_types.h>


namespace tt {
namespace engine {
namespace renderer {

class Quad2D
{
public:
	enum Vertex
	{
		// Don't change this order!
		Vertex_BottomLeft,
		Vertex_BottomRight,
		Vertex_TopLeft,
		Vertex_TopRight,
		
		Vertex_Count
	};
	
	static const s8 quadSize = 4;
	
	
	/*! \brief Constructor for creating a basic quad
	    \param p_vertexType Combination of vertex properties
	    \param p_color      [Optional] Initial quad color */
	explicit Quad2D(u32 p_vertexType, const ColorRGBA& p_color = ColorRGB::white);
	
	Quad2D(const Quad2D& p_rhs);
	Quad2D& operator=(const Quad2D& p_rhs);
	~Quad2D();
	
	/*! \brief Change the color of the quad
	    \param p_color New quad color */
	void setColor(const ColorRGBA& p_color);
	
	/*! \brief Change the color of the quad
	    \param p_color New quad color (alpha is not changed) */
	void setColor(const ColorRGB& p_color);
	
	/*! \brief Change the color of a quad corner.
	    \param p_color New color. */
	void setColor(const ColorRGBA& p_color, Vertex p_vertex);
	
	/*! \brief Change the color of a quad corner.
	    \param p_color New color (alpha is not changed). */
	void setColor(const ColorRGB& p_color, Vertex p_vertex);
	
	/*! \brief Change the alpha of the quad
	   \param p_color New alpha value (0 - 255) */
	void setAlpha(u8 p_alpha);
	
	/*! \brief Change the texture coordinates of a specific vertex
	    \param p_vertex   Vertex to change
	    \param p_texcoord New texture coordinates */
	void setTexcoord(Vertex p_vertex, const math::Vector2& p_texcoord);
	
	
	/*! \brief Update the texture coordinates based on frame size
	    \param p_texture     Current texture used for this quad */
	void updateTexcoords(const TexturePtr& p_texture);
	
	/*! \brief Update the texture coordinates based on frame size
	    \param p_texture     Current texture used for this quad
		\param p_frameWidth  Width of a single frame in texels
		\param p_frameHeight Height of a single frame in texels */
	void updateTexcoords(const TexturePtr& p_texture, s32 p_frameWidth, s32 p_frameHeight);
	
	/*! \brief Only used on Nitro */
	inline void setPolygonID(u8) { }
	
	/*! \brief Update the quad (must be called to make changes to properties take effect) */
	void update();
	
	/*! \brief Render this quad on the screen */
	void render() const;
	
private:
	u8 m_vertexType;
	
	// OSX specific
	ColorRGBA     m_color    [Vertex_Count];
	math::Vector2 m_texcoords[Vertex_Count];
};

// Namespace end
}
}
}


#endif // !defined(INC_TT_ENGINE_RENDERER_QUAD_H)
