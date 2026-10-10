//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @file fontutils.cpp
 *
 * @brief Font descriptor serialization utilities for the GUI layer.
 *
 * Implements the FontUtils namespace functions that convert between QFont
 * objects and a pipe-delimited string descriptor format used throughout the
 * layout and rendering systems.
 *
 * @section fontutils_format Descriptor Format
 * The pipe-delimited format is: "Name|Size|Bold|Italic|Underline|Superscript|Subscript"
 * - Name: font family name (e.g., "Courier New", "Times New Roman")
 * - Size: point size as decimal string with 1 decimal place (e.g., "12.0", "8.5")
 * - Bold/Italic/Underline/Superscript/Subscript: "0" or "1"
 * - Example: "Courier New|12.0|0|0|0|0|0"
 *
 * @section fontutils_functions Conversion Functions
 * - ToDescriptor(QFont): creates a descriptor string from a QFont object,
 *   preserving fractional point sizes that QFont::toString() would lose
 * - FromDescriptor(string): creates a QFont from a descriptor string,
 *   setting family, point size, bold, italic, and underline properties
 * - GetFontName(string): extracts just the font family name from a descriptor
 * - GetFontSize(string): extracts just the point size from a descriptor
 *
 * @section fontutils_hidden macOS Hidden Families
 * macOS can leave installed, enabled families (Times, Courier, ...) out of its
 * available-font list. FromDescriptor registers such a family when a document
 * asks for it.
 *
 * @section fontutils_parity GUI/TUI Parity
 * The TUI layer (TUIFontUtils namespace) uses an identical descriptor format,
 * ensuring font descriptors can be shared between GUI and TUI codebases
 * via the common cLayoutBase and cLayoutState classes.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see FontUtils Font descriptor utility namespace
 * @see cLayout GUI layout engine using font descriptors
 * @see cQtTextMeasurement Qt measurement class using font descriptors
 */

#include "fontutils.h"
#ifdef __APPLE__
#include <QFontDatabase>
#include <QStringList>
#include <CoreText/CoreText.h>
#include <algorithm>
#include <climits>
#include <mutex>
#include <set>
#endif
#include <QString>
#include <sstream>
#include <iomanip>
#include <vector>

#ifdef __APPLE__
namespace
{

// Families already looked up by RegisterHiddenFamily (main thread only, like all font resolution)
std::set<QString> gCheckedFamilies;

// Font files RegisterFontFile has registered (startup thread and main thread)
std::mutex gRegisterMutex;
std::set<QString> gRegisteredFiles;


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - CoreText font descriptor
///
/// @return QString - the descriptor's family name, empty if it has none
///
/// @brief
/// Read the family name of a CoreText font descriptor.
///
/////////////////////////////////////////////////////////////////////////////
QString FamilyOf(CTFontDescriptorRef descriptor)
{
    QString family;
    CFStringRef name = static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute));
    if (name != nullptr)
    {
        family = QString::fromCFString(name);
        CFRelease(name);
    }
    return family;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  url [in] - file URL
///
/// @return QString - the file system path, empty if the URL has none
///
/// @brief
/// Convert a CoreText font file URL to a path Qt can open.
///
/////////////////////////////////////////////////////////////////////////////
QString PathOf(CFURLRef url)
{
    QString path;
    char buffer[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(buffer), sizeof(buffer)) == true)
    {
        path = QString::fromUtf8(buffer);
    }
    return path;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  file [in] - path of an installed font file
