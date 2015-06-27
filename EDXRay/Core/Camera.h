#pragma once

#include "Graphics/Camera.h"
#include "../ForwardDecl.h"

namespace EDX
{
	namespace RayTracer
	{
		struct CameraParameters
		{
			Vector3	Pos;
			Vector3	Target;
			Vector3	Up;
			float	FieldOfView;
			float	NearClip, FarClip;
			float	FocusPlaneDist;
			float	LensRadius;
		};

		class Camera : public EDX::Camera
		{
		public:
			float mLensRadius, mFocalPlaneDist;
			float mImagePlaneDist;

			// Differential
			Vector3 mDxCam;
			Vector3 mDyCam;

		public:
			void Init(const Vector3& pos,
				const Vector3& tar,
				const Vector3& up,
				const int resX,
				const int resY,
				const float FOV = 35.0f,
				const float nearClip = 1.0f,
				const float farClip = 1000.0f,
				const float lensR = 0.0f,
				const float focalDist = 0.0f);

			void Resize(int width, int height);
			void GenerateRay(const CameraSample& sample, Ray* pRay) const;
			void GenRayDifferential(const CameraSample& sample, RayDifferential* pRay) const;

			float GetLensRadius() const
			{
				return mLensRadius;
			}
			float GetFocusDistance() const
			{
				return mFocalPlaneDist;
			}
			float GetImagePlaneDistance() const
			{
				return mImagePlaneDist;
			}
			const CameraParameters GetCameraParams() const
			{
				CameraParameters ret;
				ret.Pos = mPos;
				ret.Target = mTarget;
				ret.Up = mUp;
				ret.FieldOfView = mFOV;
				ret.NearClip = mNearClip;
				ret.FarClip = mFarClip;
				ret.FocusPlaneDist = mFocalPlaneDist;
				ret.LensRadius = mLensRadius;

				return ret;
			}
		};
	}
}