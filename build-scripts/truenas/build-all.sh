
truenas_versions=$(curl https://download.truenas.com/ | grep -Po "(?<=href=\")TrueNAS-SCALE-\w*?/\d+(\.\d+)*" | uniq)

for version in ${truenas_versions}; do 
    # only build for TrueNAS-SCALE-Dragonfish/24.04.0 and later versions
    if [ $(echo $version | grep -Po "(?<=/)\d+") -ge 24 ] && [ ! -d "build/$version" ]; then
        echo Building "$version"
        bash build.sh "$version"
    fi
done

find -name *.ko
