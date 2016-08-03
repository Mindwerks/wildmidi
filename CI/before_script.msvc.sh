#!/bin/bash

while [ $# -gt 0 ]; do
	ARGSTR=$1
	shift

	if [ ${ARGSTR:0:1} != "-" ]; then
		echo "Unknown argument $ARGSTR"
		echo "Try '$0 -h'"
		exit 1
	fi

	for (( i=1; i<${#ARGSTR}; i++ )); do
		ARG=${ARGSTR:$i:1}
		case $ARG in
			V )
				VERBOSE=true ;;

			v )
				VS_VERSION=$1
				shift ;;

			d )
				SKIP_DOWNLOAD=true ;;

			e )
				SKIP_EXTRACT=true ;;

			k )
				KEEP=true ;;

			u )
				UNITY_BUILD=true ;;

			p )
				PLATFORM=$1
				shift ;;

			c )
				CONFIGURATION=$1
				shift ;;

			h )
				cat <<EOF
Usage: $0 [-cdehkpuvV]

Options:
	-c <Release/Debug>
		Set the configuration, can also be set with environment variable CONFIGURATION.
	-d
		Skip checking the downloads.
	-e
		Skip extracting dependencies.
	-h
		Show this message.
	-k
		Keep the old build directory, default is to delete it.
	-p <Win32/Win64>
		Set the build platform, can also be set with environment variable PLATFORM.
	-u
		Configure for unity builds.
	-v <2013/2015>
		Choose the Visual Studio version to use.
	-V
		Run verbosely
EOF
				exit 0
				;;

			* )
				echo "Unknown argument $ARG."
				echo "Try '$0 -h'"
				exit 1 ;;
		esac
	done
done

if [ -z $VERBOSE ]; then
	STRIP="> /dev/null 2>&1"
fi
if [ -z $VS_VERSION ]; then
	VS_VERSION="2013"
fi

if [ -z $APPVEYOR ]; then
	echo "Running prebuild outside of Appveyor."

	DIR=$(echo "$0" | sed "s,\\\\,/,g" | sed "s,\(.\):,/\\1,")
	cd $(dirname "$DIR")/..
else
	echo "Running prebuild in Appveyor."

	cd $APPVEYOR_BUILD_FOLDER
	VERSION="$(cat README.md | grep Version: | awk '{ print $3; }')-$(git rev-parse --short HEAD)"
	appveyor UpdateBuild -Version "$VERSION" > /dev/null &
fi

run_cmd() {
	CMD="$1"
	shift

	if [ -z $VERBOSE ]; then
		eval $CMD $@ > output.log 2>&1
		RET=$?

		if [ $RET -ne 0 ]; then
			if [ -z $APPVEYOR ]; then
				echo "Command $CMD failed, output can be found in `real_pwd`/output.log"
			else
				echo
				echo "Command $CMD failed;"
				cat output.log
			fi
		else
			rm output.log
		fi

		return $RET
	else
		eval $CMD $@
		return $?
	fi
}

download() {
	if [ $# -lt 3 ]; then
		echo "Invalid parameters to download."
		return 1
	fi

	NAME=$1
	shift

	echo "$NAME..."

	while [ $# -gt 1 ]; do
		URL=$1
		FILE=$2
		shift
		shift

		if ! [ -f $FILE ]; then
			printf "  Downloading $FILE... "

			if [ -z $VERBOSE ]; then
				curl --silent --retry 10 -kLy 5 -o $FILE $URL
				RET=$?
			else
				curl --retry 10 -kLy 5 -o $FILE $URL
				RET=$?
			fi

			if [ $RET -ne 0 ]; then
				echo "Failed!"
			else
				echo "Done."
			fi
		else
			echo "  $FILE exists, skipping."
		fi
	done

	if [ $# -ne 0 ]; then
		echo "Missing parameter."
	fi
}

real_pwd() {
	pwd | sed "s,/\(.\),\1:,"
}

CMAKE_OPTS=""
add_cmake_opts() {
	CMAKE_OPTS="$CMAKE_OPTS $@"
}

RUNTIME_DLLS=""
add_runtime_dlls() {
	RUNTIME_DLLS="$RUNTIME_DLLS $@"
}

OSG_PLUGINS=""
add_osg_dlls() {
	OSG_PLUGINS="$OSG_PLUGINS $@"
}

if [ -z $PLATFORM ]; then
	PLATFORM=`uname -m`
fi

if [ -z $CONFIGURATION ]; then
	CONFIGURATION="Debug"
fi

case $VS_VERSION in
	14|2015 )
		GENERATOR="Visual Studio 14 2015"
		XP_TOOLSET="v140_xp"
		;;

#	12|2013|
	* )
		GENERATOR="Visual Studio 12 2013"
		XP_TOOLSET="v120_xp"
		;;
esac

case $PLATFORM in
	x64|x86_64|x86-64|win64|Win64 )
		ARCHNAME=x86-64
		ARCHSUFFIX=64
		BITS=64

		BASE_OPTS="-G\"$GENERATOR Win64\""
		add_cmake_opts "-G\"$GENERATOR Win64\""
		;;

	x32|x86|i686|i386|win32|Win32 )
		ARCHNAME=x86
		ARCHSUFFIX=86
		BITS=32

		BASE_OPTS="-G\"$GENERATOR\" -T$XP_TOOLSET"
		add_cmake_opts "-G\"$GENERATOR\"" -T$XP_TOOLSET
		;;

	* )
		echo "Unknown platform $PLATFORM."
		exit 1
		;;
