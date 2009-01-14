#include "xorg-server.h"
#ifdef DRI2
#include "nv_include.h"
#include "dri2.h"

struct nouveau_dri2_buffer {
	PixmapPtr pPixmap;
};

DRI2BufferPtr
nouveau_dri2_create_buffers(DrawablePtr pDraw, unsigned int *attachments,
			    int count)
{
	ScreenPtr pScreen = pDraw->pScreen;
	DRI2BufferPtr dri2_bufs;
	struct nouveau_dri2_buffer *nv_bufs;
	PixmapPtr ppix, pzpix;
	int i;

	dri2_bufs = xcalloc(count, sizeof(*dri2_bufs));
	if (!dri2_bufs)
		return NULL;

	nv_bufs = xcalloc(count, sizeof(*nv_bufs));
	if (!nv_bufs) {
		xfree(dri2_bufs);
		return NULL;
	}

	pzpix = NULL;
	for (i = 0; i < count; i++) {
		if (attachments[i] == DRI2BufferFrontLeft) {
			if (pDraw->type == DRAWABLE_PIXMAP) {
				ppix = (PixmapPtr)pDraw;
			} else {
				WindowPtr pwin = (WindowPtr)pDraw;
				ppix = pScreen->GetWindowPixmap(pwin);
			}

			ppix->refcnt++;
		} else
		if (attachments[i] == DRI2BufferStencil && pzpix) {
			ppix = pzpix;
			ppix->refcnt++;
		} else {
			ppix = pScreen->CreatePixmap(pScreen, pDraw->width,
						     pDraw->height,
						     pDraw->depth, 0);
		}

		if (attachments[i] == DRI2BufferDepth)
			pzpix = ppix;

		dri2_bufs[i].attachment = attachments[i];
		dri2_bufs[i].pitch = ppix->devKind;
		dri2_bufs[i].cpp = ppix->drawable.bitsPerPixel / 8;
		dri2_bufs[i].driverPrivate = &nv_bufs[i];
		dri2_bufs[i].flags = 0;
		nv_bufs[i].pPixmap = ppix;

		nouveau_bo_handle_get(nouveau_pixmap(ppix)->bo,
				      &dri2_bufs[i].name);
	}

	return dri2_bufs;
}

void
nouveau_dri2_destroy_buffers(DrawablePtr pDraw, DRI2BufferPtr buffers,
			     int count)
{
	struct nouveau_dri2_buffer *nvbuf;

	while (count--) {
		nvbuf = buffers[count].driverPrivate;
		pDraw->pScreen->DestroyPixmap(nvbuf->pPixmap);
	}

	if (buffers) {
		xfree(buffers[0].driverPrivate);
		xfree(buffers);
	}
}

void
nouveau_dri2_copy_region(DrawablePtr pDraw, RegionPtr pRegion,
			 DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
	struct nouveau_dri2_buffer *nvbuf = pSrcBuffer->driverPrivate;
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	RegionPtr pCopyClip;
	GCPtr pGC;

	pGC = GetScratchGC(pDraw->depth, pScreen);
	pCopyClip = REGION_CREATE(pScreen, NULL, 0);
	REGION_COPY(pScreen, pCopyClip, pRegion);
	pGC->funcs->ChangeClip(pGC, CT_REGION, pCopyClip, 0);
	ValidateGC(pDraw, pGC);
	pGC->ops->CopyArea(&nvbuf->pPixmap->drawable, pDraw, pGC, 0, 0,
			   pDraw->width, pDraw->height, 0, 0);
	FreeScratchGC(pGC);

	FIRE_RING(pNv->chan);
}

Bool
nouveau_dri2_init(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	DRI2InfoRec dri2;

	dri2.version = 1;
	dri2.fd = nouveau_device(pNv->dev)->fd;
	dri2.driverName = "nouveau";
	dri2.deviceName = "/dev/dri/card0";
	dri2.CreateBuffers = nouveau_dri2_create_buffers;
	dri2.DestroyBuffers = nouveau_dri2_destroy_buffers;
	dri2.CopyRegion = nouveau_dri2_copy_region;

	return DRI2ScreenInit(pScreen, &dri2);
}

void
nouveau_dri2_takedown(ScreenPtr pScreen)
{
	DRI2CloseScreen(pScreen);
}
#endif
