#ifndef OPENMW_MWAUTOMATION_SCREENSHOT_H
#define OPENMW_MWAUTOMATION_SCREENSHOT_H

#include <filesystem>
#include <optional>

#include <osg/ref_ptr>
#include <osgViewer/ViewerEventHandlers>

namespace osgViewer
{
    class Viewer;
}

namespace MWAutomation
{
    class MemoryCaptureOperation;

    struct CropRect
    {
        int mX = 0;
        int mY = 0;
        int mWidth = 0;
        int mHeight = 0;
    };

    struct ScreenshotRequest
    {
        std::filesystem::path mPath;
        // In window pixels, measured from the top left corner.
        std::optional<CropRect> mCrop;
        // Applied after cropping. 1 keeps the full size.
        float mScale = 1;
    };

    struct ScreenshotResult
    {
        std::filesystem::path mPath;
        int mWidth;
        int mHeight;
    };

    // Captures the final image of a frame, including the UI, and writes it to a chosen file.
    //
    // It borrows the engine's screen capture handler, which is also used by the screenshot key. While a capture is
    // pending, the handler's output goes to memory instead of to the screenshots folder. After the frame is captured,
    // the handler's output goes back to the screenshots folder.
    class Screenshotter
    {
    public:
        // defaultOperation is the handler's normal output, which is restored after each capture. It has to be passed
        // in, because ScreenCaptureHandler::getCaptureOperation() crashes before the handler's first capture.
        Screenshotter(osgViewer::Viewer& viewer, osgViewer::ScreenCaptureHandler& handler,
            osgViewer::ScreenCaptureHandler::CaptureOperation& defaultOperation);
        ~Screenshotter();

        bool isBusy() const { return mRequest.has_value(); }

        void start(ScreenshotRequest request);

        // Returns the result once the frame has been captured and written. Throws std::runtime_error if writing
        // fails.
        std::optional<ScreenshotResult> poll();

        // Gives up on the pending capture.
        void cancel();

    private:
        osgViewer::Viewer& mViewer;
        osgViewer::ScreenCaptureHandler& mHandler;
        osg::ref_ptr<MemoryCaptureOperation> mOperation;
        osg::ref_ptr<osgViewer::ScreenCaptureHandler::CaptureOperation> mDefaultOperation;
        std::optional<ScreenshotRequest> mRequest;

        void restoreDefaultOperation();
    };
}

#endif
