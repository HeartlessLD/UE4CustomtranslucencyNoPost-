#include "NoPostProcessCustomStencil.h"
#include "ShaderCompilerCore.h"
#include "PixelShaderUtils.h"
#include "PostProcessing.h"

namespace 
{
	class FNoPostProcessPassPS: public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FNoPostProcessPassPS);
		SHADER_USE_PARAMETER_STRUCT(FNoPostProcessPassPS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4, SeparateTranslucencyBilinearUVMinMax)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState, SceneColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SeparateTranslucency)
		SHADER_PARAMETER_SAMPLER(SamplerState, SeparateTranslucencySampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SeparateModulation)
		SHADER_PARAMETER_SAMPLER(SamplerState, SeparateModulationSampler)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_SRV(Texture2D<uint2>, CustomStencilTexture)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FNoPostProcessPassPS, "/Engine/Private/NoPostProcessCustomStencil.usf", "MainPS", SF_Pixel);
}

FScreenPassTexture AddNoPostProcessPass(FRDGBuilder& GraphBuilder, const FViewInfo& View,  const FNoPostProcessInputs& Inputs, FRDGTextureRef SceneColor, const FSeparateTranslucencyTextures& SeparateTranslucencyTextures, FRHIShaderResourceView* CustomStencil)
{
	check(Inputs.SceneColor.IsValid());
	RDG_EVENT_SCOPE(GraphBuilder, "NoPostProcessPass");

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if(!Output.IsValid())
	{
		FRDGTextureDesc OutputTextureDesc = Inputs.SceneColor.Texture->Desc;
		OutputTextureDesc.Flags |= TexCreate_DisableDCC;
		OutputTextureDesc.Reset();
		OutputTextureDesc.ClearValue = FClearValueBinding(FLinearColor::Transparent);

		Output.Texture = GraphBuilder.CreateTexture(OutputTextureDesc, TEXT("MyTestTexture"));
		//Output.ViewRect = Inputs.SceneColor.ViewRect;
		Output.LoadAction = ERenderTargetLoadAction::EClear;
	}

	const FScreenPassTextureViewport Viewport(Inputs.SceneColor);
	const FSceneViewFamily& ViewFamily = *(View.Family);

	/*FRDGTextureDesc SceneColorDesc = SceneColor->Desc;
	SceneColorDesc.Reset();*/
	FRDGTextureRef SeparateTranslucency = SeparateTranslucencyTextures.GetColorForRead(GraphBuilder);
	
	const FIntRect SeparateTranslucencyRect = SeparateTranslucencyTextures.GetDimensions().GetViewport(View.ViewRect).Rect;
	const bool bScaleSeparateTranslucency = SeparateTranslucencyRect != View.ViewRect;
	FNoPostProcessPassPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNoPostProcessPassPS::FParameters>();
	PassParameters->SeparateTranslucencyBilinearUVMinMax.X = (SeparateTranslucencyRect.Min.X + 0.5f) / float(SeparateTranslucency->Desc.Extent.X);
	PassParameters->SeparateTranslucencyBilinearUVMinMax.Y = (SeparateTranslucencyRect.Min.Y + 0.5f) / float(SeparateTranslucency->Desc.Extent.Y);
	PassParameters->SeparateTranslucencyBilinearUVMinMax.Z = (SeparateTranslucencyRect.Max.X - 0.5f) / float(SeparateTranslucency->Desc.Extent.X);
	PassParameters->SeparateTranslucencyBilinearUVMinMax.W = (SeparateTranslucencyRect.Max.Y - 0.5f) / float(SeparateTranslucency->Desc.Extent.Y);
	PassParameters->SceneColor = Inputs.SceneColor.Texture;
	PassParameters->SceneColorSampler = TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->SeparateTranslucency = SeparateTranslucency;
	PassParameters->SeparateTranslucencySampler = bScaleSeparateTranslucency ? TStaticSamplerState<SF_Bilinear>::GetRHI() : TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->SeparateModulation = SeparateTranslucencyTextures.GetColorModulateForRead(GraphBuilder);
	PassParameters->SeparateModulationSampler = bScaleSeparateTranslucency ? TStaticSamplerState<SF_Bilinear>::GetRHI() : TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CustomStencilTexture = CustomStencil;
	
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

	TShaderMapRef<FNoPostProcessPassPS> PixelShader(View.ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("NoPostProcessPass %dx%d", Viewport.Rect.Width(), Viewport.Rect.Height()),
		PixelShader,
		PassParameters,
		Viewport.Rect
		);

	return MoveTemp(Output);
	
}
