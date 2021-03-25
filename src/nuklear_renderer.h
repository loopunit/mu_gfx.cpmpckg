#pragma once

#if USING_NUKLEAR
struct nk_diligent_globals* nk_diligent_init_globals(Diligent::IRenderDevice* device);

struct nk_diligent_context* nk_diligent_init(
	nk_diligent_globals*	 glob,
	unsigned int			 width,
	unsigned int			 height,
	Diligent::TEXTURE_FORMAT BackBufferFmt,
	Diligent::TEXTURE_FORMAT DepthBufferFmt,
	unsigned int			 max_vertex_buffer_size,
	unsigned int			 max_index_buffer_size);

struct nk_context* nk_diligent_get_nk_ctx(struct nk_diligent_context* nk_dlg_ctx);

void nk_diligent_init_resources(struct nk_diligent_context* nk_dlg_ctx, nk_diligent_globals* glob, Diligent::IDeviceContext* device_ctx);

void nk_diligent_render(struct nk_diligent_context* nk_dlg_ctx, Diligent::IDeviceContext* device_ctx, bool in_resize);

void nk_diligent_resize(struct nk_diligent_context* nk_dlg_ctx, nk_diligent_globals* glob, Diligent::IDeviceContext* device_ctx, unsigned int width, unsigned int height);

void nk_diligent_shutdown(struct nk_diligent_context* nk_dlg_ctx);

void nk_diligent_shutdown_globals(struct nk_diligent_globals* glob);

#endif // #if USING_NUKLEAR