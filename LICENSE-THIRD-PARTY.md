# Third-Party Notices and Licenses

This project incorporates components from third-party libraries. Their
copyright notices and license terms are listed below.

---

## Qt 6 Framework

* **Project:** Qt Framework (version 6.8.x)
* **Copyright:** Copyright (C) The Qt Company Ltd and other contributors.
* **License:** GNU Lesser General Public License v3.0 (LGPLv3)
* **License Text:** [https://www.gnu.org/licenses/lgpl-3.0.html](https://www.gnu.org/licenses/lgpl-3.0.html)

### Source Code & Re-linking

* **Official Repository:** The source code of the Qt 6 components used
  in this application can be found at
  [https://code.qt.io/cgit/qt/qtbase.git](https://code.qt.io/cgit/qt/qtbase.git/)
  or downloaded via the
  [Qt Downloads Archives](https://download.qt.io/official_releases/qt/).
* **Linking Notice:** This application links dynamically against
  unmodified Qt 6 libraries. In accordance with Section 4 of the
  LGPLv3, users are free to modify Qt 6 libraries and replace or
  re-link them with this application. Release bundles ship the
  unmodified Qt 6 shared libraries, which may be replaced by a
  compatible re-linked version.
* **Source Code Access:** If you need a direct copy of the exact Qt 6
  source code used during the compilation of any release, please open
  an issue in this repository or contact the project maintainers.

---

## lz-string

* **Project:** lz-string (version 1.5.0)
* **Copyright:** Copyright (C) 2014 Piero Xu (Pieroxy)
* **License:** WTFPL — [http://www.wtfpl.net/](http://www.wtfpl.net/)
* **Source:** [https://github.com/pieroxy/lz-string](https://github.com/pieroxy/lz-string)

This application contains a faithful C++ port of the UTF-16 codec
(`compressToUTF16` / `decompressFromUTF16`) in
`src/core/lzstring.cpp`, required by the FUMBBL WebSocket protocol
(every frame is LZString-UTF16 compressed). The original JavaScript
source and its WTFPL license apply to the ported code.
