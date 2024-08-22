
truenas_versions=$(curl https://download.truenas.com/ | grep -Po "(?<=href=\")TrueNAS-SCALE-\w*?/\d+(\.\d+)*" | uniq)

for version in ${truenas_versions}; do 
    echo Building "$version"
    bash build.sh "$version"
done
