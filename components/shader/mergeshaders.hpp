#ifndef OPENMW_COMPONENTS_SHADER_MERGESHADERS_H
#define OPENMW_COMPONENTS_SHADER_MERGESHADERS_H

#include <string>
#include <string_view>

namespace Shader
{
    // Merging linked shaders into one, which OpenGL ES needs: it links only one shader object per stage. Only the
    // Android build merges shaders, but these are plain text functions, so every build compiles and tests them.
    // Adapted from Alpha3's support library (https://gitlab.com/duron27/alpha3,
    // cpp/openmw-upstream/support/src/shaders.cpp), which is licensed under the GNU GPL version 3.

    /// Separate shader objects may each declare the same uniform, but one shader may declare it only once. This puts
    /// a plain uniform declaration behind a guard named after the uniform, so the preprocessor keeps the first one.
    /// Other lines, and declarations with an initial value, a list or a layout, are returned as they are.
    std::string declareUniformOnce(std::string_view line);

    /// Guards the source's uniform declarations, and drops the #version line of a linked shader. Each line keeps its
    /// place apart from the guards' lines.
    std::string foldForMerge(const std::string& source, bool linked);
}

#endif
