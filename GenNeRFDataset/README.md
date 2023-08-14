# Generate NeRF Dataset in Unreal
Unreal有着一套优秀的MoviePipeline，原生内容中已经可以导出图片，但是缺少了相机信息。因此，我们的修改主要是增加相机位姿的输出。
## 增加OnTick的调用
在UE5.2版本中，`UMoviePipelineSetting`存在着虚函数`OnTick`函数，但是在`UMoivePipeline`中，并没有对此函数进行调用。因此，我们需要修改源码，添加以下代码
```cpp
void UMoviePipeline::RenderFrame()
{
	// Flush built in systems before we render anything. This maximizes the likelihood that the data is prepared for when
	// the render thread uses it.
	FlushAsyncEngineSystems();

	// Send any output frames that have been completed since the last render.
	ProcessOutstandingFinishedFrames();

	FMoviePipelineCameraCutInfo& CurrentCameraCut = ActiveShotList[CurrentShotIndex]->ShotInfo;
	APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController();

	if (CurrentCameraCut.State == EMovieRenderShotState::Rendering)
	{
		for (UMoviePipelineOutputBase* OutputContainer : GetPipelinePrimaryConfig()->GetOutputContainers())
		{
			OutputContainer->OnPostTick();
		}
	}
    ...
}
```

## 实现相机信息导出类
具体的内容在本文件夹下的`MoviePipelineCameraOutput.h/cpp`中，这部分主要需要解决的是不同坐标系下的相机姿态的转换。TODO