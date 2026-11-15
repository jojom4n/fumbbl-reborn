#pragma once

#include <QString>

// The per-user save root: every file the app persists (match archives,
// credentials, ...) lives under it.
//   Windows  : %USERPROFILE%\Saved Games\FUMBBL Reborn
//   otherwise: ~/.config/fumbbl-reborn
QString appSaveRoot();
