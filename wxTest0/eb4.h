#pragma once

#include <utility>

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/d3d9.h"
#include "zzlab/gfx.h"

#include "eb.pb.h"

extern std::pair<std::string, D3DXCOLOR> defaultColours[];
extern void loadEdgeParameters(eb::Workspace& workspace, zzlab::XmlNode* node);
extern void setDefaultWorkspace(eb::Workspace& workspace, zzlab::XmlNode* node);
extern void genHorEdge(const eb::Workspace& workspace, zzlab::d3d9::DynamicTextureResource* res);
extern void genVerEdge(const eb::Workspace& workspace, zzlab::d3d9::DynamicTextureResource* res);

struct VertexTraits
{
	enum
	{
		none = 0,
		cease = 1,
	};
};

typedef zzlab::gfx::Subdivision2D<double, VertexTraits> Subdivision_t;
typedef Subdivision_t::Vertex2D Vertex2D;
typedef Subdivision_t::Vertex2Ds Vertex2Ds;
typedef Subdivision_t::Vertex2DsPtr Vertex2DsPtr;
