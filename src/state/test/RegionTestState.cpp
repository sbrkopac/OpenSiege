
#include "RegionTestState.hpp"

#include "IFileSys.hpp"
#include "gas/Fuel.hpp"
#include "osg/ReaderWriterSNO.hpp"

#include <set>
#include <osgDB/ReadFile>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osgDB/Options>
#include <osg/ComputeBoundsVisitor>
#include <osgViewer/Viewer>

#include <spdlog/spdlog.h>

namespace ehb
{
    void RegionTestState::enter()
    {
        auto log = spdlog::get("log");
        log->set_level(spdlog::level::debug);

        log->info("RegionTestState::enter()");

        auto node = osgDB::readNodeFile("/world/maps/multiplayer_world/regions/town_center/terrain_nodes/nodes.gas");
        uint32_t targetNodeGuid = 0; node->getUserValue("targetnode", targetNodeGuid);

        osg::MatrixTransform* targetNodeXform = dynamic_cast<osg::MatrixTransform *>(node->getOrCreateUserDataContainer()->getUserObject(1));

        // TODO: is there a better way to do this?
        // re-position the camera based on the size of node and orient it up a little bit get a birds eye-view
        if (auto manipulator = viewer.getCameraManipulator())
        {
            if (SiegeNodeMesh* mesh = static_cast<SiegeNodeMesh*>(targetNodeXform->getChild(0)))
            {
                osg::ComputeBoundsVisitor cbv;
                mesh->accept(cbv);

                osg::BoundingSphere sphere;
                sphere.expandBy(cbv.getBoundingBox());

                double radius = osg::maximum(double(sphere.radius()), 1e-6);
                double dist = 7.f * radius;

                manipulator->setHomePosition(sphere.center() + osg::Vec3d(0.0, -dist, 15.0f), sphere.center(), osg::Vec3d(0.0f, 0.0f, 1.0f));
                manipulator->home(1);
            }
            else
            {
                log->error("failed to find siegenodemesh attached to transform with guid: {}", targetGuid);
            }
        }

        scene.addChild(node);

        #if 0
        // use std::filesystem?
        static const std::string mapsFolder = "/world/maps/";
        static const std::string worldName = "multiplayer_world";
        static const std::string regionName = "town_center";

        static const std::string fullRegionPath = mapsFolder + worldName + "/regions/" + regionName;
        static const std::string mainDotGas = fullRegionPath + "/main.gas";
        static const std::string nodesDotGas = fullRegionPath + "/terrain_nodes/nodes.gas";

        log->debug("attempting to load region @ path {}", mainDotGas);

        osg::ref_ptr<osg::Group> regionGroup = new osg::Group;

        scene.addChild(regionGroup);

        // main.gas
        if (auto stream = fileSys.createInputStream(mainDotGas))
        {
            if (Fuel doc; doc.load(*stream))
            {
                // this is mandatory but i'm not sure all the fields under it are required
                if (auto child = doc.child("region"))
                {
                    std::string created = child->valueOf("created");
                    std::string description = child->valueOf("description");
                    std::string guid = child->valueOf("guid");
                    std::string lastmodified = child->valueOf("lastmodified");
                    std::string notes = child->valueOf("notes");
                    std::string scid_range = child->valueOf("scid_range");
                    std::string se_version = child->valueOf("se_version");

                    // hold onto our region guid for lookup in the future
                    regionGroup->setName(guid);
                }
            }
        }
        else
        {
            log->error("failed to load region @ {}", mainDotGas);

            return;
        }

        log->debug("attempting to load nodesDotGas @ path {}", nodesDotGas);



        // nodes.gas
        if (auto stream = fileSys.createInputStream(nodesDotGas))
        {

        }
        else
        {
            log->error("failed to load nodesDotGas @ {}", nodesDotGas);
        }

        #endif
    }

    void RegionTestState::leave()
    {
    }

    void RegionTestState::update(double deltaTime)
    {
    }

    bool RegionTestState::handle(const osgGA::GUIEventAdapter& event, osgGA::GUIActionAdapter& action)
    {
        return false;
    }
}
