#include "stdafx.h"

#include <utility>

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/d3d9.h"

#include "eb.pb.h"

ZZLAB_USE_LOG4CPLUS(eb4);

using namespace boost;
using namespace zzlab;
using namespace google;

std::pair<std::string, D3DXCOLOR> defaultColours[] =
{
	std::make_pair("white", D3DXCOLOR(1, 1, 1, 1)),
	std::make_pair("yellow", D3DXCOLOR(1, 1, 0, 1)),
	std::make_pair("fuchsia", D3DXCOLOR(1, 0, 1, 1)),
	std::make_pair("red", D3DXCOLOR(1, 0, 0, 1)),
	std::make_pair("silver", D3DXCOLOR(0.75f, 0.75f, 0.75f, 1)),
	std::make_pair("gray", D3DXCOLOR(0.5f, 0.5f, 0.5f, 1)),
	std::make_pair("olive", D3DXCOLOR(0.5f, 0.5f, 0, 1)),
	std::make_pair("purple", D3DXCOLOR(0.5f, 0, 0.5f, 1)),
	std::make_pair("maroon", D3DXCOLOR(0.5f, 0, 0, 1)),
	std::make_pair("aqua", D3DXCOLOR(0, 1, 1, 1)),
	std::make_pair("lime", D3DXCOLOR(0, 1, 0, 1)),
	std::make_pair("teal", D3DXCOLOR(0, 0.5f, 0.5f, 1)),
	std::make_pair("green", D3DXCOLOR(0, 0.5f, 0, 1)),
	std::make_pair("blue", D3DXCOLOR(0, 0, 1, 1)),
	std::make_pair("navy", D3DXCOLOR(0, 0, 0.5f, 1)),
	std::make_pair("black", D3DXCOLOR(0, 0, 0, 1)),
};

int defaultSubdivLevel = 3;
int defaultCPCols = 9;
int defaultCPRows = 5;
int defaultGridWidth = 9;
int defaultGridHeight = 5;

void loadEdgeParameters(eb::Workspace& workspace, XmlNode* node)
{
	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_left_edge_settings();
		XmlNode* n = node->first_node(L"left");

		edgeBlendSettings->set_enable(_wcsicmp(n->first_attribute(L"enable")->value(), L"true") == 0);
		edgeBlendSettings->set_width(_wtof(n->first_attribute(L"width")->value()));
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_right_edge_settings();
		XmlNode* n = node->first_node(L"right");

		edgeBlendSettings->set_enable(_wcsicmp(n->first_attribute(L"enable")->value(), L"true") == 0);
		edgeBlendSettings->set_width(_wtof(n->first_attribute(L"width")->value()));
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_top_edge_settings();
		XmlNode* n = node->first_node(L"top");

		edgeBlendSettings->set_enable(_wcsicmp(n->first_attribute(L"enable")->value(), L"true") == 0);
		edgeBlendSettings->set_width(_wtof(n->first_attribute(L"width")->value()));
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings =
			workspace.mutable_bottom_edge_settings();
		XmlNode* n = node->first_node(L"bottom");

		edgeBlendSettings->set_enable(_wcsicmp(n->first_attribute(L"enable")->value(), L"true") == 0);
		edgeBlendSettings->set_width(_wtof(n->first_attribute(L"width")->value()));
	}

	{
		eb::LatticeSettings *latticeSettings =
			workspace.mutable_lattice_settings();

		XmlAttribute* attr = node->first_attribute(L"subdiv-level");
		if (attr)
		{
			int level = _wtoi(attr->value());
			ZZLAB_INFO("SubDivLevel=" << level);
			latticeSettings->set_subdiv_level(level);
		}
	}
}

