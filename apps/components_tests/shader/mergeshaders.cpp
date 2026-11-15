#include <components/shader/mergeshaders.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace Shader
{
    namespace
    {
        std::string guarded(std::string_view name, std::string_view line)
        {
            const std::string guard = "OMW_UNIFORM_" + std::string(name);
            return "#ifndef " + guard + "\n#define " + guard + "\n" + std::string(line) + "\n#endif";
        }

        TEST(ShaderMergeShadersTest, plainUniformShouldBeGuardedByItsName)
        {
            EXPECT_EQ(declareUniformOnce("uniform sampler2D diffuseMap;"),
                guarded("diffuseMap", "uniform sampler2D diffuseMap;"));
        }

        TEST(ShaderMergeShadersTest, indentedUniformShouldBeGuarded)
        {
            EXPECT_EQ(declareUniformOnce("    uniform mat4 osg_ViewMatrix;"),
                guarded("osg_ViewMatrix", "    uniform mat4 osg_ViewMatrix;"));
        }

        TEST(ShaderMergeShadersTest, uniformArrayShouldBeGuardedByItsNameWithoutTheSize)
        {
            EXPECT_EQ(declareUniformOnce("uniform vec4 lights[8];"), guarded("lights", "uniform vec4 lights[8];"));
        }

        TEST(ShaderMergeShadersTest, uniformWithAPrecisionShouldBeGuardedByItsName)
        {
            EXPECT_EQ(declareUniformOnce("uniform highp float near;"), guarded("near", "uniform highp float near;"));
        }

        TEST(ShaderMergeShadersTest, uniformWithAnInitialValueShouldBeLeftAlone)
        {
            EXPECT_EQ(declareUniformOnce("uniform float scale = 1.0;"), "uniform float scale = 1.0;");
        }

        TEST(ShaderMergeShadersTest, uniformListShouldBeLeftAlone)
        {
            EXPECT_EQ(declareUniformOnce("uniform float a, b;"), "uniform float a, b;");
        }

        TEST(ShaderMergeShadersTest, uniformBlockShouldBeLeftAlone)
        {
            EXPECT_EQ(declareUniformOnce("uniform LightBuffer"), "uniform LightBuffer");
        }

        TEST(ShaderMergeShadersTest, uniformWithALayoutQualifierShouldBeLeftAlone)
        {
            EXPECT_EQ(declareUniformOnce("layout(std140) uniform Lights;"), "layout(std140) uniform Lights;");
            EXPECT_EQ(declareUniformOnce("uniform sampler2D maps(4);"), "uniform sampler2D maps(4);");
        }

        TEST(ShaderMergeShadersTest, otherLinesShouldBeUnchanged)
        {
            for (std::string_view line : { "", "void main()", "  gl_FragColor = vec4(1.0);", "// uniform float x;",
                     "uniforms are declared above" })
                EXPECT_EQ(declareUniformOnce(line), line);
        }

        TEST(ShaderMergeShadersTest, windowsLineEndingsShouldStillBeGuarded)
        {
            EXPECT_EQ(declareUniformOnce("uniform float near;\r"), guarded("near", "uniform float near;\r"));
        }

        TEST(ShaderMergeShadersTest, sameUniformTwiceShouldBeGuardedBothTimes)
        {
            const std::string merged = foldForMerge("uniform float near;\nuniform float near;\n", false);
            EXPECT_EQ(
                merged, guarded("near", "uniform float near;") + "\n" + guarded("near", "uniform float near;") + "\n");
        }

        TEST(ShaderMergeShadersTest, mainShaderShouldKeepItsVersionLine)
        {
            EXPECT_EQ(foldForMerge("#version 120\nvoid main() {}\n", false), "#version 120\nvoid main() {}\n");
        }

        TEST(ShaderMergeShadersTest, linkedShaderShouldLoseItsVersionLine)
        {
            EXPECT_EQ(foldForMerge("#version 120\nvoid helper() {}\n", true), "\nvoid helper() {}\n");
            EXPECT_EQ(foldForMerge("  #version 120\nvoid helper() {}\n", true), "\nvoid helper() {}\n");
        }

        TEST(ShaderMergeShadersTest, lineCountShouldStayTheSameApartFromGuards)
        {
            const std::string source = "#version 120\nuniform float near;\nvoid main()\n{\n}\n";
            const std::string merged = foldForMerge(source, true);
            // Each guard adds three lines around the declaration.
            EXPECT_EQ(
                std::count(merged.begin(), merged.end(), '\n'), std::count(source.begin(), source.end(), '\n') + 3);
        }

        TEST(ShaderMergeShadersTest, sourceWithoutAFinalNewlineShouldNotGainOne)
        {
            EXPECT_EQ(foldForMerge("void main() {}", false), "void main() {}");
        }
    }
}
