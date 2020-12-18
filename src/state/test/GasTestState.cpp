
#include "GasTestState.hpp"

#include <sstream>

#include <spdlog/spdlog.h>

#include "IFileSys.hpp"
#include "gas/Fuel.hpp"

namespace ehb
{
    void GasTestState::enter()
    {
        auto log = spdlog::get("log");
        log->info("GasTestState::enter()");

        {
            auto stream = std::make_unique<std::stringstream>();
            *stream << "[t:template,n:actor]{doc=\"Generic brained objects\";}";

            if (Fuel doc; doc.load(*stream))
            {
                assert(doc.child("actor")->name() == "actor");
                assert(doc.child("actor")->type() == "template");
            }
        }

        {
            std::string tmpl = R"(
	[t:template,n:3W_dsx_skeleton_04]{

	[inventory]
		{
		[pcontent]
	{
		[oneof*]
		{
			[gold*]
			{
				chance = 0.2;
				min = 143018;
				max = 286036;
			}
			[oneof*]
			{
				chance = 0.088888888888889;
				il_main = potion_mana_super;
				il_main = potion_health_super;
			}
			[oneof*]
			{
				chance = 0.15;
				il_main = #weapon/186;
				il_main = #armor/45-510;
				il_main = #*/186;
			}
			[oneof*]
			{
				chance = 0.0075;
				il_main = #weapon/-rare(1)/225-265;
				il_main = #armor/-rare(1)/0-645;
				il_main = #*/-rare(1)/225-265;
			}
			
		}
	}
	}	
		}
})";
            auto stream = std::make_unique<std::stringstream>();
            *stream << tmpl;

            if (Fuel doc; doc.load(*stream))
            {
            }
        }
    }

    void GasTestState::leave()
    {
    }

    void GasTestState::update(double deltaTime)
    {
    }

    bool GasTestState::handle(const osgGA::GUIEventAdapter& event, osgGA::GUIActionAdapter& action)
    {
        return false;
    }
}
