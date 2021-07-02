#/bin/sh

function test {
  echo "+ $@"
  "$@"
  local status=$?
  if [ $status -ne 0 ]; then
    exit $status
  fi
  return $status
}

GIT_VERSION=`git describe --tags`
rm -f build/linux-x64/CMakeCache.txt 2>&1 >/dev/null

if [ "$1" == "local" ]
then
  set -e
  test make lt2http TARGET_OS=linux TARGET_ARCH=x64 GIT_VERSION=${GIT_VERSION} CROSS_ROOT=/usr/ CROSS_TRIPLE=x86_64-linux-gnu
  test chmod +x /var/tmp/lt2http
  test cp -rf /var/tmp/lt2http $HOME/.kodi/addons/service.lt2http/resources/bin/linux-x64/
  test cp -rf /var/tmp/lt2http $HOME/.kodi/userdata/addon_data/service.lt2http/bin/linux-x64/
elif [ "$1" == "sanitize" ]
then
  set -e
  test make lt2http TARGET_OS=linux TARGET_ARCH=x64 GIT_VERSION=${GIT_VERSION} CROSS_ROOT=/usr/ CROSS_TRIPLE=x86_64-linux-gnu CMAKE_BUILD_TYPE=RelWithDebInfo
  test chmod +x /var/tmp/lt2http
  test cp -rf /var/tmp/lt2http $HOME/.kodi/addons/service.lt2http/resources/bin/linux-x64/
  test cp -rf /var/tmp/lt2http $HOME/.kodi/userdata/addon_data/service.lt2http/bin/linux-x64/
elif [ "$1" == "docker" ]
then
  test make linux-x64
  test cp -rf build/linux-x64/out/lt2http $HOME/.kodi/addons/service.lt2http/resources/bin/linux-x64/
  test cp -rf build/linux-x64/out/lt2http $HOME/.kodi/userdata/addon_data/service.lt2http/bin/linux-x64/
fi
