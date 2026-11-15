#include "PicturePreview.h"

#include <wx/filedlg.h>

#if defined(__WXGTK3__)

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include <gtk/gtk.h>

#include "Pictures.h"

namespace
{

// The pane's width, and the longest side a preview is drawn at.
constexpr int PREVIEW_SIDE = 220;

// Files bigger than this aren't read for a preview: loadPicture reads the
// whole file into memory first.
constexpr long long PREVIEW_MAX_FILE = 200LL * 1024 * 1024;

std::string sizeText(long long bytes)
{
    char text[32];
    if (bytes < 1000)
        std::snprintf(text, sizeof text, "%lld bytes", bytes);
    else if (bytes < 1000 * 1000)
        std::snprintf(text, sizeof text, "%.0f kB", bytes / 1000.0);
    else
        std::snprintf(text, sizeof text, "%.1f MB", bytes / 1e6);
    return text;
}

} // namespace

struct PicturePreview::State
{
    // Only the window's thread touches these.
    GtkFileChooser* chooser = nullptr;
    GtkWidget* image = nullptr;
    GtkWidget* label = nullptr;
    gulong handler = 0;

    std::mutex mutex;
    bool alive = true;   // false once the dialog has closed
    bool busy = false;   // a reader thread is running
    std::string wanted;  // the file the chooser is on ("" for none)
    std::weak_ptr<State> self; // for the reader threads to hold
};

namespace
{

using State = PicturePreview::State;

// What a reader thread made of one file, handed to the window's thread.
struct Shown
{
    std::shared_ptr<State> state;
    std::string path;
    bool ok = false;
    int width = 0, height = 0;    // the preview's
    std::vector<uint8_t> rgb;     // width × height × 3
    std::string caption;
};

void read(const std::string& path, Shown& out)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
    {
        out.caption = "It can't be read.";
        return;
    }
    if ((long long)st.st_size > PREVIEW_MAX_FILE)
    {
        out.caption = sizeText((long long)st.st_size) + ": too big to preview.";
        return;
    }
    ragtime::picture::Image picture;
    std::string error;
    if (!loadPicture(path, picture, error))
    {
        out.caption = error;
        return;
    }
    wxImage image = toWxImage(picture);
    int w = 0, h = 0;
    ragtime::picture::fitWithin(picture.width, picture.height, PREVIEW_SIDE, w, h);
    if (w != image.GetWidth() || h != image.GetHeight()) image = image.Scale(w, h, wxIMAGE_QUALITY_HIGH);
    out.width = image.GetWidth();
    out.height = image.GetHeight();
    const unsigned char* data = image.GetData();
    out.rgb.assign(data, data + (size_t)out.width * (size_t)out.height * 3);
    char dims[48];
    std::snprintf(dims, sizeof dims, "%d × %d, ", picture.width, picture.height);
    out.caption = dims + sizeText((long long)st.st_size);
    out.ok = true;
}

gboolean show(gpointer data)
{
    std::unique_ptr<Shown> shown(static_cast<Shown*>(data));
    State& s = *shown->state;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.alive || s.wanted != shown->path) return G_SOURCE_REMOVE;
    }
    if (shown->ok)
    {
        GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, shown->width, shown->height);
        if (pixbuf != nullptr)
        {
            const int stride = gdk_pixbuf_get_rowstride(pixbuf);
            guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
            for (int y = 0; y < shown->height; y++)
                std::memcpy(pixels + (size_t)y * (size_t)stride, shown->rgb.data() + (size_t)y * shown->width * 3,
                            (size_t)shown->width * 3);
            gtk_image_set_from_pixbuf(GTK_IMAGE(s.image), pixbuf);
            g_object_unref(pixbuf);
        }
    }
    else
    {
        gtk_image_clear(GTK_IMAGE(s.image));
    }
    gtk_widget_set_opacity(s.image, 1.0);
    gtk_label_set_text(GTK_LABEL(s.label), shown->caption.c_str());
    gtk_file_chooser_set_preview_widget_active(s.chooser, TRUE);
    return G_SOURCE_REMOVE;
}

