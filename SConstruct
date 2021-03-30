# $Id: SConstruct,v 1.62 2006/06/06 01:11:32 hvengel Exp $

import os
import sys
from build_support import *
from build_config import *


# Setup some of our requirements

# setup user configuration section ---------------------

# Get our configuration options:
opts = Options('lprof.conf') 
opts.Add(PathOption("qt_directory", "Path to Qt directory", "/"))
opts.Add(PathOption('PREFIX', 'Directory to install under', os.path.normpath('/usr/local')))
opts.Add('ccflags', 'Flags to be passed to c compiler.', '-O2 -Wall -pipe')
opts.Add('cxxflags', 'Flags to be passed to c++ compiler.', '-O2 -Wall -pipe')
opts.Add('ldflags', 'Stuff to be added to LDFLAGS. If more than one item is being added use space btween items. Enclose multipule items in quotes.', '')
opts.Add('added_cppflags', 'List of flags and/or directories to be added to CPPPATH.  If more than one item is being added use space btween items. Enclose multipule items in quotes.', '') 
opts.Add('added_libpath', 'List of directories to be added to the LIBPATH.  If more than one item is being added use space btween items. Enclose multipule items in quotes.', '')
# opts.Add(BoolOption('qt-mt-lib', 'Flag used to set QT library to either qt-mt or qt. Value of 1 = qt-mt, 0 = qt.', 'yes'))

# setup base environemnt 
env = Environment(
    ENV = {
      'PATH' : os.environ[ 'PATH' ],
      'HOME' : os.environ[ 'HOME' ], # required for distcc
      'LDFLAGS' : ''
    }, options = opts)

opts.Update(env)
opts.Save('lprof.conf', env) # Save, so user doesn't have to 
                             # specify PREFIX CFLAGS or CXXFLAGS every time

Help(opts.GenerateHelpText(env))

# check SCons version make sure it is new enough

env.EnsureSConsVersion(0,96)

# Prevent the creation of .sconsign files in every directory
# This will keep these files from showing up in PREFIX/share/lprof

env.SConsignFile()

# end user configuration section ---------------------------------------

# Tool functions --------------------------------------------------------------
# to help with QT configuration checks

def RunProgramAndGetResult(commandline):
  file = os.popen(commandline, "r")
  result = file.read().rstrip()
  exit_code = file.close()
  if exit_code is not None:
    raise RuntimeError, "Program exited with non-zero exit code."
  return result


def DoWithVariables(variables, prefix, what):
  saved_variables = { }
  for name in variables.keys():
    saved_variables[ name ] = env[ name ][:]
    if isinstance(variables[ name ], list):
      env[ name ].extend(variables[ name ])
    else:
      env[ name ].append(variables[ name ])

  result = what()
  
  for name in saved_variables.keys():
    env[ name ] = saved_variables[ name ]
    env[ prefix+name ] = variables[ name ]

  return result

# end Tool functions -----------------------------------------------------

# Functions to Check for QT ----------------------------------------------------------

def CheckForQtAt(context, qtdir):
  context.Message('Checking for Qt at %s... ' % qtdir)
  libp = os.path.join(qtdir, 'lib')
  cppp = os.path.join(qtdir, 'include')
  result = AttemptLinkWithVariables(context,
      { "LIBS": "qt-mt", "LIBPATH": libp , "CPPPATH": cppp },
      """
#include <qapplication.h>
int main(int argc, char **argv) { 
  QApplication qapp(argc, argv);
  return 0;
}
""",".cpp","QT_")
  context.Result(result)
  return result


def CheckForQt(context):
  # list is currently POSIX centric - what happens with Windows?
  potential_qt_dirs = [
    "/usr/share/qt3", # Debian unstable
    "/usr/share/qt",
    "/usr",
    "/usr/local",
    "/usr/lib/qt3", # Suse
    "/usr/lib/qt",
    "/usr/qt/3", # Gentoo
    "/usr/pkg/qt3" # pkgsrc (NetBSD)
    ]

  if os.environ.has_key('QTDIR'):
    potential_qt_dirs.insert(0, os.environ[ 'QTDIR' ])
  
  if env[ 'qt_directory' ] != "/":
     uic_path = os.path.join(env['qt_directory'], 'bin', 'uic')
     if os.path.isfile(uic_path):
        potential_qt_dirs.insert(0, env[ 'qt_directory' ])
     else:
        print "QT not found. Invalid qt_directory value - failed to find uic."
        return 0

  for i in potential_qt_dirs:
    context.env.Replace(QTDIR = i)
    if CheckForQtAt(context, i):
       # additional checks to validate QT installation
       if not os.path.isfile(os.path.join(i, 'bin', 'uic')):
          print "QT - failed to find uic."
          return 0
       if not os.path.isfile(os.path.join(i, 'bin', 'moc')):
          print "QT - failed to find moc."
          return 0
       if not os.path.exists(os.path.join(i, 'lib')):
          print "QT - failed to find QT lib path."
          return 0
       if not os.path.exists(os.path.join(i, 'include')):
          print "QT - failed to find QT include path."
          return 0
       return 1
    else:
      if i==env['qt_directory']:
        print "QT directory not valid.  Failed QT test build."
        return 0
  return 0


