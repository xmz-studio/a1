#package.sh
pack() {
	local script_path="$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)"
	local project_root=${script_path}/..
	source "${script_path}/env.sh"
	source "${script_path}/generate-version.sh"
	local time="$(date -u +'%Y%m%dT-%H%M%SZ')"
	setinfo() {
		local name=$1
		local jbenv=$2
		cp -a ${project_root}/packages/${name}.template/DEBIAN.template/*.sh \
			"${project_root}/packages/${name}/${jbenv}/DEBIAN/" || return 1
		( cd "${project_root}/packages/${name}/${jbenv}/DEBIAN/" && for f in p*.sh; do mv "$f" "${f%.sh}"; done )
	}
	setdebinfo() {
		local name=$1
		local jbenv=$2
		local jbtype=$3
		if [ ${name} = cxxa1 ]; then
			local version=$(get_version general)
		elif [ ${name} = a1gui ]; then
			local version=$(get_version gui)
		else
			echo "[Error]: The first argument of Setdebinfo does not accept $1 as a parameter" >&2
			return 1
		fi
		mkdir -p ${project_root}/packages/${name}/${jbenv}/DEBIAN
		cp -a ${project_root}/packages/${name}.template/DEBIAN.template/control.in \
			${project_root}/packages/${name}/${jbenv}/DEBIAN/ || return 1
		{
			cd ${project_root}/packages/${name}/${jbenv}/DEBIAN/
			mv control.in control
			echo "Version: ${version}" >> control
			echo "Architecture: ${jbtype}" >> control
			rm -f .keep
		}
	}
	cd "$script_path/../"
	mkdir -p ${project_root}/packages/packdeb/$time
	if ls ${project_root}/packages/cxxa1/*.deb >/dev/null 2>&1; then
		mv ${project_root}/packages/cxxa1/*.deb \
			${project_root}/packages/packdeb/$time/
	fi
	rm -rf ${project_root}/packages/cxxa1
	cp -a ${project_root}/packages/cxxa1.template \
		${project_root}/packages/cxxa1

	setinfo "cxxa1" "rootless"
	setinfo "cxxa1" "roothide"
	setinfo "cxxa1" "rootful"

	if ls ${project_root}/packages/a1gui/*.deb >/dev/null 2>&1; then
		mv ${project_root}/packages/a1gui/*.deb \
			${project_root}/packages/packdeb/$time/
	fi
	rm -rf ${project_root}/packages/a1gui
	cp -a ${project_root}/packages/a1gui.template \
		${project_root}/packages/a1gui

	setinfo "a1gui" "rootless"
	setinfo "a1gui" "roothide"

	build_a1() {
		[ -z "$1" ] && echo "[Error]: the parameter cannot be empty" >&2
		cp ${project_root}/cxx* ${project_root}/packages/cxxa1/$1/$2/a1/bin/
		[ -f ${project_root}/myini ] && \
			cp ${project_root}/myini \
			${project_root}/packages/cxxa1/$1/$2/a1/bin/ || \
			echo "[Warn]: myini does not exist" >&2
		[ -f ${project_root}/flock ] && \
			cp ${project_root}/flock \
			${project_root}/packages/cxxa1/$1/$2/a1/bin/ || \
			echo "[Warn]: flock does not exist" >&2
		[ -f ${project_root}/bundle ] && \
			cp ${project_root}/bundle \
			${project_root}/packages/cxxa1/$1/$2/a1/bin/ || \
			echo "[Warn]: bundle does not exist" >&2
		ln -sf bundle ${project_root}/packages/cxxa1/$1/$2/a1/bin/bundle_pid
		ln -sf bundle ${project_root}/packages/cxxa1/$1/$2/a1/bin/pid_bundle
		ln -sf flock ${project_root}/packages/cxxa1/$1/$2/a1/bin/flock-ios
		if [ $1 = rootful ]; then
			setdebinfo cxxa1 $1 "iphoneos-arm"
		elif [ $1 = rootless ]; then
			setdebinfo cxxa1 $1 "iphoneos-arm64"
		elif [ $1 = roothide ]; then
			setdebinfo cxxa1 $1 "iphoneos-arm64e"
		else
			echo "[Error]: unknown or not supported jailbreak type: $1" >&2
			return 1
		fi
		dpkg-deb -b ${project_root}/packages/cxxa1/$1 \
			${project_root}/packages/cxxa1/cxxa1-${1}.deb
		dpkg-name ${project_root}/packages/cxxa1/cxxa1-${1}.deb
	}
	build_a1gui() {
		[ -z "$1" ] && echo "[Error]: the parameter cannot be empty" >&2
		setlocale() {
			{
				cd ${project_root}/packages/a1gui/$1/$2/Applications/a1gui.app/
				cd $3.lproj
				mv Localizable.strings.in Localizable.strings
			}
		}
		cp ${project_root}/a1gui \
			${project_root}/packages/a1gui/$1/$2/Applications/a1gui.app/
		cp ${project_root}//src/gui/Info.plist \
			${project_root}/packages/a1gui/$1/$2/Applications/a1gui.app/
		cp -r ${project_root}/src/gui/resources/* \
			${project_root}/packages/a1gui/$1/$2/Applications/a1gui.app/
		if [ $1 = rootless ] && [ "$2" = "var/jb" ]; then
			setlocale $1 $2 "zh-CN"
			setlocale $1 $2 "en-US"
			setdebinfo a1gui rootless "iphoneos-arm64"
		elif [ $1 = roothide ] && [ "$2" = "" ]; then
			setlocale $1 "/" "zh-CN"
			setlocale $1 "/" "en-US"
			setdebinfo a1gui roothide "iphoneos-arm64e"
		else
			echo "[Error]: unknown or not supported jailbreak type: $1" >&2
			return 1
		fi
		dpkg-deb -b ${project_root}/packages/a1gui/$1 \
			${project_root}/packages/a1gui/a1gui-${1}.deb
		dpkg-name ${project_root}/packages/a1gui/a1gui-${1}.deb
	}
	if [ ! "$1" = "no-pack-a1" ] || [ "$1" = "all" ]; then
		build_a1 rootless "var/jb"
		build_a1 rootful ""
		build_a1 roothide ""
	fi
	if [ ! "$1" = "no-pack-a1gui" ] || [ "$1" = "all" ]; then
		build_a1gui rootless "var/jb"
		build_a1gui roothide ""
	fi
	rm -f ${project_root}/cxx* ${project_root}/a1gui
	rm -f ${project_root}/bundle ${project_root}/myini ${project_root}/flock
}

pack_a1() { pack "no-pack-a1gui" "$@"; }
pack_a1gui() { pack "no-pack-a1" "$@"; }
pack_all() { pack "all" "$@"; }

