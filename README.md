# WordTsar

## This is a macOS-focused fork of [WordTsar](https://sourceforge.net/p/wordtsar/mercurial/ci/default/tree/), created by Gerald Brandt.

Wordstar for the 21st Century. WordTsar is a Wordstar 7.0D document mode clone. It loads Wordstar 4, Wordstar 7, RTF (partial), and DOCX (partial) files, and saves in Wordstar 7 and RTF format.

All credit for WordTsar's design and implementation goes to Gerald Brandt — this fork simply trims the project down to a macOS-only build. The original, cross-platform project (Windows, Linux, and macOS) lives at the link above and at [wordtsar.ca](http://wordtsar.ca); go there for the full story, the forums, and the Windows/Linux builds.

WordTsar is currently Alpha. What does Alpha mean? Alpha means the program works, but is feature incomplete.

This is version **0.5.1832 Alpha**, macOS only.

__BUILDING__

WordTsar requires Qt6, CMake 3.16+, and a C++20 compatible compiler. See [BUILDING.md](BUILDING.md) for full macOS build instructions.

__NOTES__

- A backup of your file is made every 1 minute. Backups are in Wordstar format.
- The initial page/paper size is 8.5" x 11"
- The 0.5.x releases use UTF8 for all in-memory storage of the document and supports Unicode version 16.

__Source code [GNU Affero General Public License v3.0](LICENSE.md).__
