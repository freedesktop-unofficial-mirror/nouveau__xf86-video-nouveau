/*
 * Copyright 2007 Ben Skeggs
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

static Bool
NVAccelInitNullObject(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvNullObject, NV01_NULL))
			return FALSE;
		have_object = TRUE;
	}

	return TRUE;
}

uint32_t
NVAccelGetPixmapOffset(PixmapPtr pPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	unsigned long offset;

	offset = exaGetPixmapOffset(pPix);
	if (offset >= pNv->FB->size) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "AII, passed bad pixmap: offset 0x%lx\n", offset);
		return pNv->FB->offset;
	}
	offset += pNv->FB->offset;

	return offset;
}

static Bool
NVAccelInitDmaNotifier0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;

	if (!have_object) {
		pNv->Notifier0 = NVNotifierAlloc(pScrn, NvDmaNotifier0);
		if (!pNv->Notifier0)
			return FALSE;
		have_object = TRUE;
	}

	return TRUE;
}

/* FLAGS_ROP_AND, DmaFB, DmaFB, 0 */
static Bool
NVAccelInitContextSurfaces(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = (pNv->Architecture >= NV_10) ? NV10_CONTEXT_SURFACES_2D :
					       NV04_CONTEXT_SURFACES_2D;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvContextSurfaces, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_DMA_NOTIFY, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvContextSurfaces,
		   NV04_CONTEXT_SURFACES_2D_DMA_IMAGE_SOURCE, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);

	return TRUE;
}

/* FLAGS_ROP_AND, DmaFB, DmaFB, 0 */
static Bool
NVAccelInitContextBeta1(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = 0x12;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvContextBeta1, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvContextBeta1, 0x300, 1); /*alpha factor*/
	OUT_RING  (0xff << 23);

	return TRUE;
}


static Bool
NVAccelInitContextBeta4(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;
	
	class = 0x72;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvContextBeta4, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvContextBeta4, 0x300, 1); /*RGBA factor*/
	OUT_RING  (0xffff0000);
	return TRUE;
}

Bool
NVAccelGetCtxSurf2DFormatFromPixmap(PixmapPtr pPix, int *fmt_ret)
{
	switch (pPix->drawable.bitsPerPixel) {
	case 32:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_A8R8G8B8;
		break;
	case 24:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_X8R8G8B8_Z8R8G8B8;
		break;
	case 16:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_R5G6B5;
		break;
	case 8:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

Bool
NVAccelGetCtxSurf2DFormatFromPicture(PicturePtr pPict, int *fmt_ret)
{
	switch (pPict->format) {
	case PICT_a8r8g8b8:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_A8R8G8B8;
		break;
	case PICT_x8r8g8b8:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_X8R8G8B8_Z8R8G8B8;
		break;
	case PICT_r5g6b5:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_R5G6B5;
		break;
	case PICT_a8:
		*fmt_ret = NV04_CONTEXT_SURFACES_2D_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

Bool
NVAccelSetCtxSurf2D(PixmapPtr psPix, PixmapPtr pdPix, int format)
{
	ScrnInfoPtr pScrn = xf86Screens[psPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
	OUT_RING  (format);
	OUT_RING  (((uint32_t)exaGetPixmapPitch(pdPix) << 16) |
			 (uint32_t)exaGetPixmapPitch(psPix));
	OUT_RING  (NVAccelGetPixmapOffset(psPix));
	OUT_RING  (NVAccelGetPixmapOffset(pdPix));

	return TRUE;
}

static Bool
NVAccelInitImagePattern(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = NV04_IMAGE_PATTERN;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvImagePattern, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvImagePattern, NV04_IMAGE_PATTERN_DMA_NOTIFY, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvImagePattern, NV04_IMAGE_PATTERN_MONOCHROME_FORMAT, 3);
#if X_BYTE_ORDER == X_BIG_ENDIAN
	OUT_RING  (NV04_IMAGE_PATTERN_MONOCHROME_FORMAT_LE);
#else
	OUT_RING  (NV04_IMAGE_PATTERN_MONOCHROME_FORMAT_CGA6);
#endif
	OUT_RING  (NV04_IMAGE_PATTERN_MONOCHROME_SHAPE_8X8);
	OUT_RING  (NV04_IMAGE_PATTERN_PATTERN_SELECT_MONO);

	return TRUE;
}

static Bool
NVAccelInitRasterOp(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = NV03_CONTEXT_ROP;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvRop, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvRop, NV03_CONTEXT_ROP_DMA_NOTIFY, 1);
	OUT_RING  (NvNullObject);

	pNv->currentRop = ~0;
	return TRUE;
}

static Bool
NVAccelInitRectangle(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = NV04_GDI_RECTANGLE_TEXT;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvRectangle, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_DMA_NOTIFY, 1);
	OUT_RING  (NvDmaNotifier0);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_DMA_FONTS, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_SURFACE, 1);
	OUT_RING  (NvContextSurfaces);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_ROP, 1);
	OUT_RING  (NvRop);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_PATTERN, 1);
	OUT_RING  (NvImagePattern);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_OPERATION, 1);
	OUT_RING  (NV04_GDI_RECTANGLE_TEXT_OPERATION_ROP_AND);
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_MONOCHROME_FORMAT, 1);
	/* XXX why putting 1 like renouveau dump, swap the text */
