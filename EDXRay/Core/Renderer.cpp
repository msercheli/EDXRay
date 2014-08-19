#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
#include "../Integrators/DirectLighting.h"
#include "Sampler.h"
#include "Film.h"
#include "DifferentialGeom.h"
#include "Graphics/Color.h"
#include "RenderTask.h"
#include "Config.h"

namespace EDX
{
	namespace RayTracer
	{
		void Renderer::Initialize(const RenderJobDesc& desc)
		{
			mJobDesc = desc;

			// Initalize camera
			if (!mpCamera)
			{
				mpCamera = new Camera();
			}
			mpCamera->Init(desc.CameraParams.Pos,
				desc.CameraParams.Target,
				desc.CameraParams.Up,
				desc.ImageWidth,
				desc.ImageHeight,
				desc.CameraParams.FieldOfView,
				desc.CameraParams.NearClip,
				desc.CameraParams.FarClip,
				desc.CameraParams.LensRadius,
				desc.CameraParams.FocusPlaneDist);

			// Initialize scene
			mpScene = new Scene;
			mpIntegrator = new DirectLightingIntegrator;

			mpFilm = new Film;
			mpFilm->Init(desc.ImageWidth, desc.ImageHeight);

			mTaskSync.Init(desc.ImageWidth, desc.ImageHeight);

			mThreadScheduler.InitTAndLaunchThreads();
		}

		Renderer::~Renderer()
		{
		}

		void Renderer::RenderFrame(RandomGen& random, MemoryArena& memory)
		{
			RenderTile* pTask;
			while (mTaskSync.GetNextTask(pTask))
			{
				for (auto y = pTask->minY; y < pTask->maxY; y++)
				{
					for (auto x = pTask->minX; x < pTask->maxX; x++)
					{
						Sample sample;
						sample.imageX = x;
						sample.imageY = y;
						RayDifferential ray;
						mpCamera->GenRayDifferential(sample, &ray);

						Color L = mpIntegrator->Li(ray, mpScene.Ptr(), &sample, random, memory);

						mpFilm->AddSample(x, y, L);
					}
				}
			}
		}

		void Renderer::RenderImage(int threadId, RandomGen& random, MemoryArena& memory)
		{
			for (auto i = 0; i < mJobDesc.SamplesPerPixel; i++)
			{
				// Sync barrier before render
				mTaskSync.SyncThreadsPreRender(threadId);

				RenderFrame(random, memory);

				// Sync barrier after render
				mTaskSync.SyncThreadsPostRender(threadId);

				// One thread only
				if (threadId == 0)
				{
					mpFilm->IncreSampleCount();
					mpFilm->ScaleToPixel();
					mTaskSync.ResetTasks();
				}
			}
		}

		void Renderer::LaunchRenderThreads()
		{
			for (auto i = 0; i < mThreadScheduler.GetThreadCount(); i++)
			{
				mTasks.push_back(new RenderTask(this));

				mThreadScheduler.AddTasks(Task((Task::TaskFunc)&RenderTask::_Render, mTasks[i]));
			}
		}

		const Color* Renderer::GetFrameBuffer() const
		{
			return mpFilm->GetPixelBuffer();
		}
	}
}