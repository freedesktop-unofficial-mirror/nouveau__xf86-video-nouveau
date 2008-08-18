/*
 * Copyright 2007 Nouveau Project
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "nouveau_drmif.h"

int
nouveau_channel_alloc(struct nouveau_device *dev, uint32_t fb_ctxdma,
		      uint32_t tt_ctxdma, struct nouveau_channel **chan)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_channel_priv *nvchan;
	int ret;

	if (!nvdev || !chan || *chan)
	    return -EINVAL;

	nvchan = calloc(1, sizeof(struct nouveau_channel_priv));
	if (!nvchan)
		return -ENOMEM;
	nvchan->base.device = dev;

	nvchan->drm.fb_ctxdma_handle = fb_ctxdma;
	nvchan->drm.tt_ctxdma_handle = tt_ctxdma;
	ret = drmCommandWriteRead(nvdev->fd, DRM_NOUVEAU_CHANNEL_ALLOC,
				  &nvchan->drm, sizeof(nvchan->drm));
	if (ret) {
		free(nvchan);
		return ret;
	}

	nvchan->base.id = nvchan->drm.channel;
	if (nouveau_grobj_ref(&nvchan->base, nvchan->drm.fb_ctxdma_handle,
			      &nvchan->base.vram) ||
	    nouveau_grobj_ref(&nvchan->base, nvchan->drm.tt_ctxdma_handle,
		    	      &nvchan->base.gart)) {
		nouveau_channel_free((void *)&nvchan);
		return -EINVAL;
	}

	ret = drmMap(nvdev->fd, nvchan->drm.notifier, nvchan->drm.notifier_size,
		     (drmAddressPtr)&nvchan->notifier_block);
	if (ret) {
		nouveau_channel_free((void *)&nvchan);
		return ret;
	}

	ret = nouveau_grobj_alloc(&nvchan->base, 0x00000000, 0x0030,
				  &nvchan->base.nullobj);
	if (ret) {
		nouveau_channel_free((void *)&nvchan);
		return ret;
	}

	nouveau_pushbuf_init(&nvchan->base);

	if (1) { //dev->chipset < 0x10) {
		unsigned m2mf;

		switch (dev->chipset & 0xf0) {
		case 0x50:
		case 0x80:
		case 0x90:
			m2mf = 0x5039;
			break;
		default:
			m2mf = 0x0039;
			break;
		}

		ret = nouveau_grobj_alloc(&nvchan->base, 0xbeef3901, m2mf,
					  &nvchan->fence_grobj);
		if (ret) {
			nouveau_channel_free((void *)&nvchan);
			return ret;
		}

		ret = nouveau_notifier_alloc(&nvchan->base, 0xbeef3902, 1,
					     &nvchan->fence_ntfy);
		if (ret) {
			nouveau_channel_free((void *)&nvchan);
			return ret;
		}

		BIND_RING (&nvchan->base, nvchan->fence_grobj, 0);
		BEGIN_RING(&nvchan->base, nvchan->fence_grobj, 0x0180, 1);
		OUT_RING  (&nvchan->base, nvchan->fence_ntfy->handle);
	}

	*chan = &nvchan->base;
	return 0;
}

void
nouveau_channel_free(struct nouveau_channel **chan)
{
	struct nouveau_channel_priv *nvchan;
	struct nouveau_device_priv *nvdev;
	struct drm_nouveau_channel_free cf;

	if (!chan || !*chan)
		return;
	nvchan = nouveau_channel(*chan);
	*chan = NULL;
	nvdev = nouveau_device(nvchan->base.device);
	
	FIRE_RING(&nvchan->base);

	nouveau_grobj_free(&nvchan->base.vram);
	nouveau_grobj_free(&nvchan->base.gart);
	nouveau_grobj_free(&nvchan->base.nullobj);

	cf.channel = nvchan->drm.channel;
	drmCommandWrite(nvdev->fd, DRM_NOUVEAU_CHANNEL_FREE, &cf, sizeof(cf));
	free(nvchan);
}


