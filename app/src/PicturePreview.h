#ifndef RAGTIME_PICTURE_PREVIEW_H
#define RAGTIME_PICTURE_PREVIEW_H

#include <memory>

class wxFileDialog;

// A preview of the picture under the cursor in a file dialog (Jeff,
// 2026-11-15): GTK's file chooser gets a pane on its right with the
// picture, its size in pixels and its size on disk, read with Ragtime's
// own loader (so JPEG 2000, WebP and HEIF show, turned the way their EXIF
// says) on a thread of its own, so moving through a folder of big photos
// never holds the dialog up. Make one before ShowModal() and keep it until
// the dialog closes; declared after the dialog, it goes first. Elsewhere
// (Windows has its own preview pane) it does nothing.
class PicturePreview
{
public:
    explicit PicturePreview(wxFileDialog& dialog);
    ~PicturePreview();

    PicturePreview(const PicturePreview&) = delete;
    PicturePreview& operator=(const PicturePreview&) = delete;

    struct State;

private:
    std::shared_ptr<State> state_;
};

#endif
