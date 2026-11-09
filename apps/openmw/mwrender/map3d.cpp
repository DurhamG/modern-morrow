#include "map3d.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <osg/Group>
#include <osg/PolygonMode>
#include <osg/Texture2D>
#include <osg/Uniform>

#include <components/myguiplatform/myguitexture.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/fog.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/rtt.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/stereo/multiview.hpp>
#include <components/terrain/quadtreeworld.hpp>

#include "util.hpp"
#include "vismask.hpp"

namespace MWRender
{
    namespace
    {
        // The camera's vertical field of view, and how far it tilts down from level, in degrees.
        constexpr double sFieldOfView = 45.0;
        constexpr double sPitch = 55.0;
    }

    class Map3DRenderToTexture : public SceneUtil::RTTNode
    {
    public:
        Map3DRenderToTexture(osg::Node* sceneRoot, int width, int height)
            : RTTNode(width, height, 0, false, 0, StereoAwareness::Unaware_MultiViewShaders,
                  shouldAddMSAAIntermediateTarget())
            , mSceneRoot(sceneRoot)
            , mAspect(static_cast<double>(width) / height)
            , mDetail(new Terrain::CameraDetail)
            , mProjectionUniform(new osg::Uniform("projectionMatrix", osg::Matrixf()))
        {
            setNodeMask(0);
            setDepthBufferInternalFormat(GL_DEPTH24_STENCIL8);
        }

        void setView(const osg::Vec3f& focus, float distance)
        {
            const double pitch = osg::DegreesToRadians(sPitch);
            const osg::Vec3d eye
                = osg::Vec3d(focus) + osg::Vec3d(0.0, -std::cos(pitch) * distance, std::sin(pitch) * distance);
            mViewMatrix.makeLookAt(eye, osg::Vec3d(focus), osg::Vec3d(0.0, 0.0, 1.0));

            // The near plane moves out as the camera does, so the depth buffer keeps its precision. The far plane
            // reaches past the top of the picture, which is the farthest ground in view.
            const double nearPlane = std::max(10.0, distance * 0.05);
            const double farPlane = distance * 6.0 + 16384.0;
            mPlainProjection.makePerspective(sFieldOfView, mAspect, nearPlane, farPlane);
            mProjectionMatrix = SceneUtil::AutoDepth::isReversed()
                ? SceneUtil::getReversedZProjectionMatrixAsPerspective(sFieldOfView, mAspect, nearPlane, farPlane)
                : mPlainProjection;
            // Everything in view gets the detail of the point the camera looks at, so panning doesn't change it.
            mDetail->mViewDistance = static_cast<float>(farPlane);
            mDetail->mDetailDistance = distance;
        }

        bool worldToPicture(const osg::Vec3f& position, osg::Vec2f& picture) const
        {
            const osg::Vec4d clip = osg::Vec4d(position, 1.0) * mViewMatrix * mPlainProjection;
            if (clip.w() <= 0.0)
                return false;
            picture.x() = static_cast<float>((clip.x() / clip.w() + 1.0) / 2.0);
            picture.y() = static_cast<float>(1.0 - (clip.y() / clip.w() + 1.0) / 2.0);
            return true;
        }

        void setDefaults(osg::Camera* camera) override
        {
            // Small feature culling would drop distant buildings, which a map needs.
            camera->setCullingMode(
                (osg::Camera::DEFAULT_CULLING | osg::Camera::FAR_PLANE_CULLING) & ~osg::Camera::SMALL_FEATURE_CULLING);
            SceneUtil::setCameraClearDepth(camera);
            camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
            // The camera's own position decides the terrain's detail, rather than the player's.
            camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT, osg::Camera::PIXEL_BUFFER_RTT);
            camera->setClearColor(osg::Vec4(0.03f, 0.03f, 0.04f, 1.f));
            camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            camera->setRenderOrder(osg::Camera::PRE_RENDER);
            const unsigned int mask = Mask_Scene | Mask_SimpleWater | Mask_Terrain | Mask_Object | Mask_Static;
            camera->setCullMask(mask);
            camera->setCullMaskLeft(mask);
            camera->setCullMaskRight(mask);
            camera->setNodeMask(Mask_RenderToTexture);
            camera->setUserData(mDetail);