def AttemptLinkWithVariables(context, variables, code, extension, prefix):
  return DoWithVariables(variables, prefix, lambda: context.TryLink(code, extension))

# end Functions to Check for QT -------------------------------------

# ----- Setup general paths need for build and install ----------------------

# Here are our installation paths:
idir_prefix = env['PREFIX']
idir_lib    = os.path.join(env['PREFIX'], 'lib')
idir_bin    = os.path.join(env['PREFIX'], 'bin')
idir_inc    = os.path.join(env['PREFIX'], 'include')
idir_data   = os.path.join(env['PREFIX'], 'share', 'lprof')

# setup the include paths include_search_path is set in build_config.py
env.Append(CPPPATH=include_search_path)

# add user specified paths to cppflags
if  env['added_cppflags'] != '' :
   env.Append(CPPPATH= env['added_cppflags'].split(' '))

env.Append(LIBS=['m', 'tiff', 'lcms', 'Xext', 'X11', 'qassistantclient'])

# set up LDFLAGS currently untested
if env['ldflags'] != '':
   env.Append(LDFLAGS= env['ldflags'].split(' '))

# set LIBPATH
env.Append(LIBPATH= lib_search_path)
# add user specified libpaths
if env['added_libpath'] != '':
   env.Append(LIBPATH = env['added_libpath'].split(' '))

# setup compiler flags
# XXX Why isn't this CFLAGS, rather than CCFLAGS? - gdt
# Why the lack of symmetry between C and CXX? -gdt
env.Append(CCFLAGS=env ['ccflags'])
env.Replace(CXXFLAGS=env ['cxxflags'] + ' -DQT_THREAD_SUPPORT')

# check system to make sure everything needed is available
# begin configuration section
# ---------------------------------------------------------

# check for QT
# Setup correct qt library
#if env['qt-mt-lib']:
env['QT_LIB'] = 'qt-mt'
#else:
#   env['QT_LIB'] = 'qt'
config=env.Configure(custom_tests = { 
    "CheckForQt" : CheckForQt
    })
if not config.CheckForQt():
  print "Failed to find valid QT environment."
  Exit(1)

# QT was found so finish QT setup 
print "QT was found - finish QT setup"
env.Tool('qt', ['$TOOL_PATH'])
env['QT_AUTOSCAN'] = 1

# QTDIR is set by the QT checking code.
# Use it to make sure everything needed for QT is setup
print "QTDIR = " + env['QTDIR']
env.Replace(QT_BINPATH = os.path.join ( env['QTDIR'] , 'bin')) 
print 'Setting up QT_BINPATH = ' + env['QT_BINPATH']
env.Replace(QT_UIC = os.path.join ( env['QTDIR'] , 'bin', 'uic')) 
print 'Setting up QT_UIC = ' + env['QT_UIC']
env.Replace(QT_MOC = os.path.join ( env['QTDIR'] , 'bin', 'moc')) 
print 'Setting up QT_MOC = ' + env['QT_MOC']
# env.Append(QT_DEBUG = 1)

env.Append(LIBPATH= env['QT_LIBPATH'])
env.Append(CPPPATH = env['QT_CPPPATH'])
print ''

# look for lcms headers
if not config.CheckHeader('lcms.h'):
   print "You need lcms header files to compile lprof."
   print "Exit code 1"
   Exit(1)

print ""

# check for libtiff
if not config.CheckHeader('tiff.h'):
   print "You need to have libtiff installed for tiff support to work."
   Exit(1)
        
print ""

# check for VIGRA headers
if not config.CheckCXXHeader('vigra/impex.hxx'):
        print "You need to have VIGRA installed."
        Exit(1)

# VIGRA check OK add it to the libraries list
env.Append(LIBS=['vigraimpex'])
print ""

# finished with checks
env=config.Finish()

# Add Argyll libraries (after QT check)
env.Append(LIBS=['rspl', 'numlib'])

# end configuration section
# --------------------------------------------------------

# make sure that all of the .h files are moc'ed
# ugly but it works
# moc0=os.path.join('src', 'checkerqt', 'profilechecker.h')
moc_sources0 = env.Moc(os.path.join('src', 'checkerqt', 'profilechecker.h'))
moc_sources1 = env.Moc(os.path.join('src', 'gammaqt', 'setgamma.h'))
moc_sources2 = env.Moc(os.path.join('src', 'IDqt', 'profileid.h'))
moc_sources3 = env.Moc(os.path.join('src', 'lprofqt', 'lprofmain.h'))
moc_sources4 = env.Moc(os.path.join('src', 'monqt', 'monitorvalues.h'))
moc_sources5 = env.Moc(os.path.join('src', 'parmsqt', 'profileparms.h'))
moc_sources6 = env.Moc(os.path.join('src', 'reference_inst_qt', 'installreffile.h'))
moc_sources7 = env.Moc(os.path.join('src', 'libqtlcmswidgets', 'qtlcmswidgets.h'))
moc_sources8 = env.Moc(os.path.join('src', 'wwfloatspinbox', 'wwfloatspinbox.h'))
# moc_sources9 = env.Moc(os.path.join('src', 'wwfloatspinbox', 'wwplugins.h'))
# moc_sources10 = env.Moc(os.path.join('src', 'wwfloatspinbox', 'wwclasses.h'))

