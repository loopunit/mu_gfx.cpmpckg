#include <memory>

#include <Primitives/interface/BasicTypes.h>
#include <Common/interface/BasicMath.hpp>
#include <Common/interface/RefCntAutoPtr.hpp>
#include <Graphics/GraphicsEngine/interface/GraphicsTypes.h>

#include <imgui.h>

struct ImDrawData;

namespace Diligent
{

struct IRenderDevice;
struct IDeviceContext;
struct IBuffer;
struct IPipelineState;
struct ITextureView;
struct IShaderResourceBinding;
struct IShaderResourceVariable;
enum TEXTURE_FORMAT : Uint16;
enum SURFACE_TRANSFORM : Uint32;

class ImGuiDiligentRenderer
{
public:
    ImGuiDiligentRenderer(IRenderDevice* pDevice,
                          TEXTURE_FORMAT BackBufferFmt,
                          TEXTURE_FORMAT DepthBufferFmt,
                          Uint32         InitialVertexBufferSize,
                          Uint32         InitialIndexBufferSize);
    ~ImGuiDiligentRenderer();
    void NewFrame(Uint32            RenderSurfaceWidth,
                  Uint32            RenderSurfaceHeight,
                  SURFACE_TRANSFORM SurfacePreTransform);
    void EndFrame();
    void RenderDrawData(IDeviceContext* pCtx, ImDrawData* pDrawData);
    void InvalidateDeviceObjects();
    void CreateDeviceObjects();
    void CreateFontsTexture();

private:
    inline float4 TransformClipRect(const ImVec2& DisplaySize, const float4& rect) const;

private:
    RefCntAutoPtr<IRenderDevice>          m_pDevice;
    RefCntAutoPtr<IBuffer>                m_pVB;
    RefCntAutoPtr<IBuffer>                m_pIB;
    RefCntAutoPtr<IBuffer>                m_pVertexConstantBuffer;
    RefCntAutoPtr<IPipelineState>         m_pPSO;
    RefCntAutoPtr<ITextureView>           m_pFontSRV;
    RefCntAutoPtr<IShaderResourceBinding> m_pSRB;
    IShaderResourceVariable*              m_pTextureVar = nullptr;

    const TEXTURE_FORMAT m_BackBufferFmt;
    const TEXTURE_FORMAT m_DepthBufferFmt;
    Uint32               m_VertexBufferSize    = 0;
    Uint32               m_IndexBufferSize     = 0;
    Uint32               m_RenderSurfaceWidth  = 0;
    Uint32               m_RenderSurfaceHeight = 0;
    SURFACE_TRANSFORM    m_SurfacePreTransform = SURFACE_TRANSFORM_IDENTITY;
};

} // namespace Diligent
