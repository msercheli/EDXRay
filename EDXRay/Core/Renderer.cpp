#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"
#include "../Integrators/DirectLighting.h"
#include "../Integrators/PathTracing.h"
#include "../Integrators/BidirectionalPathTracing.h"
#include "Sampler.h"
#include "../Sampler/RandomSampler.h"
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

			mTaskSync.Init(desc.ImageWidth, desc.ImageHeight);

			mTaskSync.SetAbort(false);
			ThreadScheduler::Instance()->InitAndLaunchThreads();
		}

		void Renderer::InitComponent()
		{
			Filter* pFilter;
			switch (mJobDesc.FilterType)
			{
			case EFilterType::Box:
				pFilter = new BoxFilter;
				break;
			case EFilterType::Gaussian:
				pFilter = new GaussianFilter;
				break;
			case EFilterType::MitchellNetravali:
				pFilter = new MitchellNetravaliFilter;
				break;
			}

			mpFilm = mJobDesc.UseRHF ? new FilmRHF : new Film;
			mpFilm->Init(mJobDesc.ImageWidth, mJobDesc.ImageHeight, new GaussianFilter);

			switch (mJobDesc.SamplerType)
			{
			case ESamplerType::Random:
				mpSampler = new RandomSampler;
				break;
			case ESamplerType::Sobol:
				mpSampler = new RandomSampler;
				break;
			case ESamplerType::Metropolis:
				mpSampler = new RandomSampler;
				break;
			}

			switch (mJobDesc.IntegratorType)
			{
			case EIntegratorType::DirectLighting:
				mpIntegrator = new DirectLightingIntegrator(mJobDesc.MaxPathLength);
				break;
			case EIntegratorType::PathTracing:
				mpIntegrator = new PathTracingIntegrator(mJobDesc.MaxPathLength);
				break;
			case EIntegratorType::BidirectionalPathTracing:
				mpIntegrator = new BidirPathTracingIntegrator(mJobDesc.MaxPathLength, mpCamera.Ptr(), mpFilm.Ptr());
				break;
			case EIntegratorType::MultiplexedMLT:
				mpIntegrator = new BidirPathTracingIntegrator(mJobDesc.MaxPathLength, mpCamera.Ptr(), mpFilm.Ptr());
				break;
			case EIntegratorType::StochasticPPM:
				mpIntegrator = new BidirPathTracingIntegrator(mJobDesc.MaxPathLength, mpCamera.Ptr(), mpFilm.Ptr());
				break;
			}

			BakeSamples();
			mpScene->InitAccelerator();
		}

		Renderer::~Renderer()
		{
			ThreadScheduler::DeleteInstance();
		}

		void Renderer::Resize(int width, int height)
		{
			mJobDesc.ImageWidth = width;
			mJobDesc.ImageHeight = height;

			mpCamera->Resize(width, height);
			//mpFilm->Resize(width, height);
			mTaskSync.Init(width, height);
		}

		void Renderer::RenderFrame(SampleBuffer* pSampleBuf, RandomGen& random, MemoryArena& memory)
		{
			RenderTile* pTask;
			while (mTaskSync.GetNextTask(pTask))
			{
				for (auto y = pTask->minY; y < pTask->maxY; y++)
				{
					for (auto x = pTask->minX; x < pTask->maxX; x++)
					{
						if (mTaskSync.Aborted())
							return;

						mpSampler->GenerateSamples(pSampleBuf, random);
						pSampleBuf->imageX += x;
						pSampleBuf->imageY += y;

						RayDifferential ray;
						mpCamera->GenRayDifferential(*pSampleBuf, &ray);

						Color L = mpIntegrator->Li(ray, mpScene.Ptr(), pSampleBuf, random, memory);

						mpFilm->AddSample(pSampleBuf->imageX, pSampleBuf->imageY, L);
						memory.FreeAll();
					}
				}
			}
		}

		void Renderer::RenderImage(int threadId, RandomGen& random, MemoryArena& memory)
		{
			SampleBuffer* pSampleBuf = mpSampleBuf->Duplicate(1);

			for (auto i = 0; i < mJobDesc.SamplesPerPixel; i++)
			{
				// Sync barrier before render
				mTaskSync.SyncThreadsPreRender(threadId);

				RenderFrame(pSampleBuf, random, memory);

				// Sync barrier after render
				mTaskSync.SyncThreadsPostRender(threadId);

				// One thread only
				if (threadId == 0)
				{
					mpFilm->IncreSampleCount();
					mpFilm->ScaleToPixel();
					mTaskSync.ResetTasks();
				}

				if (mTaskSync.Aborted())
					break;
			}

			SafeDeleteArray(pSampleBuf);
		}

		void Renderer::BakeSamples()
		{
			mpSampleBuf = new SampleBuffer;
			mpIntegrator->RequestSamples(mpScene.Ptr(), mpSampleBuf.Ptr());
		}

		void Renderer::QueueRenderTasks()
		{
			mpFilm->Clear();
			mTaskSync.SetAbort(false);

			for (auto i = 0; i < ThreadScheduler::Instance()->GetThreadCount(); i++)
			{
				mTasks.push_back(new RenderTask(this));
				ThreadScheduler::Instance()->AddTasks(Task((Task::TaskFunc)&RenderTask::_Render, mTasks[i]));
			}
		}

		void Renderer::StopRenderTasks()
		{
			mTaskSync.SetAbort(true);
			ThreadScheduler::Instance()->JoinAllTasks();
		}

		void Renderer::SetCameraParams(const CameraParameters& params)
		{
			mJobDesc.CameraParams = params;

			mpCamera->Init(mJobDesc.CameraParams.Pos,
				mJobDesc.CameraParams.Target,
				mJobDesc.CameraParams.Up,
				mJobDesc.ImageWidth,
				mJobDesc.ImageHeight,
				mJobDesc.CameraParams.FieldOfView,
				mJobDesc.CameraParams.NearClip,
				mJobDesc.CameraParams.FarClip,
				mJobDesc.CameraParams.LensRadius,
				mJobDesc.CameraParams.FocusPlaneDist);
		}

		Film* Renderer::GetFilm()
		{
			return mpFilm.Ptr();
		}
	}
}