# variables needed by the SConsript's
Export('env', 'sources', 'ICCtoIT8_sources', 'moc_sources0', 'moc_sources1', 'moc_sources2', 'moc_sources3', 'moc_sources4', 'moc_sources5', 'moc_sources6', 'moc_sources7', 'moc_sources8', 'idir_prefix', 'idir_lib', 'idir_bin', 'idir_inc', 'idir_data', 'moc_sources8')

# start the build

target_dir = '#' + SelectBuildDir(build_base_dir)
SConscript(os.path.join(target_dir,'SConscript'))
BuildDir(target_dir, source_base_dir, duplicate=0)
Default(os.path.join(target_dir,target_name))
Default(os.path.join(target_dir,target_name2))

print target_dir

# build Argyll stuff
SConscript([os.path.join (target_dir + '/argyll/numlib/SConscript'),
	    os.path.join (target_dir +'/argyll/rspl/SConscript')])

# this sets up an alias that will compile icc2it8
env.Alias('icc2it8', target_dir + '/ICCtoIT8') 

# Installation section -----------------------------------------------

icon_files = env.Install(os.path.normpath(idir_prefix + '/share/pixmaps/'), '#' + os.path.normpath('data/icons/lprof.png'))
env.Alias('install', icon_files);

desktop_files = env.Install(os.path.normpath(idir_prefix + '/share/applications/'), '#' + os.path.normpath('data/desktop/lprof.desktop'))
env.Alias('install', desktop_files)

pic_files = env.Install(os.path.normpath(idir_data + '/data/pics'), ['#' + os.path.normpath('data/pics/grayscale.tif'), '#' + os.path.normpath('data/pics/monitor patches.tif'), '#' + os.path.normpath('data/pics/MonitorTemplate.it8'), '#' + os.path.normpath('data/pics/party_4s.png'), '#' + os.path.normpath('data/pics/scandmo.png'), '#' + os.path.normpath('data/pics/srgb.it8')] )
env.Alias('install', pic_files)

profiles = env.Install(os.path.normpath(idir_data + '/data/profiles'), ['#' + os.path.normpath('data/profiles/CIEE.icm'), '#' + os.path.normpath('data/profiles/scandmo.icm'), '#' + os.path.normpath('data/profiles/sRGB Color Space Profile.icm')] )
env.Alias('install', profiles)

templates = env.Install(os.path.normpath(idir_data + '/data/template'), ['#' + os.path.normpath('data/template/IT8_19.ITX'), '#' + os.path.normpath('data/template/IT8_22.ITX'), '#' + os.path.normpath('data/template/KODAK.ITX')] )
env.Alias('install', templates)

help_files = env.Install(os.path.normpath(idir_data + '/data/help'), ['#' + os.path.normpath('data/help/lprof-help.html'), '#' + os.path.normpath('data/help/about.txt'), '#' + os.path.normpath('data/help/checker.html'), '#' + os.path.normpath('data/help/gamma.html'), '#' + os.path.normpath('data/help/handbook.png'), '#' + os.path.normpath('data/help/inst-ref.html'), '#' + os.path.normpath('data/help/lprof.png'), '#' + os.path.normpath('data/help/lprof-help.adp'), '#' + os.path.normpath('data/help/mon-val.html'), '#' + os.path.normpath('data/help/monitor.html'), '#' + os.path.normpath('data/help/preferences.html'), '#' + os.path.normpath('data/help/profile-id.html'), '#' + os.path.normpath('data/help/profile-parms.html'), '#' + os.path.normpath('data/help/corner.jpg'), '#' + os.path.normpath('data/help/ufraw.html'), '#' + os.path.normpath('data/help/ufraw-1.jpg'), '#' + os.path.normpath('data/help/ufraw-2.jpg'), '#' + os.path.normpath('data/help/ufraw-3.jpg')] )
env.Alias('install', help_files)

# the file list will have to be updated here every time a new translation is added.
language_files = env.Install(os.path.normpath(idir_data + '/data/translations'), ['#' + os.path.normpath('data/translations/lprof_de.qm'), '#' + os.path.normpath('data/translations/lprof_ru.qm'), '#' + os.path.normpath('data/translations/lprof_no.qm'), '#' + os.path.normpath('data/translations/lprof_fr.qm') ])

if target_dir == '#build/linux':
   chmod_share = Action("chmod -R 755 " + idir_data)
   env.AddPostAction(language_files, chmod_share)

env.Alias('install', language_files)

# end installation section -----------------------------------------------

# global Clean up section -------------------------------------

env.Clean('install', idir_data)


# end clean up section ------------------------------------


