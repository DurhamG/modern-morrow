// The Android transport for cloud saves. The app's Java class CloudRequest sends each request with Android's own
// HttpURLConnection, and this calls it through JNI.
#include "cloudsaves.hpp"

#include <SDL_system.h>
#include <jni.h>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>

namespace
{
    constexpr const char* sRequestClass = "io/github/durhamg/modernmorrow/CloudRequest";
    constexpr const char* sReplyClass = "io/github/durhamg/modernmorrow/CloudRequest$Reply";
    constexpr const char* sSendSignature
        = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B[BJ)"
          "Lio/github/durhamg/modernmorrow/CloudRequest$Reply;";

    // Clears a Java exception, after which the next call into Java would stop the app, and logs where it came from.
    // Its message isn't logged, since it can hold the address.
    bool failed(JNIEnv* env, const char* what)
    {
        if (!env->ExceptionCheck())
            return false;
        env->ExceptionClear();
        Log(Debug::Warning) << "Cloud saves: " << what << " failed with a Java exception";
        return true;
    }

    // Java takes the file paths as UTF-8 bytes. The strings JNI makes from C++ use a form of UTF-8 of Java's own,
    // which can't hold characters such as emoji, and a save's name can.
    jbyteArray pathBytes(JNIEnv* env, const std::filesystem::path* path)
    {
        if (path == nullptr)
            return nullptr;
        const std::string utf8 = Files::pathToUnicodeString(*path);
        const jsize size = static_cast<jsize>(utf8.size());
        jbyteArray bytes = env->NewByteArray(size);
        if (bytes != nullptr)
            env->SetByteArrayRegion(bytes, 0, size, reinterpret_cast<const jbyte*>(utf8.data()));
        return bytes;
    }
}

// Called by CloudRequest as a request goes, with the address of the CloudProgress that send was given.
extern "C" JNIEXPORT void JNICALL Java_io_github_durhamg_modernmorrow_CloudRequest_reportProgress(
    JNIEnv*, jclass, jlong progress, jlong done, jlong total)
{
    const auto& report = *reinterpret_cast<const MWState::CloudProgress*>(progress);
    report(static_cast<std::uint64_t>(done), static_cast<std::uint64_t>(total));
}

namespace MWState
{
    namespace
    {
        class AndroidTransport final : public CloudTransport
        {
        public:
            // Made on the game's main thread, which Java started. Java's classes are looked up here, because on a
            // thread the game starts itself, such as the one requests run on, FindClass only finds Android's own.
            explicit AndroidTransport(std::string userAgent)
                : mUserAgent(std::move(userAgent))
            {
                JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
                if (env == nullptr)
                {
                    Log(Debug::Warning)
                        << "Cloud saves: there's no Java on this thread, so cloud saves aren't available";
                    return;
                }
                jclass request = env->FindClass(sRequestClass);
                if (failed(env, "finding CloudRequest") || request == nullptr)
                    return;
                jclass reply = env->FindClass(sReplyClass);
                if (failed(env, "finding CloudRequest.Reply") || reply == nullptr)
                {
                    env->DeleteLocalRef(request);
                    return;
                }
                mSend = env->GetStaticMethodID(request, "sendFromEngine", sSendSignature);
                if (!failed(env, "finding CloudRequest.sendFromEngine"))
                    mStatus = env->GetFieldID(reply, "mStatus", "I");
                if (!failed(env, "finding Reply.mStatus"))
                    mBody = env->GetFieldID(reply, "mBody", "[B");
                if (!failed(env, "finding Reply.mBody"))
                    mFailure = env->GetFieldID(reply, "mFailure", "Ljava/lang/String;");
                if (!failed(env, "finding Reply.mFailure") && mSend != nullptr && mStatus != nullptr && mBody != nullptr
                    && mFailure != nullptr)
                    mRequest = static_cast<jclass>(env->NewGlobalRef(request));
                env->DeleteLocalRef(reply);
                env->DeleteLocalRef(request);
            }

            ~AndroidTransport() override
            {
                if (mRequest == nullptr)
                    return;
                if (JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv()))
                    env->DeleteGlobalRef(mRequest);
            }

            CloudReply send(std::string_view method, const std::string& url, const std::string& authorization,
                const std::filesystem::path* bodyFile, const std::filesystem::path* downloadTo,
                const CloudProgress& progress) override
            {
                CloudReply reply;
                if (mRequest == nullptr)
                {
                    reply.mAvailable = false;
                    return reply;
                }
                // SDL connects this thread to Java the first time, and disconnects it when the thread ends.
                JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
                if (env == nullptr)
                {
                    Log(Debug::Warning) << "Cloud saves: there's no Java on the requests' thread";
                    return reply;
                }
                // The thread lasts as long as the game, so Java's objects made for a request are let go here, rather
                // than when the thread ends.
                if (env->PushLocalFrame(16) != 0)
                {
                    failed(env, "making room for a request");
                    return reply;
                }

                jstring javaMethod = env->NewStringUTF(std::string(method).c_str());
                jstring javaUrl = env->NewStringUTF(url.c_str());
                jstring javaAuthorization = env->NewStringUTF(authorization.c_str());
                jstring javaUserAgent = env->NewStringUTF(mUserAgent.c_str());
                jbyteArray javaBodyFile = pathBytes(env, bodyFile);
                jbyteArray javaDownloadTo = pathBytes(env, downloadTo);
                if (failed(env, "starting a request"))
                {
                    env->PopLocalFrame(nullptr);
                    return reply;
                }

                const jlong progressAddress = progress ? reinterpret_cast<jlong>(&progress) : 0;
                jobject result = env->CallStaticObjectMethod(mRequest, mSend, javaMethod, javaUrl, javaAuthorization,
                    javaUserAgent, javaBodyFile, javaDownloadTo, progressAddress);
                if (failed(env, "the request") || result == nullptr)
                {
                    env->PopLocalFrame(nullptr);
                    return reply;
                }

                reply.mStatus = env->GetIntField(result, mStatus);
                auto body = static_cast<jbyteArray>(env->GetObjectField(result, mBody));
                if (body != nullptr)
                {
                    reply.mBody.resize(static_cast<std::size_t>(env->GetArrayLength(body)));
                    env->GetByteArrayRegion(
                        body, 0, static_cast<jsize>(reply.mBody.size()), reinterpret_cast<jbyte*>(reply.mBody.data()));
                }
                // The reason a request failed, which never holds the address or the phrase.
                auto failure = static_cast<jstring>(env->GetObjectField(result, mFailure));
                if (failure != nullptr)
                {
                    const char* chars = env->GetStringUTFChars(failure, nullptr);
                    if (chars != nullptr)
                    {
                        Log(Debug::Warning) << "Cloud saves: " << chars;
                        env->ReleaseStringUTFChars(failure, chars);
                    }
                }
                failed(env, "reading the reply");
                env->PopLocalFrame(nullptr);
                return reply;
            }

        private:
            std::string mUserAgent;
            jclass mRequest = nullptr;
            jmethodID mSend = nullptr;
            jfieldID mStatus = nullptr;
            jfieldID mBody = nullptr;
            jfieldID mFailure = nullptr;
        };
    }

    std::unique_ptr<CloudTransport> makeCloudTransport(const std::string& userAgent)
    {
        return std::make_unique<AndroidTransport>(userAgent);
    }
}
