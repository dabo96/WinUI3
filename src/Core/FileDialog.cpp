#include "core/FileDialog.h"
#include <SDL3/SDL_dialog.h>

namespace FluentUI {

// Internal callback data passed through SDL's void* userdata.
// Owns copies of all strings so they outlive the async dialog call.
struct DialogCallbackData {
    FileDialogCallback callback;
    SDL_Window* window;  // To restore viewport after dialog closes
    // Owned copies of filter strings (must stay alive until callback fires)
    std::vector<std::string> filterNames;
    std::vector<std::string> filterPatterns;
    // SDL filter structs pointing into the owned strings above
    std::vector<SDL_DialogFileFilter> sdlFilters;
    // Owned copy of default path
    std::string defaultPath;
};

// SDL callback — bridges to our C++ callback, then restores window state
static void SDLCALL sdlDialogCallback(void* userdata, const char* const* filelist, int filter) {
    auto* data = static_cast<DialogCallbackData*>(userdata);
    if (!data) return;

    std::vector<std::string> paths;
    if (filelist) {
        for (int i = 0; filelist[i] != nullptr; ++i) {
            paths.emplace_back(filelist[i]);
        }
    }

    if (data->callback) {
        data->callback(paths, filter);
    }

    // After the native dialog closes, the window may need a
    // resize/expose event so the renderer picks up the correct size.
    // Push a synthetic expose event to trigger a repaint.
    if (data->window) {
        SDL_Event ev{};
        ev.type = SDL_EVENT_WINDOW_EXPOSED;
        ev.window.windowID = SDL_GetWindowID(data->window);
        SDL_PushEvent(&ev);
    }

    delete data;
}

// Build a DialogCallbackData that owns all strings
static DialogCallbackData* MakeCallbackData(
        FileDialogCallback callback,
        SDL_Window* window,
        const std::vector<FileFilter>& filters,
        const std::string& defaultPath) {
    auto* data = new DialogCallbackData();
    data->callback = std::move(callback);
    data->window = window;
    data->defaultPath = defaultPath;

    // Copy filter strings so they outlive this scope
    data->filterNames.reserve(filters.size());
    data->filterPatterns.reserve(filters.size());
    data->sdlFilters.reserve(filters.size());
    for (const auto& f : filters) {
        data->filterNames.push_back(f.name);
        data->filterPatterns.push_back(f.pattern);
    }
    // Build SDL filter array pointing into our owned strings
    for (size_t i = 0; i < filters.size(); ++i) {
        data->sdlFilters.push_back({
            data->filterNames[i].c_str(),
            data->filterPatterns[i].c_str()
        });
    }
    return data;
}

void ShowOpenFileDialog(SDL_Window* window,
                        const std::vector<FileFilter>& filters,
                        const std::string& defaultPath,
                        bool allowMany,
                        FileDialogCallback callback) {
    auto* data = MakeCallbackData(std::move(callback), window, filters, defaultPath);

    SDL_ShowOpenFileDialog(
        sdlDialogCallback,
        data,
        window,
        data->sdlFilters.empty() ? nullptr : data->sdlFilters.data(),
        static_cast<int>(data->sdlFilters.size()),
        data->defaultPath.empty() ? nullptr : data->defaultPath.c_str(),
        allowMany
    );
}

void ShowSaveFileDialog(SDL_Window* window,
                        const std::vector<FileFilter>& filters,
                        const std::string& defaultPath,
                        FileDialogCallback callback) {
    auto* data = MakeCallbackData(std::move(callback), window, filters, defaultPath);

    SDL_ShowSaveFileDialog(
        sdlDialogCallback,
        data,
        window,
        data->sdlFilters.empty() ? nullptr : data->sdlFilters.data(),
        static_cast<int>(data->sdlFilters.size()),
        data->defaultPath.empty() ? nullptr : data->defaultPath.c_str()
    );
}

void ShowOpenFolderDialog(SDL_Window* window,
                          const std::string& defaultPath,
                          bool allowMany,
                          FileDialogCallback callback) {
    auto* data = MakeCallbackData(std::move(callback), window, {}, defaultPath);

    SDL_ShowOpenFolderDialog(
        sdlDialogCallback,
        data,
        window,
        data->defaultPath.empty() ? nullptr : data->defaultPath.c_str(),
        allowMany
    );
}

} // namespace FluentUI
