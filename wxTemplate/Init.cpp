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

using namespace boost;
using namespace zzlab;

ZZLAB_USE_LOG4CPLUS(Init);

extern utils::Timer* _Timer;
extern d3d9::IDirect3D9ExPtr _d3d9ex;
extern di8::IDirectInput8Ptr _di8;

utils::Timer* oldTimer;

d3d9::RenderWindow* rw;
di8::Input* input = nullptr;

utils::Delegate0 frameBeginDelegate;
utils::Delegate0 drawDelegate;

gfx::Camera camera;

d3d9::EffectResource* effect;
d3d9::MeshResource* mesh;
d3d9::TextureResource* texture;
D3DXCOLOR diffuse;

void quitapp()
{
	frameBeginDelegate.cancel();
	drawDelegate.cancel();
	rw->cancel();

	_MainService.post(bind(&d3d9::RenderWindow::Destroy, rw));
}

void frameBegin()
{
	//ZZLAB_TRACE_FUNCTION();

	input->update();

	if (input->getKey(DIK_ESCAPE))
		gfx::_RenderService->post(bind(quitapp));

	rw->waitForFrameBegin(frameBeginDelegate());
}

void draw()
{
	//ZZLAB_TRACE_FUNCTION();

	effect->effect->SetTexture("MainTex", texture->textures[0]);
	effect->effect->SetFloatArray("Diffuse", diffuse, sizeof(diffuse));

	{
		Eigen::Projective3f MATRIX_MVP = camera.matrix() * Eigen::Affine3f::Identity();
		effect->effect->SetMatrix("MATRIX_MVP", (const D3DXMATRIX *)MATRIX_MVP.matrix().data());
		mesh->draw(effect->effect);
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
	mesh = rw->get<d3d9::QuadMeshResource>(L"Quad");
	texture = rw->get<d3d9::TextureResource>(L"aa.png");
	diffuse = D3DXCOLOR(1, 1, 1, 1);

	frameBeginDelegate.connect(frameBegin);
	drawDelegate.connect(draw);

	rw->waitForFrameBegin(frameBeginDelegate());
	rw->waitForDraw(drawDelegate());
}

void init()
{
	ZZLAB_TRACE_FUNCTION();

	oldTimer = _Timer;
	_Timer = new utils::ManualTimer();

	rw = d3d9::loadWindow(_d3d9ex, _Settings.first_node(L"D3D9Window1"));
	gfx::_RenderService->post(bind(initRenderer));
}

void uninit()
{
	delete oldTimer;

	ZZLAB_TRACE_FUNCTION();
}

