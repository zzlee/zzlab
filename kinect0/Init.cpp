#include "stdafx.h"

#include "zzlab.h"
#include "zzlab/av.h"
#include "zzlab/utils.h"
#include "zzlab/gfx.h"
#include "zzlab/net.h"
#include "zzlab/wx.h"
#include "zzlab/d3d9.h"
#include "zzlab/di8.h"

#include <comdef.h>
#include <dinput.h>

#include <boost/array.hpp>

#include <NuiApi.h>
#include <KinectBackgroundRemoval.h>

_COM_SMARTPTR_TYPEDEF(INuiSensor, __uuidof(INuiSensor));
_COM_SMARTPTR_TYPEDEF(INuiBackgroundRemovedColorStream, __uuidof(INuiBackgroundRemovedColorStream));
_COM_SMARTPTR_TYPEDEF(INuiFrameTexture, __uuidof(INuiFrameTexture));
_COM_SMARTPTR_TYPEDEF(INuiCoordinateMapper, __uuidof(INuiCoordinateMapper));

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(Init);

extern utils::Timer* _Timer;
extern d3d9::IDirect3D9ExPtr _d3d9ex;
extern di8::IDirectInput8Ptr _di8;

utils::Timer* orgTimer;

d3d9::RenderWindow* rw;
di8::Input* input = nullptr;

utils::Delegate0 frameBeginDelegate;
utils::Delegate0 drawDelegate;

gfx::Camera camera;

d3d9::EffectResource* effect;
d3d9::EffectResource* effectTrans;
d3d9::MeshResource* mesh;
d3d9::TextureResource* texture;
d3d9::DynamicTextureResource* user;
D3DXCOLOR diffuse;

INuiSensorPtr nuiSensor;
BOOL nearMode = TRUE;

HANDLE colorStream = nullptr;
HANDLE depthStream = nullptr;

cv::Mat4b colorImage;
std::vector<NUI_DEPTH_IMAGE_PIXEL> depthImage;

INuiCoordinateMapperPtr coordMapper;
std::vector<NUI_COLOR_IMAGE_POINT> colorImagePoints;

void showUser()
{
	cv::imshow("colorImage", colorImage);
}