esac

case $CONFIGURATION in
	debug|Debug|DEBUG )
		CONFIGURATION=Debug
		;;

	release|Release|RELEASE )
		CONFIGURATION=Release
		;;

	relwithdebinfo|RelWithDebInfo|RELWITHDEBINFO )
		CONFIGURATION=RelWithDebInfo
		;;
esac

echo
echo "=========================="
echo "Starting prebuild on win$BITS"
echo "=========================="
echo

mkdir -p deps
cd deps

DEPS="`pwd`"

if [ -z $SKIP_DOWNLOAD ]; then
	echo "Downloading dependency packages."
	echo

	# OpenAL
	download "OpenAL-Soft 1.16.0" \
		http://kcat.strangesoft.net/openal-binaries/openal-soft-1.16.0-bin.zip \
		OpenAL-Soft-1.16.0.zip

fi

cd .. #/..

# Set up dependencies
if [ -z $KEEP ]; then
	echo
	printf "Preparing build directory... "

	rm -rf Build_$BITS
	mkdir -p Build_$BITS/deps

	echo Done.
fi
mkdir -p Build_$BITS/deps
cd Build_$BITS/deps

DEPS_INSTALL=`pwd`
cd $DEPS

echo
echo "Extracting dependencies..."

# OpenAL
printf "OpenAL-Soft 1.16.0... "
{
	if [ -d openal-soft-1.16.0-bin ]; then
		printf "Exists. "
	elif [ -z $SKIP_EXTRACT ]; then
		rm -rf openal-soft-1.16.0-bin
		eval 7z x -y OpenAL-Soft-1.16.0.zip $STRIP
	fi

	OPENAL_SDK="`real_pwd`/openal-soft-1.16.0-bin"

	add_cmake_opts -DOPENAL_INCLUDE_DIR="$OPENAL_SDK/include/AL" \
		-DOPENAL_LIBRARY="$OPENAL_SDK/libs/Win$BITS/OpenAL32.lib"

	echo Done.
}

cd $DEPS_INSTALL/..

echo
echo "Setting up WildMIDI build..."

if [ -z $VERBOSE ]; then
	printf "  Configuring... "
else
	echo "  cmake .. $CMAKE_OPTS"
fi

run_cmd cmake .. $CMAKE_OPTS
RET=$?

if [ -z $VERBOSE ]; then
	if [ $RET -eq 0 ]; then echo Done.
	else echo Failed.; fi
fi

echo

# NOTE: Disable this when/if we want to run test cases
if [ -z $CI ]; then
	echo "Copying Runtime DLLs..."
	mkdir -p $CONFIGURATION
	for DLL in $RUNTIME_DLLS; do
		echo "  `basename $DLL`."
		cp "$DLL" $CONFIGURATION/
	done

	echo "Copying Runtime Resources/Config Files"

	echo "  wildmidi.cfg"
	cp $CONFIGURATION/../cfg/wildmidi.cfg $CONFIGURATION/wildmidi.cfg
fi

exit $RET
