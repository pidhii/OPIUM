p=`realpath ${1:?path not specified}`

#echo "PATH += $p/bin"
#echo "OPIUM_ROOT = $p"
#echo "OPIUM_PATH = $p/lib/opium"
#echo "PKG_CONFIG_PATH = $p/lib/pkgconfig"

export PATH=$p/bin:$PATH
export OPIUM_ROOT=$p
export OPIUM_PATH=$p/lib/opium
export PKG_CONFIG_PATH=${PKG_CONFIG_PATH:+${PKG_CONFIG_PATH}:}$p/lib/pkgconfig