            osg::StateSet* stateset = camera->getOrCreateStateSet();
            stateset->setAttribute(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL),
                osg::StateAttribute::OVERRIDE);
            stateset->addUniform(mProjectionUniform, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

            // No fog, and no fading into the sky at the far edge.
            SceneUtil::disableFog(*stateset, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
            stateset->addUniform(new osg::Uniform("far", 10000000.0f));
            stateset->addUniform(new osg::Uniform("skyBlendingStart", 8000000.0f));
            stateset->addUniform(
                new osg::Uniform("screenRes", osg::Vec2f(static_cast<float>(width()), static_cast<float>(height()))));

            // The same fixed light as the flat local map, so the time of day doesn't matter.
            SceneUtil::Light light;
            light.setPosition(osg::Vec4(-0.3f, -0.3f, 0.7f, 0.f));
            light.setDiffuse(osg::Vec4(0.7f, 0.7f, 0.7f, 1.f));
            light.setAmbient(osg::Vec4(0.35f, 0.35f, 0.35f, 1.f));
            light.setSpecular(osg::Vec4(0, 0, 0, 0));
            light.setConstantAttenuation(1.f);
            light.setLinearAttenuation(0.f);
            light.setQuadraticAttenuation(0.f);
            SceneUtil::ShadowManager::instance().disableShadowsForStateSet(*stateset);
            SceneUtil::configureStateSetSunOverride(&light, stateset);

            camera->addChild(mSceneRoot);
        }

        void apply(osg::Camera* camera) override
        {
            camera->setViewMatrix(mViewMatrix);
            camera->setProjectionMatrix(mProjectionMatrix);
            mProjectionUniform->set(static_cast<osg::Matrixf>(mProjectionMatrix));
            if (Stereo::getMultiview())
                Stereo::setMultiviewMatrices(camera->getOrCreateStateSet(), { mProjectionMatrix, mProjectionMatrix });
        }

    private:
        osg::ref_ptr<osg::Node> mSceneRoot;
        double mAspect;
        osg::Matrixd mViewMatrix;
        osg::Matrixd mProjectionMatrix;
        // The same projection with a normal depth range, for working out where things show in the picture.
        osg::Matrixd mPlainProjection;
        osg::ref_ptr<Terrain::CameraDetail> mDetail;
        osg::ref_ptr<osg::Uniform> mProjectionUniform;
    };

    Map3D::Map3D(osg::Group* root, int width, int height)
        : mRoot(root)
        , mWidth(width)
        , mHeight(height)
    {
        SceneUtil::FindByNameVisitor find("Scene Root");
        mRoot->accept(find);
        if (find.mFoundNode == nullptr)
            throw std::runtime_error("no scene root found");

        mRtt = new Map3DRenderToTexture(find.mFoundNode, width, height);
        mRoot->addChild(mRtt);
    }

    Map3D::~Map3D()
    {
        mRoot->removeChild(mRtt);
    }

    void Map3D::setShown(bool shown)
    {
        mRtt->setNodeMask(shown ? Mask_RenderToTexture : 0);
    }

    void Map3D::setView(const osg::Vec3f& focus, float distance)
    {
        mRtt->setView(focus, distance);
    }

    std::unique_ptr<MyGUI::ITexture> Map3D::makeGuiTexture() const
    {
        return std::make_unique<MyGUIPlatform::OSGTexture>(
            static_cast<osg::Texture2D*>(mRtt->getColorTexture(nullptr)));
    }

    bool Map3D::worldToPicture(const osg::Vec3f& position, osg::Vec2f& picture) const
    {
        return mRtt->worldToPicture(position, picture);
    }
}
