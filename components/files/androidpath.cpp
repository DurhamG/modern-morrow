#include "androidpath.hpp"

#if defined(__ANDROID__)

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <jni.h>
#include <pwd.h>
#include <unistd.h>

static const char* g_path_global; //< Path to global directory root, e.g. /data/data/io.github.durhamg.modernmorrow
static const char* g_path_user; //< Path to user root, e.g. /sdcard/Android/data/io.github.durhamg.modernmorrow/files

/**
 * \brief Called by java code to set up directory paths
 */
extern "C" JNIEXPORT void JNICALL Java_io_github_durhamg_modernmorrow_GameActivity_getPathToJni(
    JNIEnv* env, jobject obj, jstring global, jstring user)
{
    g_path_global = env->GetStringUTFChars(global, nullptr);
    g_path_user = env->GetStringUTFChars(user, nullptr);
}

namespace Files
{

    AndroidPath::AndroidPath(const std::string& application_name) {}

    // /sdcard/Android/data/io.github.durhamg.modernmorrow/files/config
    std::filesystem::path AndroidPath::getUserConfigPath() const
    {
        return std::filesystem::path(g_path_user) / "config";
    }

    // /sdcard/Android/data/io.github.durhamg.modernmorrow/files/
    // (so that saves are placed at /sdcard/Android/data/io.github.durhamg.modernmorrow/files/saves)
    std::filesystem::path AndroidPath::getUserDataPath() const
    {
        return std::filesystem::path(g_path_user);
    }

    // /data/data/io.github.durhamg.modernmorrow/cache
    // (supposed to be "official" android cache location)
    std::filesystem::path AndroidPath::getCachePath() const
    {
        return std::filesystem::path(g_path_global) / "cache";
    }

    // /data/data/io.github.durhamg.modernmorrow/files/config
    // (note the addition of "files")
    std::filesystem::path AndroidPath::getGlobalConfigPath() const
    {
        return std::filesystem::path(g_path_global) / "files" / "config";
    }

    std::filesystem::path AndroidPath::getLocalPath() const
    {
        return std::filesystem::path("./");
    }

    // /sdcard/Android/data/io.github.durhamg.modernmorrow/files
    // (so that the data is at /sdcard/Android/data/io.github.durhamg.modernmorrow/files/data)
    std::filesystem::path AndroidPath::getGlobalDataPath() const
    {
        return std::filesystem::path(g_path_user);
    }

    std::vector<std::filesystem::path> AndroidPath::getInstallPaths() const
    {
        return {};
    }

} /* namespace Files */

#endif /* defined(__Android__) */
