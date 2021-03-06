// $Id: main.cpp,v 1.24 2006/06/04 02:14:23 hvengel Exp $
//  Little cms - profiler construction set
//Copyright (C) 2005 Hal Engel
//
// THIS SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
// EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
// WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//
// IN NO EVENT SHALL MARTI MARIA BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
// INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
// OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
// LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.
//
// This file is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
// As a special exception to the GNU General Public License, if you
// distribute this file as part of a program that contains a
// configuration script generated by Autoconf, you may include it under
// the same distribution terms that you use for the rest of that program.
//

#include <qapplication.h>
#include <qdir.h>
#include "lprofmain.h"
#include "qstring.h"
#include <qtranslator.h>
#include <qtextcodec.h>

#if !defined(__WIN32__) && (defined(_WIN32) || defined(WIN32))
#define __WIN32__
#endif

static QString prefix_path;

static void Create_Config_Dir(const QString a)
{
    // This code is not portable at this point
    // Needs to be updated to work on Windows and Mac

    prefix_path=a;
    // QString home_path;
    QString files;
    // BOOL homeDirExists;

     QDir* dir = new QDir ();

     // Find the path to the data directory.  It is either in the build directory
     // or in prefix/share/lprof/data.  Check for build directory first.
     // Need to do this not only for creating the users config directory
     // but also so the help system can locate the help files.
     // Otherwise it would be inside of the if below
     prefix_path.truncate(prefix_path.find( QString("/build") ));
     prefix_path = prefix_path + QString("/data");
     if (dir->exists(prefix_path, TRUE))
     {
         prefix_path = prefix_path + QString("/");
     }
     else
     {
          // not in the build directory so need to check for and remove /bin
          prefix_path = a;
          prefix_path.truncate(prefix_path.length()-3);
          prefix_path = prefix_path + QString("share/lprof/data/");
     }
     // check to see if config directory exists
     // if not create it and move template files into it
     // home_path =  dir->homeDirPath() + QString("/") + QString(".lprof");
     if (!dir->exists(dir->homeDirPath() + QString("/.lprof"), TRUE))
     {
         // homeDirExists=FALSE;

         QUrlOperator *urlOp = new QUrlOperator();
         if (!dir->exists(dir->homeDirPath() + QString("/.lprof"), TRUE))
             dir->mkdir(dir->homeDirPath() + QString("/.lprof"), TRUE);

        # ifdef __WIN32__
            // hide the user configuration directory
         QString home_path = dir->homeDirPath() + "/.lprof";
        files = QString("attrib +h \"") +  dir->convertSeparators(home_path) + QString("\"");
        system(files.latin1());
        # endif /* __WIN32__ */

        if (!dir->exists(dir->homeDirPath() + QString("/.lprof/config"), TRUE))
            dir->mkdir(dir->homeDirPath() + QString("/.lprof/config"), TRUE);

        if (!dir->exists(dir->homeDirPath() + QString("/.lprof/target_refs"), TRUE))
            dir->mkdir(dir->homeDirPath() + QString("/.lprof/target_refs"), TRUE);

        if (!dir->exists(dir->homeDirPath() + QString("/.lprof/temp"), TRUE))
            dir->mkdir(dir->homeDirPath() + QString("/.lprof/temp"), TRUE);

        if (!dir->exists(dir->homeDirPath() + QString("/.lprof/templates"), TRUE))
            dir->mkdir(dir->homeDirPath() + QString("/.lprof/templates"), TRUE);

        if (!dir->exists(dir->homeDirPath() + QString("/.lprof/measurements"), TRUE))
            dir->mkdir(dir->homeDirPath() + QString("/.lprof/measurements"), TRUE);

        // Copy the template files into the users config directory
        QDir d (prefix_path + QString("template/"));
       d.setFilter(QDir::Files | QDir::Hidden);
       const QFileInfoList *list = d.entryInfoList();
       QFileInfoListIterator it( *list );
       QFileInfo *fi;
       while ( (fi = it.current()) != 0 )
       {
          ++it;
          if (fi->fileName().contains("ITX", FALSE))
              urlOp -> copy( prefix_path + QString("template/") + fi->fileName(), dir->homeDirPath() + QString("/.lprof/templates/") );
       }
   }

   dir->setCurrent(prefix_path);  // used to find help files later on
   delete dir;
}

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    Create_Config_Dir(a.applicationDirPath());

    QTranslator translator( 0 );
    translator.load( (QString)"lprof_" + (QString) QTextCodec::locale(), QDir::currentDirPath() + "/translations" );
    a.installTranslator( &translator );
    lprofMain w;
    w.show();
    a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );
    return a.exec();
}