// GTK opens its chooser at the size it was last closed at or, the first
// time, at a size of its own: 822 pixels high with the filter's own row,
// too tall for a 1366 × 768 screen, where Open and Cancel fell off the
// bottom. A largest size, the screen less room for the title bar, holds it
// in whatever GTK asks for later.
void fitScreen(GtkWidget* widget, gpointer)
{
    GdkWindow* window = gtk_widget_get_window(widget);
    if (window == nullptr) return;
    GdkMonitor* monitor = gdk_display_get_monitor_at_window(gdk_window_get_display(window), window);
    if (monitor == nullptr) return;
    GdkRectangle area;
    gdk_monitor_get_workarea(monitor, &area);
    GdkGeometry geometry;
    std::memset(&geometry, 0, sizeof geometry);
    geometry.max_width = area.width;
    geometry.max_height = std::max(area.height - 64, 400);
    gtk_window_set_geometry_hints(GTK_WINDOW(widget), nullptr, &geometry, GDK_HINT_MAX_SIZE);
}

// Reads files one at a time until it has read the one the chooser is on.
void work(std::shared_ptr<State> state, std::string path)
{
    for (;;)
    {
        std::unique_ptr<Shown> shown(new Shown);
        shown->state = state;
        shown->path = path;
        read(path, *shown);
        g_idle_add(show, shown.release());
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->alive || state->wanted.empty() || state->wanted == path)
        {
            state->busy = false;
            return;
        }
        path = state->wanted;
    }
}

void onUpdatePreview(GtkFileChooser* chooser, gpointer data)
{
    State* s = static_cast<State*>(data);
    gchar* name = gtk_file_chooser_get_preview_filename(chooser);
    std::string path = name != nullptr ? name : "";
    g_free(name);
    if (!path.empty() && g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) path.clear();

    bool start = false;
    {
        std::lock_guard<std::mutex> lock(s->mutex);
        if (path == s->wanted) return;
        s->wanted = path;
        if (!path.empty() && !s->busy) s->busy = start = true;
    }
    if (path.empty())
    {
        gtk_file_chooser_set_preview_widget_active(chooser, FALSE);
        return;
    }
    // The last picture stays, dimmed, until this one is read.
    gtk_widget_set_opacity(s->image, 0.35);
    if (start)
    {
        // The reader holds the state, so it may outlive the dialog.
        std::shared_ptr<State> shared = s->self.lock();
        if (shared) std::thread(work, shared, path).detach();
    }
}

} // namespace

#endif

PicturePreview::PicturePreview(wxFileDialog& dialog)
{
#if defined(__WXGTK3__)
    GtkWidget* widget = static_cast<GtkWidget*>(dialog.GetHandle());
    if (widget == nullptr || !GTK_IS_FILE_CHOOSER(widget)) return;
    addPictureReadHandlers(); // here, before a reader thread wants them
    state_ = std::make_shared<State>();
    state_->self = state_;
    State& s = *state_;
    s.chooser = GTK_FILE_CHOOSER(widget);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(box, PREVIEW_SIDE, -1);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    s.image = gtk_image_new();
    s.label = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(s.label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(s.label), 24);
    gtk_label_set_justify(GTK_LABEL(s.label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box), s.image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), s.label, FALSE, FALSE, 0);
    gtk_widget_show_all(box);

    gtk_file_chooser_set_preview_widget(s.chooser, box);
    gtk_file_chooser_set_use_preview_label(s.chooser, FALSE);
    gtk_file_chooser_set_preview_widget_active(s.chooser, FALSE);
    s.handler = g_signal_connect(s.chooser, "update-preview", G_CALLBACK(onUpdatePreview), &s);
    g_signal_connect_after(widget, "realize", G_CALLBACK(fitScreen), nullptr);
#else
    (void)dialog;
#endif
}

PicturePreview::~PicturePreview()
{
#if defined(__WXGTK3__)
    if (!state_) return;
    if (state_->handler != 0) g_signal_handler_disconnect(state_->chooser, state_->handler);
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->alive = false;
#endif
}
