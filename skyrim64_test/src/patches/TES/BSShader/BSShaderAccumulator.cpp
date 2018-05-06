#include "../../rendering/common.h"
#include "../../../common.h"
#include "../BSGraphicsRenderer.h"
#include "../BSBatchRenderer.h"
#include "BSShaderManager.h"
#include "BSShaderAccumulator.h"
#include "../BSReadWriteLock.h"
#include "../MTRenderer.h"

extern ID3DUserDefinedAnnotation *annotation;

void BSShaderAccumulator::InitCallbackTable()
{
	// If the pointer is null, it defaults to the function at index 0
	RegisterObjectArray[0] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 00 RegisterObject_Normal
	RegisterObjectArray[1] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 01 RegisterObject_Normal
	RegisterObjectArray[2] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 02 RegisterObject_Normal
	RegisterObjectArray[3] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 03 RegisterObject_Normal
	RegisterObjectArray[4] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 04 RegisterObject_Normal
	RegisterObjectArray[5] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 05 RegisterObject_Normal
	RegisterObjectArray[6] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 06 RegisterObject_Normal
	RegisterObjectArray[7] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 07 RegisterObject_Normal
	RegisterObjectArray[8] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 08 RegisterObject_Normal
	RegisterObjectArray[9] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 09 RegisterObject_Normal
	RegisterObjectArray[10] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 10 RegisterObject_Normal
	RegisterObjectArray[11] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 11 RegisterObject_Normal
	RegisterObjectArray[12] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 12 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[13] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 13 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[14] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 14 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[15] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 15 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[16] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 16 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[17] = (RegisterObjFunc)(g_ModuleBase + 0x12E1650);	// 17 RegisterObject_ShadowMapOrMask
	RegisterObjectArray[18] = (RegisterObjFunc)(g_ModuleBase + 0x12E1400);	// 18 ?
	RegisterObjectArray[19] = (RegisterObjFunc)(g_ModuleBase + 0x12E1400);	// 19 ?
	RegisterObjectArray[20] = (RegisterObjFunc)(g_ModuleBase + 0x12E1340);	// 20 RegisterObject_Interface
	RegisterObjectArray[21] = (RegisterObjFunc)(g_ModuleBase + 0x12E1370);	// 21 ?
	RegisterObjectArray[22] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 22 RegisterObject_Normal
	RegisterObjectArray[23] = (RegisterObjFunc)(g_ModuleBase + 0x12E1380);	// 23 ?
	RegisterObjectArray[24] = (RegisterObjFunc)(g_ModuleBase + 0x12E15E0);	// 24 ?
	RegisterObjectArray[25] = (RegisterObjFunc)(g_ModuleBase + 0x12E1780);	// 25 RegisterObject_LOD
	RegisterObjectArray[26] = (RegisterObjFunc)(g_ModuleBase + 0x12E1780);	// 26 RegisterObject_LOD
	RegisterObjectArray[27] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 27 RegisterObject_Normal
	RegisterObjectArray[28] = (RegisterObjFunc)(g_ModuleBase + 0x12E1800);	// 28 ?
	RegisterObjectArray[29] = (RegisterObjFunc)(g_ModuleBase + 0x12E0F90);	// 29 RegisterObject_Normal

	// If the pointer is null, it defaults to the function at index 0
	FinishAccumulatingArray[0] = FinishAccumulating_Normal;						// 00 FinishAccumulating_Normal
	FinishAccumulatingArray[1] = nullptr;										// 01
	FinishAccumulatingArray[2] = nullptr;										// 02
	FinishAccumulatingArray[3] = nullptr;										// 03
	FinishAccumulatingArray[4] = nullptr;										// 04
	FinishAccumulatingArray[5] = nullptr;										// 05
	FinishAccumulatingArray[6] = nullptr;										// 06
	FinishAccumulatingArray[7] = nullptr;										// 07
	FinishAccumulatingArray[8] = nullptr;										// 08
	FinishAccumulatingArray[9] = nullptr;										// 09
	FinishAccumulatingArray[10] = nullptr;										// 10
	FinishAccumulatingArray[11] = nullptr;										// 11
	FinishAccumulatingArray[12] = FinishAccumulating_ShadowMapOrMask;			// 12 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[13] = FinishAccumulating_ShadowMapOrMask;			// 13 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[14] = FinishAccumulating_ShadowMapOrMask;			// 14 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[15] = FinishAccumulating_ShadowMapOrMask;			// 15 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[16] = FinishAccumulating_ShadowMapOrMask;			// 16 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[17] = FinishAccumulating_ShadowMapOrMask;			// 17 FinishAccumulating_ShadowMapOrMask
	FinishAccumulatingArray[18] = nullptr;										// 18
	FinishAccumulatingArray[19] = nullptr;										// 19
	FinishAccumulatingArray[20] = FinishAccumulating_InterfaceElements;			// 20 FinishAccumulating_InterfaceElements
	FinishAccumulatingArray[21] = nullptr;										// 21
	FinishAccumulatingArray[22] = FinishAccumulating_FirstPerson;				// 22 FinishAccumulating_FirstPerson
	FinishAccumulatingArray[23] = (FinishAccumFunc)(g_ModuleBase + 0x12E3020);	// 23 ?
	FinishAccumulatingArray[24] = (FinishAccumFunc)(g_ModuleBase + 0x12E3030);	// 24 ?
	FinishAccumulatingArray[25] = FinishAccumulating_LODOnly;					// 25 FinishAccumulating_LODOnly
	FinishAccumulatingArray[26] = FinishAccumulating_LODOnly;					// 26 FinishAccumulating_LODOnly
	FinishAccumulatingArray[27] = nullptr;										// 27
	FinishAccumulatingArray[28] = FinishAccumulating_Unknown1;					// 28 Never used? BSSM_RENDER_PRECIPITATION_OCCLUSION_MAP?
	FinishAccumulatingArray[29] = nullptr;										// 29

	SetRenderMode(0);
}

