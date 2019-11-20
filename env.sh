p=`realpath ${1:?path not specified}`

echo "PATH += $p/bin"
echo "OPIUM_PATH = $p/lib/opium"

export PATH=$p/bin:$PATH
export OPIUM_PATH=$p/lib/opium