#if 1 || X_BYTE_ORDER == X_BIG_ENDIAN
	OUT_RING  (NV04_GDI_RECTANGLE_TEXT_MONOCHROME_FORMAT_LE);
#else
	OUT_RING  (NV04_GDI_RECTANGLE_TEXT_MONOCHROME_FORMAT_CGA6);
#endif

	return TRUE;
}

static Bool
NVAccelInitImageBlit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	class = (pNv->WaitVSyncPossible) ? NV12_IMAGE_BLIT : NV_IMAGE_BLIT;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvImageBlit, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_DMA_NOTIFY, 1);
	OUT_RING  (NvDmaNotifier0);
	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_COLOR_KEY, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_SURFACE, 1);
	OUT_RING  (NvContextSurfaces);
	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_CLIP_RECTANGLE, 3);
	OUT_RING  (NvNullObject);
	OUT_RING  (NvImagePattern);
	OUT_RING  (NvRop);
	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_OPERATION, 1);
	OUT_RING  (NV_IMAGE_BLIT_OPERATION_ROP_AND);

	if (pNv->WaitVSyncPossible) {
		BEGIN_RING(NvImageBlit, 0x0120, 3);
		OUT_RING  (0);
		OUT_RING  (1);
		OUT_RING  (2);
	}

	return TRUE;
}

static Bool
NVAccelInitScaledImage(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	switch (pNv->Architecture) {
	case NV_ARCH_04:
		class = NV04_SCALED_IMAGE_FROM_MEMORY;
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
		class = NV10_SCALED_IMAGE_FROM_MEMORY;
		break;
	case NV_ARCH_40:
	default:
		class = NV10_SCALED_IMAGE_FROM_MEMORY | 0x3000;
		break;
	}

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvScaledImage, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvScaledImage,
			NV04_SCALED_IMAGE_FROM_MEMORY_DMA_NOTIFY, 7);
	OUT_RING  (NvDmaNotifier0);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvNullObject);
	OUT_RING  (NvNullObject);
	OUT_RING  (NvContextBeta1);
	OUT_RING  (NvContextBeta4);
	OUT_RING  (NvContextSurfaces);
	if (pNv->Architecture>=NV_ARCH_10) {
	BEGIN_RING(NvScaledImage,
		   NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_CONVERSION, 1);
	OUT_RING  (NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_CONVERSION_DITHER);
	}
	BEGIN_RING(NvScaledImage, NV04_SCALED_IMAGE_FROM_MEMORY_OPERATION, 1);
	OUT_RING  (NV04_SCALED_IMAGE_FROM_MEMORY_OPERATION_SRCCOPY);

	return TRUE;
}

static Bool
NVAccelInitClipRectangle(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	int class = NV01_CONTEXT_CLIP_RECTANGLE;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvClipRectangle, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvClipRectangle, NV01_CONTEXT_CLIP_RECTANGLE_DMA_NOTIFY, 1);
	OUT_RING  (NvNullObject);

	return TRUE;
}

static Bool
NVAccelInitSolidLine(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	int class = NV04_RENDER_SOLID_LINE;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvSolidLine, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvSolidLine, NV01_RENDER_SOLID_LINE_CLIP_RECTANGLE, 3);
	OUT_RING  (NvClipRectangle);
	OUT_RING  (NvImagePattern);
	OUT_RING  (NvRop);
	BEGIN_RING(NvSolidLine, NV04_RENDER_SOLID_LINE_SURFACE, 1);
	OUT_RING  (NvContextSurfaces);
	BEGIN_RING(NvSolidLine, NV01_RENDER_SOLID_LINE_OPERATION, 1);
	OUT_RING  (NV01_RENDER_SOLID_LINE_OPERATION_ROP_AND);

	return TRUE;
}

