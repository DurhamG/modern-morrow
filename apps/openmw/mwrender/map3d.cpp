#include "map3d.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/PolygonMode>
#include <osg/Program>
#include <osg/Shader>
#include <osg/Texture2D>
#include <osg/Uniform>

#include <components/myguiplatform/myguitexture.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/fog.hpp>
#include <components/sceneutil/lightmanager.hpp>
#include <components/sceneutil/rtt.hpp>
#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/workqueue.hpp>
#include <components/stereo/multiview.hpp>
#include <components/terrain/quadtreeworld.hpp>
#include <components/terrain/view.hpp>

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

            // The pieces reach well past the top of the picture, so a short pan still finds them there. Everything
            // in view gets the detail of the point the camera looks at, so panning doesn't change it.
            mWantedPoint = osg::Vec3f(eye);
            mWantedViewDistance = distance * 3.f + 8192.f;
            mWantedDetailDistance = distance;
        }

        /// The pieces of land the camera should draw from where it is now: around which point, how far out, and at
        /// what detail.
        const osg::Vec3f& getWantedPoint() const { return mWantedPoint; }
        float getWantedViewDistance() const { return mWantedViewDistance; }
        float getWantedDetailDistance() const { return mWantedDetailDistance; }

        /// Which pieces of land the camera draws. Map3D changes it once the pieces are built.
        Terrain::CameraDetail& getDetail() { return *mDetail; }

        /// Turns a point of the picture, from -1 to 1 across and up with the depth buffer's depth, into a world
        /// position.
        osg::Matrixd getPictureToWorld() const { return osg::Matrixd::inverse(mViewMatrix * mProjectionMatrix); }

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
            // The same colour as the fog of war, so the unexplored land and the empty space around it look alike.
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
        osg::Vec3f mWantedPoint;
        float mWantedViewDistance = 0.f;
        float mWantedDetailDistance = 0.f;
    };

    /// Builds the pieces of land for one place and zoom of the 3D view, on a background thread.
    class Map3DPreload : public SceneUtil::WorkItem
    {
    public:
        Map3DPreload(Terrain::QuadTreeWorld* terrain, Terrain::View* view, const osg::Vec3f& point,
            const osg::Vec4i& grid, float viewDistance, float detailDistance)
            : mPoint(point)
            , mViewDistance(viewDistance)
            , mDetailDistance(detailDistance)
            , mTerrain(terrain)
            , mView(view)
            , mGrid(grid)
        {
        }

        void doWork() override
        {
            mTerrain->preloadForCamera(mView, mPoint, mGrid, mViewDistance, mDetailDistance, mAbort);
        }

        void abort() override { mAbort = true; }

        const osg::Vec3f mPoint;
        const float mViewDistance;
        const float mDetailDistance;

    private:
        Terrain::QuadTreeWorld* mTerrain;
        Terrain::View* mView;
        const osg::Vec4i mGrid;
        std::atomic<bool> mAbort{ false };
    };

    namespace
    {
        constexpr char sFogVertexShader[] = R"GLSL(
            #version 120
            varying vec2 uv;
            void main()
            {
                gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
                uv = gl_MultiTexCoord0.xy;
            }
        )GLSL";

        // Works out where each pixel of the 3D view is in the world from its depth, then looks up how explored that
        // place is. Unexplored places take the colour of the empty background, so nothing of them shows. The mask is
        // soft at the edge of what was explored, and the fog becomes solid a short way into that soft edge.
        constexpr char sFogFragmentShader[] = R"GLSL(
            #version 120
            uniform sampler2D sceneColor;
            uniform sampler2D sceneDepth;
            uniform sampler2D fog;
            uniform mat4 pictureToWorld;
            uniform bool reversedDepth;
            uniform bool fogShown;
            uniform vec2 fogSouthWest;
            uniform float fogSize;
            const vec3 fogColor = vec3(0.03, 0.03, 0.04);
            varying vec2 uv;
            void main()
            {
                vec4 color = texture2D(sceneColor, uv);
                float depth = texture2D(sceneDepth, uv).r;
                bool background = reversedDepth ? depth <= 0.0 : depth >= 1.0;
                if (!fogShown || background)
                {
                    gl_FragColor = vec4(color.rgb, 1.0);
                    return;
                }
                float ndcDepth = reversedDepth ? depth : depth * 2.0 - 1.0;
                vec4 world = pictureToWorld * vec4(uv * 2.0 - 1.0, ndcDepth, 1.0);
                world /= world.w;
                float unexplored = texture2D(fog, (world.xy - fogSouthWest) / fogSize).r;
                float hidden = smoothstep(0.35, 0.75, unexplored);
                gl_FragColor = vec4(mix(color.rgb, fogColor, hidden), 1.0);
            }
        )GLSL";
    }

    /// Draws the 3D view's picture again into a texture of its own, darkening what the player hasn't explored. It
    /// draws after the 3D view, every frame while the view shows.
    class Map3DFogPass : public SceneUtil::RTTNode
    {
    public:
        Map3DFogPass(Map3DRenderToTexture* scene, int width, int height)
            : RTTNode(width, height, 0, false, 1, StereoAwareness::Unaware, false)
            , mScene(scene)
            , mPictureToWorld(new osg::Uniform("pictureToWorld", osg::Matrixf()))
            , mFogShown(new osg::Uniform("fogShown", false))
            , mFogSouthWest(new osg::Uniform("fogSouthWest", osg::Vec2f()))
            , mFogSize(new osg::Uniform("fogSize", 1.f))
            , mFogTexture(new osg::Texture2D)
        {
            setNodeMask(0);
            mFogTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            mFogTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            mFogTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_BORDER);
            mFogTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_BORDER);
            mFogTexture->setBorderColor(osg::Vec4d(1.0, 1.0, 1.0, 1.0));
            mFogTexture->setResizeNonPowerOfTwoHint(false);
        }

        void setFog(osg::Image* image, const osg::Vec2f& southWest, float size)
        {
            mFogShown->set(image != nullptr);
            if (image == nullptr)
                return;
            mFogTexture->setImage(image);
            mFogSouthWest->set(southWest);
            mFogSize->set(size);
        }

        void setDefaults(osg::Camera* camera) override
        {
            camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
            camera->setProjectionMatrix(osg::Matrix::identity());
            camera->setViewMatrix(osg::Matrix::identity());
            camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT, osg::Camera::PIXEL_BUFFER_RTT);
            camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            camera->setNodeMask(Mask_RenderToTexture);

            // The pass reads the depth as it is, one value per pixel.
            osg::Texture* sceneDepth = mScene->getDepthTexture(nullptr);
            sceneDepth->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
            sceneDepth->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);

            // One quad covers the whole picture.
            osg::ref_ptr<osg::Geometry> quad = osg::createTexturedQuadGeometry(
                osg::Vec3(-1.f, -1.f, 0.f), osg::Vec3(2.f, 0.f, 0.f), osg::Vec3(0.f, 2.f, 0.f));
            quad->setCullingActive(false);
            camera->addChild(quad);

            const osg::StateAttribute::GLModeValue force
                = osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE | osg::StateAttribute::PROTECTED;
            const osg::StateAttribute::GLModeValue forceOff
                = osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE | osg::StateAttribute::PROTECTED;
            osg::StateSet* stateset = camera->getOrCreateStateSet();
            osg::ref_ptr<osg::Program> program = new osg::Program;
            program->addShader(new osg::Shader(osg::Shader::VERTEX, sFogVertexShader));
            program->addShader(new osg::Shader(osg::Shader::FRAGMENT, sFogFragmentShader));
            stateset->setAttributeAndModes(program, force);
            stateset->setMode(GL_DEPTH_TEST, forceOff);
            stateset->setMode(GL_BLEND, forceOff);
            stateset->setMode(GL_CULL_FACE, forceOff);
            stateset->setTextureAttributeAndModes(0, mScene->getColorTexture(nullptr), force);
            stateset->setTextureAttributeAndModes(1, sceneDepth, force);
            stateset->setTextureAttributeAndModes(2, mFogTexture, force);
            stateset->addUniform(new osg::Uniform("sceneColor", 0));
            stateset->addUniform(new osg::Uniform("sceneDepth", 1));
            stateset->addUniform(new osg::Uniform("fog", 2));
            stateset->addUniform(new osg::Uniform("reversedDepth", SceneUtil::AutoDepth::isReversed()));
            stateset->addUniform(mPictureToWorld);
            stateset->addUniform(mFogShown);
            stateset->addUniform(mFogSouthWest);
            stateset->addUniform(mFogSize);
        }

        void apply(osg::Camera* camera) override
        {
            mPictureToWorld->set(static_cast<osg::Matrixf>(mScene->getPictureToWorld()));
        }

    private:
        osg::ref_ptr<Map3DRenderToTexture> mScene;
        osg::ref_ptr<osg::Uniform> mPictureToWorld;
        osg::ref_ptr<osg::Uniform> mFogShown;
        osg::ref_ptr<osg::Uniform> mFogSouthWest;
        osg::ref_ptr<osg::Uniform> mFogSize;
        osg::ref_ptr<osg::Texture2D> mFogTexture;
    };

    Map3D::Map3D(osg::Group* root, Terrain::World* terrain, std::shared_ptr<SceneUtil::WorkQueue> workQueue, int width,
        int height)
        : mRoot(root)
        , mTerrain(dynamic_cast<Terrain::QuadTreeWorld*>(terrain))
        , mWorkQueue(std::move(workQueue))
        , mWidth(width)
        , mHeight(height)
    {
        if (mTerrain != nullptr)
            mPreloadView = mTerrain->createView();

        SceneUtil::FindByNameVisitor find("Scene Root");
        mRoot->accept(find);
        if (find.mFoundNode == nullptr)
            throw std::runtime_error("no scene root found");

        mRtt = new Map3DRenderToTexture(find.mFoundNode, width, height);
        mRoot->addChild(mRtt);
        mFogPass = new Map3DFogPass(mRtt, width, height);
        mRoot->addChild(mFogPass);
    }

    Map3D::~Map3D()
    {
        // The background thread uses the view this owns.
        if (mPreload != nullptr)
        {
            mPreload->abort();
            mPreload->waitTillDone();
        }

        mRoot->removeChild(mFogPass);
        mRoot->removeChild(mRtt);
    }

    void Map3D::setShown(bool shown)
    {
        mRtt->setNodeMask(shown ? Mask_RenderToTexture : 0);
        mFogPass->setNodeMask(shown ? Mask_RenderToTexture : 0);
        // The view usually opens somewhere else, so its first frame builds the pieces it needs while it draws. Its old
        // pieces would show the wrong place until the new ones are built.
        if (shown)
            mRtt->getDetail().mSelectionPoint.reset();
    }

    void Map3D::setFog(osg::Image* image, const osg::Vec2f& southWest, float size)
    {
        mFogPass->setFog(image, southWest, size);
    }

    void Map3D::setView(const osg::Vec3f& focus, float distance)
    {
        mRtt->setView(focus, distance);
        updateTerrainPieces();
    }

    void Map3D::updateTerrainPieces()
    {
        Terrain::CameraDetail& detail = mRtt->getDetail();
        const osg::Vec3f& point = mRtt->getWantedPoint();
        const float viewDistance = mRtt->getWantedViewDistance();
        const float detailDistance = mRtt->getWantedDetailDistance();

        // Without distant terrain, or for the first frame, the camera builds what it draws as it draws.
        if (mTerrain == nullptr || !detail.mSelectionPoint.has_value())
        {
            detail.mSelectionPoint = point;
            detail.mViewDistance = viewDistance;
            detail.mDetailDistance = detailDistance;
            return;
        }

        if (mPreload != nullptr)
        {
            if (!mPreload->isDone())
                return;
            // The pieces are built, so drawing them now only finds them in the caches.
            detail.mSelectionPoint = mPreload->mPoint;
            detail.mViewDistance = mPreload->mViewDistance;
            detail.mDetailDistance = mPreload->mDetailDistance;
            mPreload.reset();
        }

        // The terrain picks its pieces again once the point moves this far, so smaller moves need no new pieces.
        constexpr float moveBeforeUpdate = 150.f;
        if ((*detail.mSelectionPoint - point).length2() < moveBeforeUpdate * moveBeforeUpdate
            && detail.mViewDistance == viewDistance && detail.mDetailDistance == detailDistance)
            return;
        mPreload = std::make_shared<Map3DPreload>(
            mTerrain, mPreloadView.get(), point, mTerrain->getActiveGrid(), viewDistance, detailDistance);
        mWorkQueue->addWorkItem(mPreload);
    }

    std::unique_ptr<MyGUI::ITexture> Map3D::makeGuiTexture() const
    {
        return std::make_unique<MyGUIPlatform::OSGTexture>(
            static_cast<osg::Texture2D*>(mFogPass->getColorTexture(nullptr)));
    }

    bool Map3D::worldToPicture(const osg::Vec3f& position, osg::Vec2f& picture) const
    {
        return mRtt->worldToPicture(position, picture);
    }
}
