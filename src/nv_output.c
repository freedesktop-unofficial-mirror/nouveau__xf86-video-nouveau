/*
 * Copyright 2006 Dave Airlie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *  Dave Airlie
 */
/*
 * this code uses ideas taken from the NVIDIA nv driver - the nvidia license
 * decleration is at the bottom of this file as it is rather ugly 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "os.h"
#include "mibank.h"
#include "globals.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86DDC.h"
#include "mipointer.h"
#include "windowstr.h"
#include <randrstr.h>
#include <X11/extensions/render.h>

#include "nv_xf86Crtc.h"
#include "nv_randr.h"
#include "nv_include.h"

#define NV_MAX_OUTPUT 2

const char *OutputType[] = {
    "None",
    "VGA",
    "DVI",
    "LVDS",
    "S-video",
    "Composite",
};

const char *MonTypeName[7] = {
  "AUTO",
  "NONE",
  "CRT",
  "LVDS",
  "TMDS",
  "CTV",
  "STV"
};

void NVWriteRAMDAC0(xf86OutputPtr output, CARD32 ramdac_reg, CARD32 val)
{
  NVOutputPrivatePtr nv_output = output->driver_private;
  ScrnInfoPtr	pScrn = output->scrn;
  NVPtr pNv = NVPTR(pScrn);

  NV_WR32(pNv->PRAMDAC0, ramdac_reg, val);
}

void NVWriteRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg, CARD32 val)
{
  NVOutputPrivatePtr nv_output = output->driver_private;
  NV_WR32(nv_output->pRAMDACReg, ramdac_reg, val);
}

CARD32 NVReadRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg)
{
  NVOutputPrivatePtr nv_output = output->driver_private;
  return NV_RD32(nv_output->pRAMDACReg, ramdac_reg);
}

static void
nv_output_dpms(xf86OutputPtr output, int mode)
{


}

static void
nv_output_save (xf86OutputPtr output)
{
    
}

static void
nv_output_restore (xf86OutputPtr output)
{


}

static int
nv_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    if (pMode->Flags & V_DBLSCAN)
	return MODE_NO_DBLESCAN;

    if (pMode->Clock > 400000 || pMode->Clock < 25000)
	return MODE_CLOCK_RANGE;

    return MODE_OK;
}


static Bool
nv_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		    DisplayModePtr adjusted_mode)
{
    return TRUE;
}

static void
nv_output_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		  DisplayModePtr adjusted_mode)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);
    RIVA_HW_STATE *state;

    state = &pNv->ModeReg;
    if(nv_output->mon_type == MT_CRT) {
	NVWriteRAMDAC0(output, NV_RAMDAC_PLL_SELECT, state->pllsel);
	NVWriteRAMDAC0(output, NV_RAMDAC_VPLL, state->vpll);
	if(pNv->twoHeads)
	    NVWriteRAMDAC0(output, NV_RAMDAC_VPLL2, state->vpll2);
	if(pNv->twoStagePLL) {
	    NVWriteRAMDAC0(output, NV_RAMDAC_VPLL_B, state->vpllB);
	    NVWriteRAMDAC0(output, NV_RAMDAC_VPLL2_B, state->vpll2B);
	}
    } else {
	NVWriteRAMDAC(output, NV_RAMDAC_FP_CONTROL, state->scale);
	NVWriteRAMDAC(output, NV_RAMDAC_FP_HCRTC, state->crtcSync);

	if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
	    NVWriteRAMDAC(output, NV_RAMDAC_DITHER_NV11, state->dither);
	} else if(pNv->twoHeads) {
	    NVWriteRAMDAC(output, NV_RAMDAC_FP_DITHER, state->dither);
	}
    }
    NVWriteRAMDAC(output, NV_RAMDAC_GENERAL_CONTROL, state->general);

}

static Bool
nv_ddc_detect(xf86OutputPtr output)
{
  NVOutputPrivatePtr nv_output = output->driver_private;
  
  return xf86I2CProbeAddress(nv_output->pDDCBus, 0x00A0);
}

static Bool
nv_crt_load_detect(xf86OutputPtr output)
{
  NVOutputPrivatePtr nv_output = output->driver_private;
  CARD32 reg52C, reg608, temp;
  int present = FALSE;
  
  reg52C = NVReadRAMDAC(output, NV_RAMDAC_052C);
  reg608 = NVReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);

  NVWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, (reg608 & ~0x00010000));
  
  NVWriteRAMDAC(output, NV_RAMDAC_052C, (reg52C & 0x0000FEEE));
  usleep(1000);
  
  temp = NVReadRAMDAC(output, NV_RAMDAC_052C);
  NVWriteRAMDAC(output, NV_RAMDAC_052C, temp | 1);

  NVWriteRAMDAC(output, NV_RAMDAC_TEST_DATA, 0x94050140);
  temp = NVReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
  NVWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, temp | 0x1000);

  usleep(1000);
  
  present = (NVReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL) & (1 << 28)) ? TRUE : FALSE;
  
  temp = NVReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
  NVWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, temp & 0x000EFFF);
  
  NVWriteRAMDAC(output, NV_RAMDAC_052C, reg52C);
  NVWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, reg608);
  
  return present;

}

static xf86OutputStatus
nv_output_detect(xf86OutputPtr output)
{
  NVOutputPrivatePtr nv_output = output->driver_private;

  if (nv_output->type == OUTPUT_LVDS)
    return XF86OutputStatusUnknown;

  if (nv_output->type == OUTPUT_DVI) {
    if (nv_ddc_detect(output))
      return XF86OutputStatusConnected;

    if (nv_crt_load_detect(output))
      return XF86OutputStatusConnected;

    return XF86OutputStatusDisconnected;
  }
  return XF86OutputStatusUnknown;
}

static DisplayModePtr
nv_output_get_modes(xf86OutputPtr output)
{
  ScrnInfoPtr	pScrn = output->scrn;
  NVOutputPrivatePtr nv_output = output->driver_private;
  xf86MonPtr ddc_mon;
  DisplayModePtr ddc_modes, mode;
  int i;


  ddc_mon = xf86DoEDID_DDC2(pScrn->scrnIndex, nv_output->pDDCBus);
  if (ddc_mon == NULL) {
#ifdef RANDR_12_INTERFACE
    nv_ddc_set_edid_property(output, NULL, 0);
#endif
    return NULL;
  }
  
  if (output->MonInfo != NULL)
    xfree(output->MonInfo);
  output->MonInfo = ddc_mon;

  /* check if a CRT or DFP */
  if (ddc_mon->features.input_type)
    nv_output->mon_type = MT_LCD;
  else
    nv_output->mon_type = MT_CRT;

