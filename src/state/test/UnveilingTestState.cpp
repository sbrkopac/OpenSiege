
#include "UnveilingTestState.hpp"

#include "IFileSys.hpp"
#include "gas/Fuel.hpp"
#include "osg/SiegeNodeMesh.hpp"
#include "osg/Aspect.hpp"
#include "ContentDb.hpp"
#include "world/Region.hpp"
#include "spdlog/fmt/ostr.h"

#include <set>
#include <osgDB/ReadFile>
#include <osg/Group>
#include <osgDB/Options>
#include <osg/ComputeBoundsVisitor>
#include <osgViewer/Viewer>

#include <osgUtil/Optimizer>

namespace ehb
{
    struct SiegePos
    {
        uint32_t guid = 0;
        osg::Vec3 pos;

        explicit SiegePos(float x, float y, float z, uint32_t guid) : guid(guid)
        {
            pos.x() = x;
            pos.y() = y;
            pos.z() = z;
        }
    };

    // SiegePos pos = valueAsSiegePos("-1.11579,0,-3.83528,0xc4660a9d");
    static SiegePos valueAsSiegePos(const std::string& value)
    {
        const auto itr1 = value.find(',');
        const auto itr2 = value.find(',', itr1 + 1);
        const auto itr3 = value.find(',', itr2 + 2);

        float x = std::stof(value.substr(0, itr1));
        float y = std::stof(value.substr(itr1 + 1, itr2 - itr1 - 1));
        float z = std::stof(value.substr(itr2 + 1, itr3 - itr2 - 1));
        unsigned int node = std::stoul(value.substr(itr3 + 1), nullptr, 16);

        return SiegePos(x, y, z, node);
    }
}

static std::ostream& operator << (std::ostream& s, const ehb::SiegePos& pos)
{
    s << "SiegePos(x: " << pos.pos.x() << ", y: " << pos.pos.y() << ", z: " << pos.pos.z() << ", guid: " << pos.guid << ")";
    return s;
}

namespace ehb
{
    // since we are using software instancing we have to keep track of the guids of handled nodes as to not keep flipping things on and off
    class ToggleRegionLogicalFlags : public osg::NodeVisitor
    {

        std::set<uint32_t> unique;

    public:

        ToggleRegionLogicalFlags() : NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN)
        {

        }

