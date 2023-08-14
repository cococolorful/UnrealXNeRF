#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineOutputBase.h"
#include "Async/Future.h"

class ACineCameraActor;

#include "MoviePipelineCameraOutput.generated.h"

UENUM()
enum class EOutputCoordinateSystem : uint8
{
	NeRF        UMETA(DisplayName="NeRF"),
	Unreal      UMETA(DisplayName="Unreal"),
};
UCLASS()
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineCameraOutput : public UMoviePipelineOutputBase
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "UMoviePipelineCameraOutput", "Camera Info"); }
#endif
public:
	UMoviePipelineCameraOutput()
	{
	}

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CameraOutput")
	EOutputCoordinateSystem OutputCoordinateSystem;
	
protected:
	// UMoviePipelineOutputBase Interface
	virtual void OnReceiveImageDataImpl(FMoviePipelineMergerOutputFrame* InMergedOutputFrame) override;
	virtual void BeginFinalizeImpl() {}
	virtual bool HasFinishedProcessingImpl() { return true; }
	virtual void FinalizeImpl() override;
	virtual void OnShotFinishedImpl(const UMoviePipelineExecutorShot* InShot, const bool bFlushToDisk = false) {}
	virtual void OnPipelineFinishedImpl() {}
	virtual void OnPostTickImpl() override;
	// ~UMoviePipelineOutputBase
	
	int Frame{0};
	
	ACineCameraActor* CurrentCineCamera;
	
	TArray<FRotationTranslationMatrix> CameraTransforms;
};
