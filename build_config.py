import os
import sys
from build_support import *

#### You should not change these.  These are only here
#### If you want to start your own build setup with a
#### different layout than mine.

source_base_dir = 'src'
build_base_dir = 'build'
target_name = 'lprof'
target_name2 = 'icc2it8'

# XXX This is OS specific; only /usr/lib and perhaps /lib belongs here,
# and all other paths should be added via variables passed
# to scons (perhaps as PREFIX is), so that packaging systems can point
# lprof at various dependencies.
# Perhaps qt3 paths should be removed entirely given QTDIR support.
# If this remains, NetBSD needs /usr/pkg/lib and /usr/pkg/qt3/lib.

lib_search_path = [
os.path.normpath('/lib'),
os.path.normpath('/usr/lib'),
os.path.normpath('/usr/X11R6/lib'),
os.path.normpath('/usr/local/lib'),
os.path.normpath('/usr/pkg/lib'),
os.path.normpath(source_base_dir+'/argyll/rspl'),
os.path.normpath(source_base_dir+'/argyll/numlib'),
os.path.normpath('argyll/rspl'),
os.path.normpath('argyll/numlib')
]

build_dir = '#' + SelectBuildDir(build_base_dir)

## where we should find things to include
# XXX We need a general way to add prefixes whose include dirs should
# be searched.  Until then, add /usr/pkg/include.

if sys.platform == 'win32':
	compiler_path = os.environ[ 'MSVCDIR' ]
	include_search_path = [
	os.path.normpath(compiler_path + '\include'),
	'#' + os.path.normpath(source_base_dir + '/libqtlcmswidgets'),
	'#' + os.path.normpath(source_base_dir),
	'#' + os.path.normpath(source_base_dir + '/liblprof'),
	'#' + os.path.normpath(source_base_dir + '/lprofqt'),
	'#' + os.path.normpath(source_base_dir + '/checkerqt'),
	'#' + os.path.normpath(source_base_dir + '/gammaqt'),
	'#' + os.path.normpath(source_base_dir + '/IDqt'),
	'#' + os.path.normpath(source_base_dir + '/monqt'),
	'#' + os.path.normpath(source_base_dir + '/parmsqt'),
	'#' + os.path.normpath(source_base_dir + '/reference_inst_qt'),
	os.path.normpath(build_dir + '/libqtlcmswidgets'),
	os.path.normpath(build_dir+'/liblprof'),
	os.path.normpath(build_dir+'/lprofqt'),
	os.path.normpath(build_dir+'/checkerqt'),
	os.path.normpath(build_dir+'/gammaqt'),
	os.path.normpath(build_dir+'/IDqt'),
	os.path.normpath(build_dir+'/monqt'),
	os.path.normpath(build_dir+'/parmsqt'),
	os.path.normpath(build_dir+'/reference_inst_qt'),
	os.path.normpath(build_dir+'/argyll/rspl'),
	os.path.normpath(build_dir+'/argyll/numlib'),
	os.path.normpath(build_dir+'/argyll/h')
	]
else:
	include_search_path = [
	os.path.normpath('/usr/include'),
        os.path.normpath('/usr/include/lcms'),
	os.path.normpath('/usr/pkg/include'),
        os.path.normpath('/usr/local/include'),
        os.path.normpath('/usr/local/include/lcms'),
	os.path.normpath('/usr/local/pkg/include'),
        '#' + os.path.normpath(source_base_dir),
	'#' + os.path.normpath(source_base_dir + '/libqtlcmswidgets'),
	'#' + os.path.normpath(source_base_dir),
	'#' + os.path.normpath(source_base_dir + '/liblprof'),
	'#' + os.path.normpath(source_base_dir + '/lprofqt'),
	'#' + os.path.normpath(source_base_dir + '/checkerqt'),
	'#' + os.path.normpath(source_base_dir + '/gammaqt'),
	'#' + os.path.normpath(source_base_dir + '/IDqt'),
	'#' + os.path.normpath(source_base_dir + '/monqt'),
	'#' + os.path.normpath(source_base_dir + '/parmsqt'),
	'#' + os.path.normpath(source_base_dir + '/reference_inst_qt'),
        '#' + os.path.normpath(source_base_dir + '/wwfloatspinbox'),
	os.path.normpath(build_dir + '/libqtlcmswidgets'), 
        os.path.normpath(build_dir+'/liblprof'),
	os.path.normpath(build_dir+'/lprofqt'),
	os.path.normpath(build_dir+'/checkerqt'),
	os.path.normpath(build_dir+'/gammaqt'),
	os.path.normpath(build_dir+'/IDqt'),
	os.path.normpath(build_dir+'/monqt'),
	os.path.normpath(build_dir+'/parmsqt'),
	os.path.normpath(build_dir+'/reference_inst_qt'),
	os.path.normpath(build_dir+'/argyll/rspl'),
	os.path.normpath(build_dir+'/argyll/numlib'),
	os.path.normpath(build_dir+'/argyll/h'),
        os.path.normpath(build_dir+'/wwfloatspinbox')
	]

## These are our source files

liblprof_sources = [os.path.normpath('liblprof/cmslm.c'), os.path.normpath('liblprof/cmsmkmsh.c'), os.path.normpath('liblprof/cmspcoll.c'), os.path.normpath('liblprof/cmsscn.c'), os.path.normpath('liblprof/cmslnr.c'), os.path.normpath('liblprof/cmsmntr.c'), os.path.normpath('liblprof/cmsprf.c'), os.path.normpath('liblprof/cmssheet.c'), os.path.normpath('liblprof/cmshull.c'), os.path.normpath('liblprof/cmsmatn.c'), os.path.normpath('liblprof/cmsoutl.c'), os.path.normpath('liblprof/cmsreg.c')]

sources = [os.path.normpath('lprofqt/lprofmainbase.ui'), os.path.normpath('IDqt/profileidbase.ui'), os.path.normpath('gammaqt/setgammabase.ui'), os.path.normpath('reference_inst_qt/installreffilebase.ui'), os.path.normpath('parmsqt/profileparmsbase.ui'), os.path.normpath('monqt/monitorvaluesbase.ui'), os.path.normpath('checkerqt/profilecheckerbase.ui'), os.path.normpath('libqtlcmswidgets/qtlcmswidgets.cpp'), os.path.normpath('checkerqt/profilechecker.cpp'), os.path.normpath('checkerqt/qtcietonge.cpp'), os.path.normpath('checkerqt/qtdrawcurve.cpp'), os.path.normpath('gammaqt/setgamma.cpp'), os.path.normpath('IDqt/profileid.cpp'),  os.path.normpath('lprofqt/lprofmain.cpp'), os.path.normpath('lprofqt/main.cpp'), os.path.normpath('monqt/monitorvalues.cpp'), os.path.normpath('parmsqt/profileparms.cpp'), os.path.normpath('reference_inst_qt/installreffile.cpp'), os.path.normpath('wwfloatspinbox/wwfloatspinbox.cpp'),
# os.path.normpath('wwfloatspinbox/uinavlist.cpp'),
# os.path.normpath('wwfloatspinbox/wwcanvasview.cpp'),
# os.path.normpath('wwfloatspinbox/wwclasses.cpp') #,
# os.path.normpath('wwfloatspinbox/wwplugins.cpp'), 
# os.path.normpath('wwfloatspinbox/wwlineedit.cpp')
] + liblprof_sources

ICCtoIT8_sources = [os.path.normpath('ICCtoIT8/getopt.c'), os.path.normpath('ICCtoIT8/icc2it8.c')] + liblprof_sources

