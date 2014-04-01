#ifndef __ZZLAB_WX_H__
#define __ZZLAB_WX_H__

#ifdef ZZWX_EXPORTS
#define ZZWX_API __declspec(dllexport)
#else
#define ZZWX_API __declspec(dllimport)
#endif

#include "zzlab.h"
#include "zzlab/utils.h"
#include "zzlab/pystring.h"

#include <wx/app.h>
#include <wx/apptrait.h>
#include <wx/evtloop.h>

#include <boost/asio/deadline_timer.hpp>
#include <boost/system/error_code.hpp>

namespace zzlab
{
	namespace wx
	{
		ZZWX_API void install(void);

		class ZZWX_API EventLoop : public wxGUIEventLoop
		{
		public:
			EventLoop();
			virtual ~EventLoop();

			virtual int DoRun();

		protected:
			void step(boost::system::error_code err = boost::system::error_code());
			bool ProcessEvents();
			void DoExit();

		protected:
			boost::asio::deadline_timer mTimer;
			bool mNewIteration;
		};

		class ZZWX_API AppTraits : public wxGUIAppTraits
		{
			virtual wxEventLoopBase* CreateEventLoop();
		};

		class ZZWX_API App : public wxApp
		{
		public:
			virtual wxAppTraits* CreateTraits();
			virtual bool OnInit();
			virtual int OnExit();
		};

		template<class T> T* loadWindow(XmlNode* node, wxWindow* parent = NULL, wxWindowID id = wxID_ANY, long style = 0)
		{
			ZZLAB_INFO("Loading " << node->name() << "...");

			XmlAttribute *attr = node->first_attribute("title");
			wxString title = attr ? attr->value() : "wxWindow";

			wxPoint position;
			attr = node->first_attribute("position");
			if (attr)
			{
				std::vector<std::string> tokens;
				pystring::split(attr->value(), tokens, ",");
				if (tokens.size() != 2)
					position = wxDefaultPosition;
				else
				{
					position.x = atoi(pystring::strip(tokens[0]).c_str());
					position.y = atoi(pystring::strip(tokens[1]).c_str());
				}
			}
			else
				position = wxDefaultPosition;

			wxSize size;
			attr = node->first_attribute("size");
			if (attr)
			{
				std::vector<std::string> tokens;
				pystring::split(attr->value(), tokens, ",");
				if (tokens.size() != 2)
					size = wxDefaultSize;
				else
				{
					size.x = atoi(pystring::strip(tokens[0]).c_str());
					size.y = atoi(pystring::strip(tokens[1]).c_str());
				}
			}
			else
				size = wxDefaultSize;

			attr = node->first_attribute("style");
			if (attr)
			{
				std::vector<std::string> tokens;
				pystring::split(attr->value(), tokens, "|");
				for (std::vector<std::string>::const_iterator i = tokens.begin(); i != tokens.end(); ++i)
				{
					std::string s = pystring::strip(*i);
#define _MATCH(x) \
	if (_stricmp(s.c_str(), # x) == 0) \
	style |= x; \
			else

					_MATCH(wxBORDER_DEFAULT)
						_MATCH(wxBORDER_SIMPLE)
						_MATCH(wxBORDER_SUNKEN)
						_MATCH(wxBORDER_RAISED)
						_MATCH(wxBORDER_STATIC)
						_MATCH(wxBORDER_THEME)
						_MATCH(wxBORDER_NONE)
						_MATCH(wxBORDER_DOUBLE)
						_MATCH(wxTRANSPARENT_WINDOW)
						_MATCH(wxCLIP_CHILDREN)
						_MATCH(wxCAPTION)
						_MATCH(wxDEFAULT_DIALOG_STYLE)
						_MATCH(wxRESIZE_BORDER)
						_MATCH(wxSTAY_ON_TOP)
						_MATCH(wxDEFAULT_FRAME_STYLE)
						_MATCH(wxICONIZE)
						_MATCH(wxMINIMIZE)
						_MATCH(wxMINIMIZE_BOX)
						_MATCH(wxMAXIMIZE)
						_MATCH(wxMAXIMIZE_BOX)
						_MATCH(wxCLOSE_BOX)
						_MATCH(wxSYSTEM_MENU)
						_MATCH(wxFRAME_TOOL_WINDOW)
						_MATCH(wxFRAME_NO_TASKBAR)
						_MATCH(wxFRAME_FLOAT_ON_PARENT)
						_MATCH(wxFRAME_SHAPED)
						NULL;

#undef _MATCH
				}
			}
			else
				style = 0;

			return new T(parent, id, title, position, size, style);
		}
	}
}

#endif // __ZZLAB_WX_H__