/* FLAGS_NONE, NvDmaFB, NvDmaAGP, NvDmaNotifier0 */
static Bool
NVAccelInitMemFormat(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	if (pNv->Architecture < NV_ARCH_50)
		class = NV_MEMORY_TO_MEMORY_FORMAT;
	else
		class = NV50_MEMORY_TO_MEMORY_FORMAT;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvMemFormat, class))
				return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvMemFormat, NV_MEMORY_TO_MEMORY_FORMAT_DMA_NOTIFY, 1);
	OUT_RING  (NvDmaNotifier0);
	BEGIN_RING(NvMemFormat, NV_MEMORY_TO_MEMORY_FORMAT_DMA_BUFFER_IN, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);

	pNv->M2MFDirection = -1;
	return TRUE;
}

static Bool
NVAccelInitImageFromCpu(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t   class;

	switch (pNv->Architecture) {
	case NV_ARCH_04:
		class = NV05_IMAGE_FROM_CPU;
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
	case NV_ARCH_40:
	default:
		class = NV10_IMAGE_FROM_CPU;
		break;
	}

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, NvImageFromCpu, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_DMA_NOTIFY, 1);
	OUT_RING  (NvDmaNotifier0);
	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_CLIP_RECTANGLE, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_PATTERN, 1);
	OUT_RING  (NvNullObject);
	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_ROP, 1);
	OUT_RING  (NvNullObject);
	if (pNv->Architecture >= NV_ARCH_10)
	{
		BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_BETA1, 1);
		OUT_RING  (NvNullObject);
		BEGIN_RING(NvImageFromCpu, NV05_IMAGE_FROM_CPU_BETA4, 1);
		OUT_RING  (NvNullObject);
	}
	BEGIN_RING(NvImageFromCpu, NV05_IMAGE_FROM_CPU_SURFACE, 1);
	OUT_RING  (NvContextSurfaces);
	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_OPERATION, 1);
	OUT_RING  (NV01_IMAGE_FROM_CPU_OPERATION_SRCCOPY);
	return TRUE;
}

static Bool
NVAccelInit2D_NV50(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, Nv2D, 0x502d))
				return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(Nv2D, 0x180, 3);
	OUT_RING  (NvDmaNotifier0);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);

	/* Magics from nv, no clue what they do, but at least some
	 * of them are needed to avoid crashes.
	 */
	BEGIN_RING(Nv2D, 0x260, 1);
	OUT_RING  (1);
	BEGIN_RING(Nv2D, 0x290, 1);
	OUT_RING  (1);
	BEGIN_RING(Nv2D, 0x29c, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv2D, 0x58c, 1);
	OUT_RING  (0x111);

	pNv->currentRop = 0xfffffffa;
	return TRUE;
}

#define INIT_CONTEXT_OBJECT(name) do {                                        \
	ret = NVAccelInit##name(pScrn);                                       \
	if (!ret) {                                                           \
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,                         \
			   "Failed to initialise context object: "#name       \
			   " (%d)\n", ret);                                   \
		return FALSE;                                                 \
	}                                                                     \
} while(0)

Bool
NVAccelCommonInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	Bool ret;
	if(pNv->NoAccel) return TRUE;

	/* General engine objects */
	INIT_CONTEXT_OBJECT(NullObject);
	INIT_CONTEXT_OBJECT(DmaNotifier0);

	/* 2D engine */
	if (pNv->Architecture < NV_ARCH_50) {
		INIT_CONTEXT_OBJECT(ContextSurfaces);
		INIT_CONTEXT_OBJECT(ContextBeta1);
		INIT_CONTEXT_OBJECT(ContextBeta4);
		INIT_CONTEXT_OBJECT(ImagePattern);
		INIT_CONTEXT_OBJECT(RasterOp);
		INIT_CONTEXT_OBJECT(Rectangle);
		INIT_CONTEXT_OBJECT(ImageBlit);
		INIT_CONTEXT_OBJECT(ScaledImage);
		INIT_CONTEXT_OBJECT(ClipRectangle);
		INIT_CONTEXT_OBJECT(SolidLine);
		INIT_CONTEXT_OBJECT(ImageFromCpu);
	} else {
		INIT_CONTEXT_OBJECT(2D_NV50);
	}
	INIT_CONTEXT_OBJECT(MemFormat);

	/* 3D init */
	switch (pNv->Architecture) {
	case NV_ARCH_40:
		INIT_CONTEXT_OBJECT(NV40TCL);
		break;
	case NV_ARCH_30:
		INIT_CONTEXT_OBJECT(NV30TCL);
		break;
	case NV_ARCH_20:
	case NV_ARCH_10:
		INIT_CONTEXT_OBJECT(NV10TCL);
		break;
	default:
		break;
	}

	return TRUE;
}