void frameBegin()
{
	//ZZLAB_TRACE_FUNCTION();

	input->update();

	if (input->getKey(DIK_ESCAPE))
	{
		frameBeginDelegate.cancel();
		drawDelegate.cancel();
		rw->cancel();

		// TODO: how to terminate gracefully? 
		//       we need render-device to be shutdown first then destroy the render window

		_MainService.post(bind(&d3d9::RenderWindow::Destroy, rw));
		return;
	}

	NUI_IMAGE_FRAME imageFrame;
	if (SUCCEEDED(nuiSensor->NuiImageStreamGetNextFrame(colorStream, 0, &imageFrame)))
	{
		NUI_LOCKED_RECT LockedRect;
		imageFrame.pFrameTexture->LockRect(0, &LockedRect, NULL, 0);

		{
			BYTE* src0 = (BYTE*)LockedRect.pBits;
			BYTE* src1 = (BYTE*)colorImage.data;
			for (int y = 0; y < 480; ++y, src0 += LockedRect.Pitch, src1 += colorImage.step)
			{
				cv::Vec4b* p0 = (cv::Vec4b*)src0;
				cv::Vec4b* p1 = (cv::Vec4b*)src1;
				for (int x = 0; x < 640; ++x, ++p0, ++p1)
				{
					(*p1)[0] = (*p0)[0];
					(*p1)[1] = (*p0)[1];
					(*p1)[2] = (*p0)[2];
					(*p1)[3] = 0;
				}
			}
		}

		imageFrame.pFrameTexture->UnlockRect(0);

		HR(nuiSensor->NuiImageStreamReleaseFrame(colorStream, &imageFrame));
	}

	if (SUCCEEDED(nuiSensor->NuiImageStreamGetNextFrame(depthStream, 0, &imageFrame)))
	{
		INuiFrameTexturePtr texture;
		HR(nuiSensor->NuiImageFrameGetDepthImagePixelFrameTexture(depthStream, &imageFrame, &nearMode, &texture));

		NUI_LOCKED_RECT LockedRect;
		texture->LockRect(0, &LockedRect, NULL, 0);

		memcpy(&depthImage[0], LockedRect.pBits, LockedRect.size);

		texture->UnlockRect(0);

		HR(nuiSensor->NuiImageStreamReleaseFrame(depthStream, &imageFrame));
	}

	NUI_SKELETON_FRAME skeletonFrame;
	if (SUCCEEDED(nuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
	{
		NUI_SKELETON_DATA* pSkeletonData = skeletonFrame.SkeletonData;
		{
			float closestDistance = 10000.0f;
			DWORD closestPlayerIDs[NUI_SKELETON_MAX_TRACKED_COUNT] = {
				NUI_SKELETON_INVALID_TRACKING_ID, NUI_SKELETON_INVALID_TRACKING_ID
			};

			for (size_t i = 0; i < NUI_SKELETON_COUNT; ++i)
			{
				NUI_SKELETON_DATA& skeletonData = skeletonFrame.SkeletonData[i];

				if (skeletonData.eTrackingState == NUI_SKELETON_TRACKED)
				{
					if (skeletonData.Position.z < closestDistance)
					{
						closestDistance = skeletonData.Position.z;
						closestPlayerIDs[0] = skeletonData.dwTrackingID;
					}
				}
			}

			nuiSensor->NuiSkeletonSetTrackedSkeletons(closestPlayerIDs);

			if (closestPlayerIDs[0] != NUI_SKELETON_INVALID_TRACKING_ID)
			{
				//_MainService.post(bind(showUser));

				HR(coordMapper->MapDepthFrameToColorFrame(
					NUI_IMAGE_RESOLUTION_640x480, 640 * 480, &depthImage[0], 
					NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 640 * 480, &colorImagePoints[0]));

				{
					BYTE* src0 = (BYTE*)&depthImage[0];
					BYTE* src1 = (BYTE*)&colorImagePoints[0];
					for (int y = 0; y < 480; ++y, src0 += 640 * sizeof(NUI_DEPTH_IMAGE_PIXEL),
						src1 += 640 * sizeof(NUI_COLOR_IMAGE_POINT))
					{
						NUI_DEPTH_IMAGE_PIXEL* p0 = (NUI_DEPTH_IMAGE_PIXEL*)src0;
						NUI_COLOR_IMAGE_POINT* p1 = (NUI_COLOR_IMAGE_POINT*)src1;
						for (int x = 0; x < 640; ++x, ++p0, ++p1)
						{
							const NUI_DEPTH_IMAGE_PIXEL& d = *p0;
							const NUI_COLOR_IMAGE_POINT& p = *p1;

							if (d.playerIndex > 0)
							{
								const NUI_SKELETON_DATA& skeletonAtPixel = skeletonFrame.SkeletonData[d.playerIndex - 1];
								if (skeletonAtPixel.dwTrackingID == closestPlayerIDs[0])
									colorImage.at<cv::Vec4b>(p.y, p.x)[3] = 255;
							}
						}
					}
				}

				user->update(colorImage);
			}
			else
			{
				user->set(cv::Scalar(0, 0, 0, 0));
			}
		}
	}

	rw->waitForFrameBegin(frameBeginDelegate());
}

void draw()
{
	//ZZLAB_TRACE_FUNCTION();

	{
		effect->effect->SetTexture("MainTex", texture->textures[0]);
		effect->effect->SetFloatArray("Diffuse", diffuse, sizeof(diffuse));
		Eigen::Projective3f MATRIX_MVP = camera.matrix() * Eigen::Affine3f::Identity();
		effect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		mesh->draw(effect->effect);
	}

	{
		effectTrans->effect->SetTexture("MainTex", user->textures[0]);
		effectTrans->effect->SetFloatArray("Diffuse", diffuse, sizeof(diffuse));
		Eigen::Projective3f MATRIX_MVP = camera.matrix() * Eigen::Affine3f::Identity();
		effectTrans->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		mesh->draw(effectTrans->effect);
	}

	rw->waitForFrameBegin(drawDelegate());
}

void initRenderer()
{
	ZZLAB_TRACE_FUNCTION();

	rw->clear.color = D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f);
	rw->init();

	input = new di8::Input();
	input->dinput = _di8;
	input->hwnd = (HWND)rw->GetHandle();
	input->init();

	camera.proj = gfx::orthoLH(1, 1, 1.0f, 100.0f);
	camera.updateT(
		gfx::lookAtLH(
		Eigen::Vector3f(0, 0, -10),
		Eigen::Vector3f(0.0f, 0.0f, 0.0f),
		Eigen::Vector3f::UnitY()));

	effect = rw->get<d3d9::EffectResource>(L"Unlit.fx");
	effectTrans = rw->get<d3d9::EffectResource>(L"Unlit_Transparent.fx");
	mesh = rw->get<d3d9::QuadMeshResource>(L"Quad");
	texture = rw->get<d3d9::TextureResource>(L"aa.png");
	diffuse = D3DXCOLOR(1, 1, 1, 1);

	user = new d3d9::DynamicTextureResource();
	user->dev = rw->dev;
	user->width = 640;
	user->height = 480;
	user->format = D3DFMT_A8R8G8B8;
	user->init();

	colorImage = cv::Mat4b(480, 640);
	depthImage.resize(640 * 480);
	colorImagePoints.resize(640 * 480);

	frameBeginDelegate.connect(frameBegin);
	drawDelegate.connect(draw);

	rw->waitForFrameBegin(frameBeginDelegate());
	rw->waitForDraw(drawDelegate());
}

void init()
{
	ZZLAB_TRACE_FUNCTION();

	orgTimer = _Timer;
	_Timer = new utils::ManualTimer();

	HR(NuiCreateSensorByIndex(0, &nuiSensor));
	HR(nuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX));

	HR(nuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480,
		0, 2, nullptr, &colorStream));
	HR(nuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_640x480,
		0, 2, nullptr, &depthStream));
	HR(nuiSensor->NuiSkeletonTrackingEnable(nullptr,
		NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT | (nearMode ? NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE : 0)));
	HR(nuiSensor->NuiGetCoordinateMapper(&coordMapper));

	rw = d3d9::loadWindow(_d3d9ex, _Settings.first_node(L"D3D9Window1"));
	gfx::_RenderService->post(bind(initRenderer));
}

void uninit()
{
	coordMapper = nullptr;
	nuiSensor->NuiShutdown();
	nuiSensor = nullptr;

	delete orgTimer;

	ZZLAB_TRACE_FUNCTION();
}