void BSShaderAccumulator::SetRenderMode(uint32_t RenderMode)
{
	RegisterObjectCurrent = RegisterObjectArray[RenderMode];
	FinishAccumulatingCurrent = FinishAccumulatingArray[RenderMode];

	if (!RegisterObjectCurrent)
		RegisterObjectCurrent = RegisterObjectArray[0];

	if (!FinishAccumulatingCurrent)
		FinishAccumulatingCurrent = FinishAccumulatingArray[0];
}

void BSShaderAccumulator::hk_FinishAccumulatingDispatch(uint32_t RenderFlags)
{
	uint32_t renderMode = *(uint32_t *)((__int64)this + 0x150);

	SetRenderMode(renderMode);

	if (renderMode != 0)
		FinishAccumulatingCurrent(this, RenderFlags);
	else
		RenderSceneNormal(this, RenderFlags);
}

void BSShaderAccumulator::FinishAccumulating_Normal(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_Normal");
	RenderSceneNormal(Accumulator, RenderFlags);
	RenderSceneNormalAlphaZ(Accumulator, RenderFlags);
	annotation->EndEvent();
}

void BSShaderAccumulator::RenderSceneNormal(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"BSShaderAccumulator: Draw1");

	__int64 a1 = (__int64)Accumulator;
	auto renderer = BSGraphics::Renderer::GetGlobals();

	if (!Accumulator->m_pkCamera)
		return;

	if (*(BYTE *)(a1 + 92) && !*(BYTE*)(g_ModuleBase + 0x30528E5))
		renderer->DepthStencilStateSetDepthMode(4);

	// v7 = RenderDepthOnly()? RenderAlphaOnly()?
	bool v7 = (RenderFlags & 0xA) != 0;

	if (!v7)
		((void(__fastcall *)())(g_ModuleBase + 0x12F8B10))();

	//
	// Original draw order:
	//
	// RenderBatches
	// LowAniso
	// RenderGrass
	// RenderNoShadowGroup
	// RenderLODObjects
	// RenderLODLand
	// RenderSky
	// RenderSkyClouds
	// ???
	// BlendedDecals
	// RenderWaterStencil
	//
	{
		ProfileTimer("RenderBatches");

		// RenderBatches
		annotation->BeginEvent(L"RenderBatches");
		{
			Accumulator->RenderFromMainGroup(1, BSSM_DISTANTTREE_DEPTH, RenderFlags, -1);
		}
		annotation->EndEvent();

		// LowAniso
		annotation->BeginEvent(L"LowAniso");
		{
			Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 9);
		}
		annotation->EndEvent();

		// RenderGrass
		annotation->BeginEvent(L"RenderGrass");
		{
			Accumulator->RenderFromMainGroup(BSSM_GRASS_DIRONLY_LF, 0x5C00005C, RenderFlags, -1);
		}
		annotation->EndEvent();

		// RenderNoShadowGroup
		annotation->BeginEvent(L"RenderNoShadowGroup");
		{
			Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 8);
		}
		annotation->EndEvent();

		// RenderLODObjects
		annotation->BeginEvent(L"RenderLODObjects");
		{
			Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 1);

			if (*(BYTE *)(a1 + 92) && !*(BYTE*)(g_ModuleBase + 0x30528E5))
				renderer->DepthStencilStateSetDepthMode(3);
		}
		annotation->EndEvent();

		// RenderLODLand
		annotation->BeginEvent(L"RenderLODLand");
		{
			ProfileTimer("LOD");

			Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 0);

			if (!v7)
				((void(__fastcall *)())(g_ModuleBase + 0x12F8C70))();
		}
		annotation->EndEvent();
	}

	// RenderSky
	annotation->BeginEvent(L"RenderSky");
	{
		renderer->SetUseAlphaTestRef(true);
		renderer->SetAlphaTestRef(128.0f / 255.0f);

		Accumulator->RenderFromMainGroup(BSSM_SKYBASEPRE, BSSM_SKY_CLOUDSFADE, RenderFlags, -1);
	}
	annotation->EndEvent();

	// RenderSkyClouds
	annotation->BeginEvent(L"RenderSkyClouds");
	{
		renderer->AlphaBlendStateSetUnknown2(11);

		Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 13);

		renderer->AlphaBlendStateSetUnknown2(1);
	}
	annotation->EndEvent();

	if (!v7)
		((void(__fastcall *)())(g_ModuleBase + 0x12F8B10))();

	// NormalDecals?...CK doesn't have a specific name for this
	{
		renderer->AlphaBlendStateSetUnknown2(10);
		((void(__fastcall *)(__int64 a1, unsigned int a2))(g_ModuleBase + 0x12E27B0))(a1, RenderFlags);
	}

	// BlendedDecals
	annotation->BeginEvent(L"BlendedDecals");
	{
		renderer->AlphaBlendStateSetUnknown2(11);
		((void(__fastcall *)(__int64 a1, unsigned int a2))(g_ModuleBase + 0x12E2950))(a1, RenderFlags);
	}
	annotation->EndEvent();

	renderer->AlphaBlendStateSetMode(0);
	renderer->AlphaBlendStateSetUnknown2(1);

	auto sub_140D744B0 = (int(__fastcall *)())(g_ModuleBase + 0xD744E0);
	auto sub_140D69E70 = (__int64(__fastcall *)(__int64 a1, unsigned int a2))(g_ModuleBase + 0xD69EA0);
	auto sub_140D69D30 = (__int64(__fastcall *)(float *a1, float a2, float a3, float a4, int a5))(g_ModuleBase + 0xD69D60);
	auto sub_1412FD120 = (signed __int64(__fastcall *)())(g_ModuleBase + 0x12FD480);
	auto sub_140D74350 = (__int64(__fastcall *)(__int64 a1, unsigned int a2, int a3, int a4, char a5))(g_ModuleBase + 0xD74380);
	auto sub_140D74370 = (void(__fastcall *)(__int64 a1, uint32_t a2, int a3, uint32_t a4))(g_ModuleBase + 0xD743A0);
	auto sub_140D69990 = (void(__fastcall *)(__int64 a1, char a2))(g_ModuleBase + 0xD699C0);
	auto sub_1412FADA0 = (__int64(__fastcall *)())(g_ModuleBase + 0x12FB100);
	auto sub_140D69DA0 = (void(__fastcall *)(DWORD *a1))(g_ModuleBase + 0xD69DD0);

	DWORD *flt_14304E490 = (DWORD *)(g_ModuleBase + 0x304E490);

	if ((RenderFlags & 0x80) != 0 && Accumulator->m_MainBatch->HasTechniquePasses(0x5C000071, 0x5C006071))
	{
		int aiSource = sub_140D744B0();

		//	bAssert(aiSource != aiTarget &&
		//		aiSource < DEPTH_STENCIL_COUNT &&
		//		aiTarget < DEPTH_STENCIL_COUNT &&
		//		aiSource != DEPTH_STENCIL_TARGET_NONE &&
		//		aiTarget != DEPTH_STENCIL_TARGET_NONE);

		renderer->m_DeviceContext->CopyResource(*(ID3D11Resource **)(g_ModuleBase + 0x3050870), *((ID3D11Resource **)flt_14304E490 + 19 * aiSource + 1015));
	}

	// RenderWaterStencil
	annotation->BeginEvent(L"RenderWaterStencil");
	{
		if (Accumulator->m_MainBatch->HasTechniquePasses(BSSM_WATER_STENCIL, BSSM_WATER_DISPLACEMENT_STENCIL_Vc))
		{
			sub_140D69E70((__int64)flt_14304E490, 2u);// BSGraphics::Renderer::ClearDepthStencil(CLEAR_DEPTH_STENCIL_TARGET_STENCIL)
			sub_140D69D30((float *)flt_14304E490, 0.0, 0.0, 0.0, 0);
			__int64 v20 = sub_1412FD120();
			sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 0, v20, 3, 1);// RENDER_TARGET_NORMAL_TAAMASK_SSRMASK (This can change) SRTM_NO_CLEAR
			sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 1u, 7, 3, 1);// RENDER_TARGET_MOTION_VECTOR SRTM_NO_CLEAR
			sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 2u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
			sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 3u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
			sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 4u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
			Accumulator->RenderFromMainGroup(BSSM_WATER_STENCIL, BSSM_WATER_DISPLACEMENT_STENCIL_Vc, RenderFlags, -1);
			sub_140D69DA0((DWORD *)flt_14304E490);
			*(DWORD *)(*(uint64_t *)((*(uint64_t*)(g_ModuleBase + 0x31F5810)) + 496) + 44i64) = 1;
		}
	}
	annotation->EndEvent();

	if (RenderFlags & 0x40)
	{
		sub_140D74370((__int64)(g_ModuleBase + 0x3051B20), 0xFFFFFFFF, 3, 0);
		sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 1u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
		sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 2u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
		sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 3u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
		sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 4u, -1, 3, 1);// RENDER_TARGET_NONE SRTM_NO_CLEAR
		sub_140D69990((__int64)flt_14304E490, 1);
		sub_1412FADA0();
		sub_140D74350((__int64)(g_ModuleBase + 0x3051B20), 0, 1, 3, 1);
		int v21 = sub_140D744B0();
		sub_140D74370((__int64)(g_ModuleBase + 0x3051B20), v21, 3, 0);

		renderer->DepthStencilStateSetDepthMode(1);
	}

	if (!v7)
		((void(__fastcall *)())(g_ModuleBase + 0x12F8C70))();

	annotation->EndEvent();
}