///
/// @return nothing
///
/// @brief
/// Register an installed font file with Qt as an application font, once:
/// Qt then resolves its families and lists them in QFontDatabase::families()
/// (QFontComboBox).
///
/// @note The lock is held while the file is registered (a few milliseconds),
/// so a family the main thread asks for is complete once this returns, even
/// when the startup thread was registering its file at the same time.
///
/////////////////////////////////////////////////////////////////////////////
void RegisterFontFile(const QString& file)
{
    std::lock_guard<std::mutex> lock(gRegisterMutex);
    if (gRegisteredFiles.insert(file).second == false)
    {
        return;
    }

    QFontDatabase::addApplicationFont(file);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  family [in] - font family name a document asks for
///
/// @return nothing
///
/// @brief
/// Make an installed family that macOS leaves out of its available-font list
/// usable by Qt. Qt builds its font list from CTFontManagerCopyAvailableFontFamilyNames,
/// which can omit installed, enabled families (seen on macOS 15: Times,
/// Courier, STIX, ... missing from every app's font list); Qt then substitutes
/// the first font of CoreText's character-fallback list (Courier -> Menlo).
/// CoreText still finds such a family by name, so its font files are
/// registered (RegisterFontFile). Each family is looked up once.
///
/////////////////////////////////////////////////////////////////////////////
void RegisterHiddenFamily(const QString& family)
{
    if (family.isEmpty() == true || family.startsWith('.') == true || gCheckedFamilies.count(family) != 0)
    {
        return;
    }
    gCheckedFamilies.insert(family);

    if (QFontDatabase::families().contains(family) == true)
    {
        return;
    }

    CFStringRef familyName = family.toCFString();
    const void* keys[] = { kCTFontFamilyNameAttribute };
    const void* values[] = { familyName };
    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(familyName);
    CTFontDescriptorRef request = CTFontDescriptorCreateWithAttributes(attributes);
    CFRelease(attributes);
    CFArrayRef matches = CTFontDescriptorCreateMatchingFontDescriptors(request, nullptr);
    CFRelease(request);
    if (matches == nullptr)
    {
        return;
    }

    // The files holding the family's faces (a .ttc holds several faces)
    std::set<QString> files;
    CFIndex count = CFArrayGetCount(matches);
    for (CFIndex index = 0; index < count; index++)
    {
        CTFontDescriptorRef match = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matches, index));
        CFURLRef url = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(match, kCTFontURLAttribute));
        if (url != nullptr)
        {
            QString path = PathOf(url);
            if (FamilyOf(match) == family && path.isEmpty() == false)
            {
                files.insert(path);
            }
            CFRelease(url);
        }
    }
    CFRelease(matches);

    std::for_each(files.begin(), files.end(), RegisterFontFile);
}


} // namespace
#endif

namespace FontUtils
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] - QFont to convert
///
/// @return std::string - Font descriptor string
///
/// @brief
/// Converts a QFont to our custom descriptor format.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// This replaces QFont::toString() which loses fractional font sizes (8.5, 10.5, etc.)
///
/////////////////////////////////////////////////////////////////////////////
std::string FontToDescriptor(const QFont& font)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    // Font family name
    ss << font.family().toStdString();
    ss << "|";

    // Font size (with 1 decimal place - preserves 8.5, 10.5, etc.)
    ss << font.pointSizeF();
    ss << "|";

    // Bold (weight >= 75)
    ss << (font.weight() >= QFont::Bold ? "1" : "0");
    ss << "|";

    // Italic
    ss << (font.italic() ? "1" : "0");
    ss << "|";

    // Underline
    ss << (font.underline() ? "1" : "0");
    ss << "|";

    // Superscript (placeholder - not in QFont, set by layout logic)
    ss << "0";
    ss << "|";

    // Subscript (placeholder - not in QFont, set by layout logic)
    ss << "0";

    return ss.str();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return QFont - Reconstructed font
///
/// @brief
/// Converts our custom font descriptor format back to QFont.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// This replaces QFont::fromString() which loses fractional font sizes.
///
/////////////////////////////////////////////////////////////////////////////
QFont FontFromDescriptor(const std::string& descriptor)
{
    QFont font;

    if (descriptor.empty())
    {
        return font;  // Return default font
    }

    // Split descriptor on pipe
    std::vector<std::string> fields;
    std::string field;
    for (char ch : descriptor)
    {
        if (ch == '|')
        {
            fields.push_back(field);
            field.clear();
        }
        else
        {
            field += ch;
        }
    }
    fields.push_back(field);  // Add last field

    // Need at least 5 fields (name, size, bold, italic, underline)
    if (fields.size() >= 5)
    {
        // Field [0] = font family name
        QString family = QString::fromStdString(fields[0]);
        font.setFamily(family);

        // Generic "Monospace": fall back to the platform's fixed-pitch font
        if (family.compare("Monospace", Qt::CaseInsensitive) == 0)
        {
            font.setStyleHint(QFont::TypeWriter);
#ifdef __APPLE__
            // Qt on macOS ignores the style hint for an unknown family, and
            // looking "Monospace" up costs a one-time font alias scan (about
            // 150 ms): use the system fixed-pitch family (Menlo) directly
            font.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
#endif
        }
#ifdef __APPLE__
        else
        {
            // Before Qt resolves the font: an installed family macOS does not list
            RegisterHiddenFamily(family);
        }
#endif

        // Field [1] = point size (as float with decimals!)
        try
        {
            double size = std::stod(fields[1]);
            font.setPointSizeF(size);
        }
        catch (...)
        {
            font.setPointSizeF(10.0);  // Default
        }

        // Field [2] = bold
        if (fields[2] == "1")
        {
            font.setWeight(QFont::Bold);
        }
        else
        {
            font.setWeight(QFont::Normal);
        }

        // Field [3] = italic
        font.setItalic(fields[3] == "1");

        // Field [4] = underline
        font.setUnderline(fields[4] == "1");

        // Fields [5] and [6] = superscript/subscript (not used in QFont)
        // These are handled by segment flags, not font attributes
    }

    return font;
}


} // namespace FontUtils
