# Patches for the Android dependencies

The patches in `osg/` change OpenMW's fork of OpenSceneGraph so that it draws through NG-GL4ES on Android. `osg/series` lists the order `android/Dockerfile` applies them in.

`android-no-dlinfo.patch` is ours. The other patches come unchanged from [openmw-android-docker](https://gitlab.com/cavebros/openmw-android-docker) by cavebros and Duron27, at commit `30f062fb5b1bb05b1ded0e25b20685ee1277f0ed`, which is licensed under the GNU General Public License version 3. Some of them came to that project from earlier OpenMW Android ports. They apply without fuzz to OSG commit `9b384d5439977784e5cd250ff8ff7966928ddd27`, the one `extern/CMakeLists.txt` pins.
