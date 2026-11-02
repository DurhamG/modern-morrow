#include "screenshot.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <osg/Image>
#include <osgDB/ReaderWriter>
#include <osgDB/Registry>
#include <osgViewer/Viewer>

#include <components/files/conversion.hpp>
#include <components/misc/strings/lower.hpp>

namespace MWAutomation
{
    // Keeps the most recently captured frame. The capture runs on the draw thread, so access is locked.
    class MemoryCaptureOperation : public osgViewer::ScreenCaptureHandler::CaptureOperation
    {
    public:
        void operator()(const osg::Image& image, unsigned int /*contextId*/) override
        {
            osg::ref_ptr<osg::Image> copy = new osg::Image(image, osg::CopyOp::DEEP_COPY_ALL);
            const std::lock_guard lock(mMutex);
            mImage = std::move(copy);
        }

        osg::ref_ptr<osg::Image> take()
        {
            const std::lock_guard lock(mMutex);
            return std::exchange(mImage, nullptr);
        }

    private:
        std::mutex mMutex;
        osg::ref_ptr<osg::Image> mImage;
    };

    namespace
    {
        // Captured images have their first row at the bottom, as OpenGL returns them. The crop rectangle is measured
        // from the top, as window coordinates are.
        osg::ref_ptr<osg::Image> crop(const osg::Image& image, const CropRect& rect)
        {
            const int x = std::clamp(rect.mX, 0, image.s());
            const int y = std::clamp(rect.mY, 0, image.t());
            const int width = std::clamp(rect.mWidth, 0, image.s() - x);
            const int height = std::clamp(rect.mHeight, 0, image.t() - y);
            if (width == 0 || height == 0)
                throw std::runtime_error("the crop rectangle is outside the window");

            const unsigned pixelSize = image.getPixelSizeInBits() / 8;
            osg::ref_ptr<osg::Image> result = new osg::Image;
            result->allocateImage(width, height, 1, image.getPixelFormat(), image.getDataType());
            result->setOrigin(image.getOrigin());
            for (int row = 0; row < height; ++row)
            {
                // Row 0 of the result is the bottom row of the cropped area.
                const int sourceRow = image.t() - (y + height) + row;
                std::memcpy(result->data(0, row), image.data(x, sourceRow), width * pixelSize);
            }
            return result;
        }

        void write(const osg::Image& image, const std::filesystem::path& path)
        {
            std::string extension = Misc::StringUtils::lowerCase(Files::pathToUnicodeString(path.extension()));
            if (!extension.empty())
                extension.erase(0, 1);
            if (extension.empty())
                throw std::runtime_error("the screenshot path needs an extension such as .png");

            osgDB::ReaderWriter* writer = osgDB::Registry::instance()->getReaderWriterForExtension(extension);
            if (writer == nullptr)
                throw std::runtime_error("no image writer for ." + extension);

            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::binary);
            if (!stream)
                throw std::runtime_error("can't open " + Files::pathToUnicodeString(path) + " for writing");
            const osgDB::ReaderWriter::WriteResult result = writer->writeImage(image, stream);
            if (!result.success())
                throw std::runtime_error("writing the image failed: " + result.message());
        }
    }

    Screenshotter::Screenshotter(osgViewer::Viewer& viewer, osgViewer::ScreenCaptureHandler& handler,
        osgViewer::ScreenCaptureHandler::CaptureOperation& defaultOperation)
        : mViewer(viewer)
        , mHandler(handler)
        , mOperation(new MemoryCaptureOperation)
        , mDefaultOperation(&defaultOperation)
    {
    }

    Screenshotter::~Screenshotter()
    {
        cancel();
    }

    void Screenshotter::start(ScreenshotRequest request)
    {
        if (isBusy())
            throw std::runtime_error("another screenshot is still pending");
        mOperation->take();
        mHandler.setCaptureOperation(mOperation);
        mHandler.setFramesToCapture(1);
        mHandler.captureNextFrame(mViewer);
        mRequest = std::move(request);
    }

    std::optional<ScreenshotResult> Screenshotter::poll()
    {
        if (!mRequest)
            return std::nullopt;
        osg::ref_ptr<osg::Image> image = mOperation->take();
        if (image == nullptr)
            return std::nullopt;

        restoreDefaultOperation();
        const ScreenshotRequest request = *std::exchange(mRequest, std::nullopt);

        if (request.mCrop)
            image = crop(*image, *request.mCrop);
        if (request.mScale > 0 && request.mScale < 1)
        {
            const int width = std::max(1, static_cast<int>(image->s() * request.mScale));
            const int height = std::max(1, static_cast<int>(image->t() * request.mScale));
            image->scaleImage(width, height, 1);
        }
        write(*image, request.mPath);
        return ScreenshotResult{ request.mPath, image->s(), image->t() };
    }

    void Screenshotter::cancel()
    {
        if (mRequest)
        {
            mHandler.stopCapture();
            restoreDefaultOperation();
            mRequest.reset();
        }
    }

    void Screenshotter::restoreDefaultOperation()
    {
        mHandler.setCaptureOperation(mDefaultOperation);
    }
}