#ifdef RANDR_12_INTERFACE
  if (output->MonInfo->ver.version == 1) {
    nv_ddc_set_edid_property(output, ddc_mon->rawData, 128);
    } else if (output->MonInfo->ver.version == 2) {
	nv_ddc_set_edid_property(output, ddc_mon->rawData, 256);
    } else {
	nv_ddc_set_edid_property(output, NULL, 0);
    }
#endif

  /* Debug info for now, at least */
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %s\n", output->name);
  xf86PrintEDID(output->MonInfo);
  
  ddc_modes = xf86DDCGetModes(pScrn->scrnIndex, ddc_mon);
  
  /* Strip out any modes that can't be supported on this output. */
  for (mode = ddc_modes; mode != NULL; mode = mode->next) {
    int status = (*output->funcs->mode_valid)(output, mode);
    
    if (status != MODE_OK)
      mode->status = status;
  }
  i830xf86PruneInvalidModes(pScrn, &ddc_modes, TRUE);
  
  /* Pull out a phyiscal size from a detailed timing if available. */
  for (i = 0; i < 4; i++) {
    if (ddc_mon->det_mon[i].type == DT &&
	ddc_mon->det_mon[i].section.d_timings.h_size != 0 &&
	ddc_mon->det_mon[i].section.d_timings.v_size != 0)
      {
	output->mm_width = ddc_mon->det_mon[i].section.d_timings.h_size;
	output->mm_height = ddc_mon->det_mon[i].section.d_timings.v_size;
	break;
      }
  }
  
  return ddc_modes;

}

static void
nv_output_destroy (xf86OutputPtr output)
{
    if (output->driver_private)
      xfree (output->driver_private);

}

static const xf86OutputFuncsRec nv_output_funcs = {
    .dpms = nv_output_dpms,
    .save = nv_output_save,
    .restore = nv_output_restore,
    .mode_valid = nv_output_mode_valid,
    .mode_fixup = nv_output_mode_fixup,
    .mode_set = nv_output_mode_set,
    .detect = nv_output_detect,
    .get_modes = nv_output_get_modes,
    .destroy = nv_output_destroy
};

/**
 * Set up the outputs according to what type of chip we are.
 *
 * Some outputs may not initialize, due to allocation failure or because a
 * controller chip isn't found.
 */
void NvSetupOutputs(ScrnInfoPtr pScrn)
{
  int i;
  NVPtr pNv = NVPTR(pScrn);
  xf86OutputPtr	    output;
  NVOutputPrivatePtr    nv_output;
  char *ddc_name[2] =  { "DDC0", "DDC1" };
  int   crtc_mask = (1<<0) | (1<<1);
  int output_type = OUTPUT_DVI;
  int num_outputs = pNv->twoHeads ? 2 : 1;

  pNv->Television = FALSE;

  /* work out outputs and type of outputs here */
  for (i = 0; i<num_outputs; i++) {
    output = xf86OutputCreate (pScrn, &nv_output_funcs, OutputType[output_type]);
    if (!output)
	return;
    nv_output = xnfcalloc (sizeof (NVOutputPrivateRec), 1);
    if (!nv_output)
    {
      xf86OutputDestroy (output);
      return;
    }
    
    output->driver_private = nv_output;
    nv_output->type = output_type;
    nv_output->ramdac = i;
    if (i == 0)
      nv_output->pRAMDACReg = pNv->PRAMDAC0;
    else
      nv_output->pRAMDACReg = pNv->PRAMDAC1;

    NV_I2CInit(pScrn, &nv_output->pDDCBus, i ? 0x36 : 0x3e, ddc_name[i]);
    
    output->possible_crtcs = crtc_mask;
  }

  if (pNv->Mobile) {
    output = xf86OutputCreate(pScrn, &nv_output_funcs, OutputType[OUTPUT_LVDS]);
    if (!output)
      return;

    nv_output = xnfcalloc(sizeof(NVOutputPrivateRec), 1);
    if (!nv_output) {
      xf86OutputDestroy(output);
      return;
    }

    output->driver_private = nv_output;
    nv_output->type = output_type;

    output->possible_crtcs = crtc_mask;
  }
}


/*************************************************************************** \
|*                                                                           *|
|*       Copyright 1993-2003 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/