void setDefaultWorkspace(eb::Workspace& workspace, XmlNode* node)
{
	const size_t iWidth = defaultCPCols;
	const size_t iHeight = defaultCPRows;

	{
		eb::EditorSettings *editorSettings =
			workspace.mutable_editor_settings();

		editorSettings->set_display_mode(eb::DM_MESH_GRID);
		editorSettings->set_edit_mode(eb::EM_FREE);

		{
			const std::pair<std::string, D3DXCOLOR> &clr = defaultColours[8];

			eb::Colour *colour = editorSettings->mutable_line_colour();
			colour->set_r(clr.second.r);
			colour->set_g(clr.second.g);
			colour->set_b(clr.second.b);
		}

		{
			const std::pair<std::string, D3DXCOLOR> &clr = defaultColours[10];

			eb::Colour *colour = editorSettings->mutable_background_colour();
			colour->set_r(clr.second.r);
			colour->set_g(clr.second.g);
			colour->set_b(clr.second.b);
		}

		editorSettings->set_adjust_speed(5);
		editorSettings->set_show_control_point(true);

		{
			eb::Size2i *size = editorSettings->mutable_grid_size();
			size->set_width(defaultGridWidth);
			size->set_height(defaultGridHeight);
		}

		editorSettings->set_select_mode(eb::SM_SINGLE);

		{
			eb::Point2i *pt = editorSettings->mutable_selected();
			pt->set_x(iWidth / 2);
			pt->set_y(iHeight / 2);
		}
	}

	{
		const float fWidth1 = iWidth - 1;
		const float fHeight1 = iHeight - 1;

		eb::LatticeSettings *latticeSettings =
			workspace.mutable_lattice_settings();

		{
			eb::Size2i *size = latticeSettings->mutable_size();
			size->set_width(iWidth), size->set_height(iHeight);
		}

		latticeSettings->set_subdiv_level(3);

		{
			protobuf::RepeatedPtrField<eb::ControlPoint> *cps =
				latticeSettings->mutable_control_points();

			// initialize vertices
			for (size_t y = 0; y < iHeight; ++y)
			{
				for (size_t x = 0; x < iWidth; ++x)
				{
					eb::ControlPoint *cp = cps->Add();
					cp->set_x(x / fWidth1);
					cp->set_y(y / fHeight1);
					if ((x == 0 && y == 0) || (x == 0 && y == iHeight - 1)
						|| (x == iWidth - 1 && y == iHeight - 1)
						|| (x == iWidth - 1 && y == 0))
						cp->set_type(eb::CP_CEASE);
					else
						cp->set_type(eb::CP_FREE);
				}
			}
		}
	}

	loadEdgeParameters(workspace, node);
	
	{
		eb::EdgeBlendSettings *edgeBlendSettings = workspace.mutable_left_edge_settings();

		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings = workspace.mutable_right_edge_settings();

		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings = workspace.mutable_top_edge_settings();

		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}

	{
		eb::EdgeBlendSettings *edgeBlendSettings = workspace.mutable_bottom_edge_settings();

		edgeBlendSettings->set_blending(1.0f);
		edgeBlendSettings->set_gamma(2.0f);
		edgeBlendSettings->set_center(0.5f);
	}
}

void genHorEdge(const eb::Workspace& workspace, d3d9::DynamicTextureResource* res)
{
	std::vector<double> left;
	{
		{
			const eb::EdgeBlendSettings &edgeBlendSettings =
				workspace.left_edge_settings();
			left.resize(edgeBlendSettings.width());
			if (edgeBlendSettings.enable())
			{
				gfx::genEdgeMatte(edgeBlendSettings.blending(),
					edgeBlendSettings.gamma(), edgeBlendSettings.center(),
					left.begin(), left.end());
			}
			else
			{
				fill(left.begin(), left.end(), 1);
			}
		}
	}

	std::vector<double> right;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.right_edge_settings();
		right.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				right.begin(), right.end());
		}
		else
		{
			fill(right.begin(), right.end(), 1);
		}
	}

	cv::Mat4b a(1, res->width);
	cv::Vec4b *p = (cv::Vec4b*)a.data;
	for (size_t i = 0; i < left.size(); ++i, ++p)
	{
		*p = cv::Vec4b(255 * left[i], 255 * left[i], 255 * left[i], 255);
	}
	for (size_t i = left.size(); i < a.cols - right.size();
		++i, ++p)
	{
		*p = cv::Vec4b(255, 255, 255, 255);
	}
	for (size_t i = a.cols - right.size(); i < a.cols;
		++i, ++p)
	{
		*p = cv::Vec4b(255 * right[a.cols - i - 1], 255 * right[a.cols - i - 1], 255 * right[a.cols - i - 1], 255);
	}

	d3d9::updateDynamicTexture(res->dev, res->texture, a);
}

void genVerEdge(const eb::Workspace& workspace, d3d9::DynamicTextureResource* res)
{
	std::vector<double> top;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.top_edge_settings();
		top.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				top.begin(), top.end());
		}
		else
		{
			fill(top.begin(), top.end(), 1);
		}
	}

	std::vector<double> bottom;
	{
		const eb::EdgeBlendSettings &edgeBlendSettings =
			workspace.bottom_edge_settings();
		bottom.resize(edgeBlendSettings.width());
		if (edgeBlendSettings.enable())
		{
			gfx::genEdgeMatte(edgeBlendSettings.blending(),
				edgeBlendSettings.gamma(), edgeBlendSettings.center(),
				bottom.begin(), bottom.end());
		}
		else
		{
			fill(bottom.begin(), bottom.end(), 1);
		}
	}

	cv::Mat4b a(1, res->width);
	cv::Vec4b *p = (cv::Vec4b*)a.data;
	for (size_t i = 0; i < top.size(); ++i, ++p)
	{
		*p = cv::Vec4b(255 * top[i], 255 * top[i], 255 * top[i], 255);
	}
	for (size_t i = top.size(); i < a.cols - bottom.size();
		++i, ++p)
	{
		*p = cv::Vec4b(255, 255, 255, 255);
	}
	for (size_t i = a.cols - bottom.size(); i < a.cols;
		++i, ++p)
	{
		*p = cv::Vec4b(255 * bottom[a.cols - i - 1], 255 * bottom[a.cols - i - 1], 255 * bottom[a.cols - i - 1], 255);
	}

	d3d9::updateDynamicTexture(res->dev, res->texture, a);
}