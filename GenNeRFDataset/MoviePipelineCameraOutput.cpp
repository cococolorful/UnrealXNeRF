#include "MoviePipelineCameraOutput.h"
#include "MoviePipeline.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
void UMoviePipelineCameraOutput::OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame)
{
	
}

void UMoviePipelineCameraOutput::FinalizeImpl()
{
	
	
	const UMoviePipelineOutputSetting* OutputSettings = GetPipeline()->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineOutputSetting>();
	if (!OutputSettings)
	{
		UE_LOG(LogTemp,Display,TEXT("Counld not find OutputSettings \n"));
		return ;
	}

	EImageFormat ImageFormat{EImageFormat::JPEG};
	UMoviePipelineImageSequenceOutputBase* ImageSequenceOutputBase = GetPipeline()->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineImageSequenceOutput_JPG>();

	if (!ImageSequenceOutputBase)
	{
		ImageSequenceOutputBase = GetPipeline()->GetPipelinePrimaryConfig()->FindSetting<UMoviePipelineImageSequenceOutput_PNG>();

		if(!ImageSequenceOutputBase)
		{
			UE_LOG(LogTemp,Display,TEXT("Could not find JPG or PNG OutputSetttings\n"));
			return;
		}
		ImageFormat = EImageFormat::PNG;
	}

	if(CurrentCineCamera ==nullptr)
	{
		UE_LOG(LogTemp,Display,TEXT("No Camera in the end \n"));
		return;
	}

	FIntPoint OutputResolution = OutputSettings->OutputResolution;
	const float FocalInMM = CurrentCineCamera->GetCineCameraComponent()->CurrentFocalLength; // focal length in mm
	const float WidthResInPx = OutputResolution.X;
	const float HeightResInPx = OutputResolution.Y;
	const float OpticalCenterX = WidthResInPx / 2;
	const float OpticalCenterY = HeightResInPx / 2;
	const float sensor_width_size_in_mm = CurrentCineCamera->GetCineCameraComponent()->Filmback.SensorWidth;
	const float sensor_height_size_in_mm = CurrentCineCamera->GetCineCameraComponent()->Filmback.SensorHeight;
	
	const float Fx = FocalInMM / sensor_width_size_in_mm * WidthResInPx;
	const float Fy = FocalInMM / sensor_height_size_in_mm * HeightResInPx;
	
	// 声明一个存储Json内容的字符串
	FString JsonStr;

	// 创建一个Json编写器
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonStr);

	// 声明Json对象写入开始
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("fl_x"),Fx);
	JsonWriter->WriteValue(TEXT("fl_y"),Fy);
	JsonWriter->WriteValue(TEXT("cx"),OpticalCenterX);
	JsonWriter->WriteValue(TEXT("cy"),OpticalCenterY);
	JsonWriter->WriteValue(TEXT("w"),WidthResInPx);
	JsonWriter->WriteValue(TEXT("h"),HeightResInPx);

	JsonWriter->WriteArrayStart("frames");
	UE_LOG(LogTemp,Error,TEXT("total frame %d"),CameraTransforms.Num());
	for (int i = 0; i < CameraTransforms.Num(); i++)
	{
		JsonWriter->WriteObjectStart();
		// Deal with file_path
		
		{
			
			FString ResolvedFileName;
			TMap<FString, FString> FormatOverrides;
			FMoviePipelineFormatArgs FinalFormatArgs;
	
			// We really only need the output disk path for disk size info, but we'll try to resolve as much as possible anyways
			GetPipeline()->ResolveFilenameFormatArguments(OutputSettings->FileNameFormat, FormatOverrides, ResolvedFileName, FinalFormatArgs, nullptr,i);

			switch (ImageFormat)
			{
			case EImageFormat::JPEG:
				ResolvedFileName.ReplaceInline(TEXT(".{ext}"),TEXT(".jpeg"));
				break;
			case EImageFormat::PNG:
				ResolvedFileName.ReplaceInline(TEXT(".{ext}"),TEXT(".png"));
				break;
			}
			JsonWriter->WriteValue(TEXT("file_path"), ResolvedFileName);
		}
		
		// Deal with transforms_matrix
		{
			FRotationTranslationMatrix C2W = CameraTransforms[i];
			UE::Math::TMatrix<float> RotMatColMajor = UE::Math::TMatrix<float>(C2W.Rotator().Quaternion().ToMatrix().GetTransposed());
			UE::Math::TMatrix<float> NewC2W = RotMatColMajor;
			UE::Math::TVector4<float> Pos = UE::Math::TVector4<float>(C2W.TransformPosition({0,0,0}));
			
			switch (OutputCoordinateSystem)
			{
			case EOutputCoordinateSystem::NeRF:
				{
					UE::Math::TMatrix<float> CameraTrans({0,1,0,0},{0,0,1,0},{-1,0,0,0},{0,0,0,1});
					UE::Math::TMatrix<float> WorldTrans({0,1,0,0},{1,0,0,0},{0,0,1,0},{0,0,0,1});
	
					NewC2W = WorldTrans * RotMatColMajor * CameraTrans.Inverse();
					Pos = WorldTrans.TransformPosition(Pos);
				}
				break;
			case  EOutputCoordinateSystem::Unreal:
				break;
			}
			
			Pos /= 100; // Unreal uses cm by default.
			
			NewC2W.SetColumn(3,Pos);
			JsonWriter->WriteArrayStart(TEXT("transform_matrix"));

			for (int32 j = 0; j < 4; ++j) {
				JsonWriter->WriteArrayStart();
				for (int32 k  = 0; k < 4; ++k) {
					JsonWriter->WriteValue(NewC2W.M[j][k]);
				}
				JsonWriter->WriteArrayEnd();
			}
			JsonWriter->WriteArrayEnd();
		}
		
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd(); 
	
	JsonWriter->WriteObjectEnd();
	
	JsonWriter->Close();
	
	FString SaveDirectory = OutputSettings->OutputDirectory.Path;
	
	SaveDirectory.ReplaceInline(TEXT("{project_dir}"), *FPaths::ProjectDir());

	FString AbsPath = SaveDirectory + "/transform.json";
	FFileHelper::SaveStringToFile(JsonStr,*AbsPath);
	
	Frame = 0;
	CurrentCineCamera = nullptr;
	return ;
}

void UMoviePipelineCameraOutput::OnPostTickImpl()
{
	UE_LOG(LogTemp,Display,TEXT("Current Times %d\n"),Frame );
	Frame++;
	
	APlayerCameraManager* CameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	
	FVector Pos = CameraManager->GetCameraLocation();
	FRotator Rot = CameraManager->GetCameraRotation();
	auto CurrentFrameCamera = Cast<ACineCameraActor>(CameraManager->ViewTarget.Target.Get());
	if(CurrentFrameCamera ==nullptr)
	{
		UE_LOG(LogTemp,Error,TEXT("There is no camera in frame %d"),Frame);
		return;
	}
	else
	{
		CurrentCineCamera = CurrentFrameCamera;
	}

	UE_LOG(LogTemp, Warning, TEXT("current focal:%f"),CurrentCineCamera->GetCineCameraComponent()->CurrentFocalLength);
	
	// CameraManager->GetCameraCachePOV().
	FRotationTranslationMatrix World (Rot,Pos);
	
	CameraTransforms.Emplace(Rot,Pos);
	UE_LOG(LogTemp, Warning, TEXT("%d matrix %s"),Frame, *World.ToString());
}