void BSShaderAccumulator::RenderSceneNormalAlphaZ(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	((FinishAccumFunc)(g_ModuleBase + 0x12E1F70))(Accumulator, RenderFlags);
}

void BSShaderAccumulator::FinishAccumulating_ShadowMapOrMask(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_ShadowMapOrMask");
	((FinishAccumFunc)(g_ModuleBase + 0x12E3050))(Accumulator, RenderFlags);
	annotation->EndEvent();
}

void BSShaderAccumulator::FinishAccumulating_InterfaceElements(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_InterfaceElements");
	((FinishAccumFunc)(g_ModuleBase + 0x12E2FE0))(Accumulator, RenderFlags);
	annotation->EndEvent();
}

void BSShaderAccumulator::FinishAccumulating_FirstPerson(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_FirstPerson");
	((FinishAccumFunc)(g_ModuleBase + 0x12E2B20))(Accumulator, RenderFlags);
	annotation->EndEvent();
}

void BSShaderAccumulator::FinishAccumulating_LODOnly(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_LODOnly");

	if (!Accumulator->m_pkCamera)
		return;

	*(bool *)(g_ModuleBase + 0x1E32E89) = false;
	Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 0);
	Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 1);
	*(bool *)(g_ModuleBase + 0x1E32E89) = true;

	annotation->EndEvent();
}

