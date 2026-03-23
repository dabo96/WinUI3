#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <functional>

namespace FluentUI {

/// File filter for open/save dialogs (e.g., {"Images", "png;jpg;bmp"}).
struct FileFilter {
    std::string name;     ///< User-readable label (e.g., "Image files")
    std::string pattern;  ///< Semicolon-separated extensions (e.g., "png;jpg") or "*" for all
};

/// Callback invoked when the user selects files or cancels the dialog.
/// @param paths  Selected file paths (empty if cancelled).
/// @param filterIndex  Index of the filter the user chose (-1 if N/A).
using FileDialogCallback = std::function<void(const std::vector<std::string>& paths, int filterIndex)>;

/// Show a native "Open File" dialog (non-blocking).
/// @param window       Parent window (may be nullptr).
/// @param filters      File type filters.
/// @param defaultPath  Initial directory or file path (empty = OS default).
/// @param allowMany    Allow selecting multiple files.
/// @param callback     Called with results (may be called from another thread).
void ShowOpenFileDialog(SDL_Window* window,
                        const std::vector<FileFilter>& filters,
                        const std::string& defaultPath,
                        bool allowMany,
                        FileDialogCallback callback);

/// Show a native "Save File" dialog (non-blocking).
void ShowSaveFileDialog(SDL_Window* window,
                        const std::vector<FileFilter>& filters,
                        const std::string& defaultPath,
                        FileDialogCallback callback);

/// Show a native "Open Folder" dialog (non-blocking).
void ShowOpenFolderDialog(SDL_Window* window,
                          const std::string& defaultPath,
                          bool allowMany,
                          FileDialogCallback callback);

} // namespace FluentUI