        virtual void apply(osg::Group& group) override
        {
            if (osg::MatrixTransform* xform = static_cast<osg::MatrixTransform*> (&group))
            {
                if (xform->getNumChildren() > 0)
                {
                    if (SiegeNodeMesh* mesh = dynamic_cast<SiegeNodeMesh*>(xform->getChild(0)))
                    {
                        uint32_t guid; xform->getUserValue("guid", guid);

                        if (unique.insert(guid).second)
                        {
                            mesh->toggleLogicalNodeFlags();
                        }
                    }

                    traverse(group);
                }
            }
        }
    };

    Player::Player(ContentDb& contentDb, osgViewer::Viewer& viewer)
    {
        auto log = spdlog::get("log");

        isFidgeting = true;
        fidgetAnimation = nullptr;
        walkAnimation = nullptr;
        animationManager = nullptr;

        if (auto go = contentDb.getGameObjectTmpl(tmplName))
        {
            auto model = go->valueOf("aspect:model");
            const auto chore_prefix = go->valueOf("body:chore_dictionary:chore_prefix");
            const auto chore_fidget = go->valueOf("body:chore_dictionary:chore_fidget:anim_files:00");
            auto chore_walk = go->valueOf("body:chore_dictionary:chore_walk:anim_files:00");

            // well this is a silly workaround
            if (chore_walk.empty()) chore_walk = go->valueOf("body:chore_dictionary:chore_walk:anim_files:05");

            if (model.empty() || chore_prefix.empty() || chore_fidget.empty())
            {
                log->critical("super bad. something is empty on reading template={}", tmplName);
                return;
            }

            if (mesh = static_cast<Aspect*>(osgDB::readNodeFile(model + ".asp")); mesh != nullptr)
            {

                transform = new osg::MatrixTransform;
                transform->addChild(mesh);

#if 1
                // TODO: is there a better way to do this?
                // re-position the camera based on the size of the mesh and orient it up a little bit get a birds eye-view
                if (auto manipulator = viewer.getCameraManipulator())
                {
                    double radius = osg::maximum(double(mesh->getBound().radius()), 1e-6);
                    double dist = 3.5f * radius;

                    // farmgril is facing away from the camer so invert our distance - trying to keep this as close to the siege node test state
                    manipulator->setHomePosition(mesh->getBound().center() + osg::Vec3d(0.0, -dist, 3.0f), mesh->getBound().center(), osg::Vec3d(0.0f, 0.0f, 1.0f));
                    manipulator->home(1);
                }
#endif
                // applying a skeleton messes with the bounding box size and expands it way more than it should
                mesh->applySkeleton();

                /* the below defines 9 stances for a template
                   the running theory is that 00 is a default fourcc code
                [chore_default]
                {
                    chore_stances = 0, 1, 2, 3, 4, 5, 6, 7, 8;
                    skrit = simple_loop;
                    [anim_files] { 00 = dff; }
                }
                */

                // i think 00 gets read as an integer which ends up choping the leading 0
                const auto fidgetFilename = chore_prefix + "0" + "_" + chore_fidget + ".prs";
                const auto walkFilename = chore_prefix + "0" + "_" + chore_walk + ".prs";

                log->info("retrieved '{}' from the contentdb. it has the model={}, chore_prefix={}, chore_fidget={}, fidgetFilename={} walkFilename={}", tmplName, model, chore_prefix, chore_fidget, fidgetFilename, walkFilename);

                fidgetAnimation = static_cast<osgAnimation::Animation*>(osgDB::readObjectFile(fidgetFilename));
                walkAnimation = static_cast<osgAnimation::Animation*>(osgDB::readObjectFile(walkFilename));

                if (fidgetAnimation != nullptr && walkAnimation != nullptr)
                {
                    log->info("loaded fidget and walk animations for use and durations are {} and {} respectively", fidgetAnimation->getDuration(), walkAnimation->getDuration());

                    animationManager = new osgAnimation::BasicAnimationManager;
                    fidgetAnimation->setPlayMode(osgAnimation::Animation::LOOP);
                    walkAnimation->setPlayMode(osgAnimation::Animation::LOOP);

                    animationManager->registerAnimation(fidgetAnimation);
                    animationManager->registerAnimation(walkAnimation);

                    // start our fidget
                    animationManager->playAnimation(fidgetAnimation);

                    mesh->setUpdateCallback(animationManager);
                }
                else
                {
                    log->critical("failed to load a fidget or walk animation");
                }
            }
        }
        else
        {
            log->error("failed to retrieve {} from the contentdb", tmplName);
        }

    }

    void UnveilingTestState::enter()
    {
        log = spdlog::get("log");
        // log->set_level(spdlog::level::debug);

        log->info("UnveilingTestState::enter()");

        // variablizing these out as it helps my brain think about the world class a bit more
        const std::string worldName = "multiplayer_world";
        const std::string regionName = "town_center";

        // man it would be really nice to convert these to fuel paths...
        const std::string worldPath = "/world/maps/" + worldName;
        const std::string regionPath = worldPath + "/regions/" + regionName;

        // man it would be really nice to convert these to fuel paths...
        const std::string nodesDotGas = regionPath + "/terrain_nodes/nodes.gas";
        const std::string objectsPath = regionPath + "/objects/regular/";

        region = static_cast<Region*> (osgDB::readNodeFile(nodesDotGas));

        const osg::MatrixTransform* targetNodeXform = region->targetNode();
        uint32_t targetNodeGuid = region->targetNodeGuid();

        // TODO: is there a better way to do this?
        // re-position the camera based on the size of the target node and orient it up a little bit get a birds eye-view
        if (auto manipulator = viewer.getCameraManipulator())
        {
            if (const SiegeNodeMesh* mesh = static_cast<const SiegeNodeMesh*>(targetNodeXform->getChild(0)))
            {
                double radius = osg::maximum(double(mesh->getBound().radius()), 1e-6);
                double dist = 7.f * radius;

                manipulator->setHomePosition(mesh->getBound().center() + osg::Vec3d(0.0, -dist, 15.0f), mesh->getBound().center(), osg::Vec3d(0.0f, 0.0f, 1.0f));
                manipulator->home(1);
            }
            else
            {
                log->error("failed to find siegenodemesh attached to transform with guid: {}", targetNodeGuid);
            }
        }

        localPlayer.reset(new Player(contentDb, viewer));

        SiegePos position(0, 0, 0, targetNodeGuid);
        if (auto localNode = region->transformForGuid(position.guid); localNode != nullptr)
        {
            osg::Matrix copy = localNode->getMatrix();
            copy.preMultTranslate(position.pos);
            localPlayer->transform->setMatrix(copy);
        }

        region->addChild(localPlayer->transform);

        osgUtil::Optimizer optimizer;
        optimizer.optimize(region);

        scene.addChild(region);
    }

    void UnveilingTestState::leave()
    {
    }

    void UnveilingTestState::update(double deltaTime)
    {
    }

    bool UnveilingTestState::handle(const osgGA::GUIEventAdapter& event, osgGA::GUIActionAdapter& action)
    {
        if (event.getEventType() == osgGA::GUIEventAdapter::PUSH)
        {
            if (event.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            {
                if (osgViewer::View* viewer = static_cast<osgViewer::View*> (&action))
                {
                    // viewer always has a valid camera no need for a check
                    osg::Camera* camera = viewer->getCamera();

                    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector = new osgUtil::LineSegmentIntersector(osgUtil::Intersector::WINDOW, event.getX(), event.getY());
                    osgUtil::IntersectionVisitor visitor(intersector);
                    camera->accept(visitor);

                    if (intersector->containsIntersections())
                    {
                        for (auto&& intersection : intersector->getIntersections())
                        {
                            for (auto node : intersection.nodePath)
                            {
                                if (uint32_t nodeGuid; node->getUserValue("guid", nodeGuid))
                                {
                                    if (auto nodeXform = dynamic_cast<osg::MatrixTransform*> (node))
                                    {
                                        log->info("node you clicked was: 0x{:x}", nodeGuid);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (event.getEventType() == osgGA::GUIEventAdapter::KEYUP)
        {
            if (event.getKey() == '2')
            {
                log->info("enabling logical flags");

                ToggleRegionLogicalFlags visitor;
                region->accept(visitor);

                static bool runOnce = false;

                if (!runOnce)
                {
                    osgUtil::Optimizer optimizer;
                    optimizer.optimize(region);

                    runOnce = true;
                }
            }
        }

        return false;
    }
}