void BSShaderAccumulator::FinishAccumulating_Unknown1(BSShaderAccumulator *Accumulator, uint32_t RenderFlags)
{
	annotation->BeginEvent(L"FinishAccumulating_Unknown1");

	if (!Accumulator->m_pkCamera)
		return;

	Accumulator->RenderFromMainGroup(1, BSSM_BLOOD_SPLATTER, RenderFlags, 14);

	annotation->EndEvent();
}

void BSShaderAccumulator::RenderFromMainGroup(uint32_t StartTechnique, uint32_t EndTechnique, uint32_t RenderFlags, int GroupType)
{
	if (GroupType == -1)
	{
		RenderTechniques(StartTechnique, EndTechnique, RenderFlags, -1);
	}
	else
	{
		auto pass = m_MainBatch->m_Groups[GroupType];

		if (pass)
		{
			if (pass->UnkByte1 & 1)
				pass->Render(RenderFlags);
			else
				RenderTechniques(StartTechnique, EndTechnique, RenderFlags, GroupType);
		}
	}
}

void BSShaderAccumulator::RenderTechniques(uint32_t StartTechnique, uint32_t EndTechnique, uint32_t RenderFlags, int GroupType)
{
	Assert(GroupType >= -1 && GroupType < 16);
	Assert(StartTechnique <= EndTechnique);

	BSBatchRenderer::RenderGroup *group = nullptr;
	BSBatchRenderer *batch = nullptr;

	// Always run the full function because I'm not sure if the structure
	// is used somewhere important.
	MTRenderer::InsertCommand<MTRenderer::SetAccumulatorRenderCommand>(this);
	BSShaderManager::SetCurrentAccumulator(this);

	if (GroupType <= -1)
	{
		// Wildcard: render everything with the given techniques
		group = nullptr;
		batch = this->m_MainBatch;
	}
	else
	{
		// Render a single group with given techniques
		group = this->m_MainBatch->m_Groups[GroupType];
		batch = group->m_BatchRenderer;
	}

	m_CurrentTech = 0;

	if (batch)
	{
		auto *currentNode = &batch->m_UnknownList;

		batch->m_StartingTech = StartTechnique;
		batch->m_EndingTech = EndTechnique;

		m_CurrentGroupIndex = 0;
		m_HasPendingDraws = batch->sub_14131E700(m_CurrentTech, m_CurrentGroupIndex, (__int64)&currentNode);

		while (m_HasPendingDraws)
		{
			if ((unsigned int)(m_CurrentTech - BSSM_GRASS_SHADOW_L) <= 3 && (*(BYTE *)((__int64)this + 296) || *(BYTE *)((__int64)this + 297)))// if (is grass shadow) ???
				m_HasPendingDraws = batch->sub_14131ECE0(m_CurrentTech, m_CurrentGroupIndex, (__int64)&currentNode);// Probably discards pass, returns true if there's remaining sub passes
			else
				m_HasPendingDraws = batch->sub_14131E960(m_CurrentTech, m_CurrentGroupIndex, (__int64)&currentNode, RenderFlags);
		}
	}
	else
	{
		// No batcher available to render these, so just drop them
		m_HasPendingDraws = false;
	}

	if (group)
		group->Unregister();

	MTRenderer::ClearShaderAndTechnique();
}