#pragma once

#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterial.h"
#include "PostProcessing.h"

struct FNoPostProcessInputs
{
	FScreenPassRenderTarget OverrideOutput;
	FScreenPassTexture SceneColor;
	FScreenPassTexture SeparateTransluscency;
};

FScreenPassTexture AddNoPostProcessPass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FNoPostProcessInputs& Inputs,
	FRDGTextureRef SceneColor,
	const FSeparateTranslucencyTextures& SeparateTranslucencyTextures,
	FRHIShaderResourceView* CustomStencil
);