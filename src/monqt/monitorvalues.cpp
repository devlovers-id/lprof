// $Id: monitorvalues.cpp,v 1.20 2006/04/28 22:57:21 hvengel Exp $
//  Little cms Profiler
//  Copyright (C) 1998-2001 Marti Maria 
// Copyright (C) 2005 Hal Engel
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
// Version 1.11

#include "monitorvalues.h"
#include "setgamma.h"
#include "lcmsprf.h"
#include "qtlcmswidgets.h"
#include "lprofmain.h"
#include <qlineedit.h>
#include <qpixmap.h>
#include "wwfloatspinbox.h"

static PROFILERDATA sys; 
// QButtonGroup* RGB_button_group;

// QAssistantClient *help;

bool gammaOK;

// QString def tr("ITU-R BT.709 (sRGB) - DEFAULT");

// Primaries table
static struct PrimTag {

    QString Name;
  double xRed, yRed, xGreen, yGreen, xBlue, yBlue;
  } PrimTable[] = 
  {

      { MonitorValues::tr("ITU-R BT.709 (sRGB) - DEFAULT"),   0.640, 0.330, 0.300, 0.600, 0.150, 0.060 },
      { MonitorValues::tr("SMPTE RP145-1994"),                0.64,  0.33,  0.29,  0.60,  0.15,  0.06 },
      { MonitorValues::tr("SMPTE-C (CCIR 601-1)"),            0.630, 0.340, 0.310, 0.595, 0.155, 0.070},
      { MonitorValues::tr("EBU Tech.3213-E"),                 0.630, 0.340, 0.310, 0.595, 0.155, 0.070 },
      { MonitorValues::tr("EBU [Walker98]"),                  0.64,  0.33,  0.30,  0.60,  0.15,  0.06 },
      { MonitorValues::tr("P22"),                             0.625, 0.340, 0.280, 0.595, 0.155, 0.070},
      { MonitorValues::tr("NTSC"),                            0.67,  0.33,  0.21,  0.71,  0.14,  0.08 },
      { MonitorValues::tr("HDTV"),                            0.670, 0.330, 0.210, 0.710, 0.150, 0.060 },
      { MonitorValues::tr("CIE"),                             0.7355,0.2645,0.2658,0.7243,0.1669,0.0085},
      { MonitorValues::tr("Dell"),                            0.625, 0.340, 0.275, 0.605, 0.150, 0.065},
      { MonitorValues::tr("LCD"),                             0.700, 0.300, 0.170, 0.700, 0.130, 0.075},
      { MonitorValues::tr("Samsung TFT"),                     0.610, 0.350, 0.315, 0.580, 0.150, 0.140},
      { MonitorValues::tr("Apple RGB"),                       0.625, 0.34,  0.28,  0.595, 0.155, 0.07 },
      { MonitorValues::tr("Wide Gamut RGB (700/525/450nm)"),  0.7347,0.2653,0.1152,0.8264,0.1566,0.0177},  
      { MonitorValues::tr("Short persistence [Foley96]"),     0.61,  0.35,  0.29,  0.59,  0.15,  0.063},
      { MonitorValues::tr("Long persistence  [Foley96]"),     0.62,  0.33,  0.21,  0.685, 0.15,  0.063},
      { MonitorValues::tr("Adobe RGB"),                       0.64,  0.33,  0.21,  0.71,  0.15,  0.06 },
      { MonitorValues::tr("User defined"),                    0.640, 0.330, 0.300, 0.600, 0.150, 0.060 }}; 

#define NPRIMARIES (int) (sizeof(PrimTable) / sizeof(struct PrimTag))

static struct WhitePntTag 
{
    QString Name;
    double xw, yw;
    int TempK;
} WhitePoints[] = 
{
    { MonitorValues::tr("D50 (Warm white)"), 0, 0, 5000},
    { MonitorValues::tr("D65 (daylight)"),   0.3127, 0.3291, 0},
    { MonitorValues::tr("D70"),              0, 0, 7000 },
    { MonitorValues::tr("D75"),              0, 0, 7500 },
    { MonitorValues::tr("D80"),              0, 0, 8000 },
    { MonitorValues::tr("D85"),              0, 0, 8500 },
    { MonitorValues::tr("D90"),              0, 0, 9000 },
    { MonitorValues::tr("D91"),              0, 0, 9100 },
    { MonitorValues::tr("D93 - Cold white"), 0, 0, 9300 },
    { MonitorValues::tr("CIE illuminant A"), 0.4476, 0.4074, 0},
    { MonitorValues::tr("CIE illuminant B"), 0.34842, 0.35161, 0},
    { MonitorValues::tr("CIE illuminant C"), 0.3101, 0.3162, 0},
    { MonitorValues::tr("CIE Illuminant E"), 0.333, 0.333, 0}, 
    { MonitorValues::tr("User defined (blackbody locus)"), 0, 0, 0}
};
                

				
#define NWHITES (int) (sizeof(WhitePoints) / sizeof(struct WhitePntTag))

monVal mon;
QValidator* Chromaticity;

QString v(double b)
{	
   static char Buffer[128];
   ::sprintf(Buffer, "%g", b);
   return (QString) Buffer;
}

QString mon_get_QTDIR()
{
    // qDebug("get_home_dir");
    const char* qtdir= "QTDIR";
    return (QString) getenv(qtdir);
}

MonitorValues::MonitorValues(QWidget *parent )
  : MonitorValuesBase(parent, "", 0, Qt::WStyle_SysMenu | Qt::WStyle_MinMax )
{      
    passSys a;
    a.get(&sys);
    mon.get(&mon);
    	 
    // Fill primaries combo

    Chromaticity = new QDoubleValidator(0.0, 2.0, 4, this);

    xRed   -> setValidator(Chromaticity);
    yRed   -> setValidator(Chromaticity);
    xGreen -> setValidator(Chromaticity);
    yGreen -> setValidator(Chromaticity);
    xBlue  -> setValidator(Chromaticity);
    yBlue  -> setValidator(Chromaticity);

 
    int i;
    
    ComboPrimaries->clear();
    for (i=0; i < NPRIMARIES; i++)
    {
        ComboPrimaries -> insertItem(tr(PrimTable[i].Name));
    }

    ComboPrimaries -> setCurrentItem(mon.items.ppos);
             
    if (mon.items.ppos< (NPRIMARIES-1)) 
        slotChangePrimaries();
    else 
    {
        xRed -> setText(v(sys.hdr.Primaries.Red.x));
        yRed -> setText(v(sys.hdr.Primaries.Red.y));
        xGreen -> setText(v(sys.hdr.Primaries.Green.x ));
        yGreen -> setText(v(sys.hdr.Primaries.Green.y ));
        xBlue-> setText(v(sys.hdr.Primaries.Blue.x));
        yBlue-> setText(v(sys.hdr.Primaries.Blue.y));
    }
    
    ComboBoxWP -> clear();
    for (i=0; i < NWHITES; i++)
        ComboBoxWP -> insertItem(tr(WhitePoints[i].Name));

    ComboBoxWP-> setCurrentItem(mon.items.wppos);
    slotChangeWhitePoint();
	
    TempWhitePoint -> setValue(mon.items.temp);
    
    GammaR -> setMinValue(1.61);
    GammaR -> setLineStep(0.01);
    GammaR -> setMaxValue(4.0);
    
    GammaG -> setMinValue(1.61);
    GammaG -> setLineStep(0.01);
    GammaG -> setMaxValue(4.0);
    
    GammaB -> setMinValue(1.61);
    GammaB -> setLineStep(0.01);
    GammaB -> setMaxValue(4.0);
    
    GammaR -> setValue(mon.items.rgamma);
    GammaG -> setValue(mon.items.ggamma);
    GammaB-> setValue(mon.items.bgamma);
}

MonitorValues::~MonitorValues()
{
    
    dialogSize ds;
        
    ds.put(2, this -> height(), this -> width());
    delete Chromaticity;
    Chromaticity = 0;
}

void MonitorValues::slotChangePrimaries()
{
    // char buff[32];
    
    int nSel = ComboPrimaries -> currentItem();

    BOOL IsUser = (nSel == NPRIMARIES - 1);

    xRed -> setEnabled(IsUser);
    yRed -> setEnabled(IsUser);
    xGreen -> setEnabled(IsUser);
    yGreen -> setEnabled(IsUser);
    xBlue -> setEnabled(IsUser);
    yBlue -> setEnabled(IsUser);

    if (nSel >= 0 && nSel < (NPRIMARIES -1)) 
    {
        // qDebug("slotChangePrimaries in if");

        sys.hdr.Primaries.Red.x = PrimTable[nSel].xRed;
        sys.hdr.Primaries.Red.y = PrimTable[nSel].yRed;
        sys.hdr.Primaries.Red.Y = 1;

        sys.hdr.Primaries.Green.x = PrimTable[nSel].xGreen;
        sys.hdr.Primaries.Green.y = PrimTable[nSel].yGreen;
        sys.hdr.Primaries.Green.Y = 1;

        sys.hdr.Primaries.Blue.x = PrimTable[nSel].xBlue;
        sys.hdr.Primaries.Blue.y = PrimTable[nSel].yBlue;
        sys.hdr.Primaries.Blue.Y = 1;

        xRed -> setText(v(sys.hdr.Primaries.Red.x));
        yRed -> setText(v(sys.hdr.Primaries.Red.y));
        xGreen -> setText(v(sys.hdr.Primaries.Green.x ));
        yGreen -> setText(v(sys.hdr.Primaries.Green.y ));
        xBlue-> setText(v(sys.hdr.Primaries.Blue.x));
        yBlue-> setText(v(sys.hdr.Primaries.Blue.y));
    }
    else	
    {
        sys.hdr.Primaries.Red.x = xRed->text().toDouble();
        sys.hdr.Primaries.Red.y = yRed->text().toDouble();
        sys.hdr.Primaries.Red.Y = 1;

        sys.hdr.Primaries.Green.x = xGreen->text().toDouble();
        sys.hdr.Primaries.Green.y = yGreen->text().toDouble();
        sys.hdr.Primaries.Green.Y = 1;

        sys.hdr.Primaries.Blue.x = xBlue->text().toDouble();
        sys.hdr.Primaries.Blue.y = yBlue->text().toDouble();
        sys.hdr.Primaries.Blue.Y = 1;

    }
}

void MonitorValues::slotChangeWhitePoint()
{             
    // qDebug("In slotChangeWhitePoint");
    cmsCIExyY wp;

    int nSel = ComboBoxWP-> currentItem();

    if (nSel == NWHITES - 1)
        TempWhitePoint -> show();
    else 
        TempWhitePoint -> hide();
			
    if (nSel >= 0 && nSel < NWHITES) 
    {
        wp.x = WhitePoints[nSel].xw;
        wp.y = WhitePoints[nSel].yw;
        wp.Y = 1;
        if (wp.x == 0 && wp.y == 0) 
        {
            int Temp = WhitePoints[nSel].TempK;
            if (Temp == 0)
               Temp = TempWhitePoint -> value();
        cmsWhitePointFromTemp(Temp, &wp);
        }
    }
	
    cmsxyY2XYZ(&sys.hdr.WhitePoint, &wp);
}


void MonitorValues::slotOK()
{
    passSys a;
    dialogSize ds;
        
    ds.put(2, this -> height(), this -> width());
    
    slotChangeWhitePoint();
    slotChangePrimaries();
    a.put(sys);
    mon.items.rgamma = GammaR -> value();
    mon.items.ggamma = GammaG -> value();
    mon.items.bgamma = GammaB -> value();
    mon.items.valid_mon=TRUE; 
    if (ComboBoxWP-> currentItem() == NWHITES -1)
        mon.items.temp=TempWhitePoint -> value();
    else
        mon.items.temp = 0;
    mon.items.wppos=ComboBoxWP-> currentItem();
    mon.items.ppos=ComboPrimaries -> currentItem();
    mon.put(mon); 
}

void MonitorValues::slotSetGamma_dialog()
{
    //  qDebug("monitorvalues::slotSetGamma_dialog");
    
    QWidget *d = QApplication::desktop();
    if (d -> height() < 768)
    {
        QMessageBox mb( QTranslator::tr((QString)"Display device too small"),
                        QTranslator::tr((QString)"Your screen does not have enough\n"
                                                 "resolution to display the gamma\n"
                                                 "setting dialog.  You need at least\n"
                                                 "a 1024 by 768 pixel display."),
                        QMessageBox::Critical,
                        QMessageBox::Ok | QMessageBox::Default,
                        QMessageBox::NoButton,  QMessageBox::NoButton);

        mb.exec();  
        return;
    } 
    
    mon.items.rgamma = GammaR -> value();
    mon.items.ggamma = GammaG -> value();
    mon.items.bgamma = GammaB -> value();
    gammaOK = false;
    SetGamma gammaVals(this);
    gammaVals.show();
    gammaVals.setWindowState(Qt::WindowFullScreen);
    gammaVals.exec();
    if (gammaOK)
    {
       GammaR -> setValue(mon.items.rgamma);
       GammaG -> setValue(mon.items.ggamma);
       GammaB-> setValue(mon.items.bgamma);
    }
        
}


void MonitorValues::slotHelpButton()
{
     QString path = QDir::currentDirPath() + "/help/mon-val.html";
     getHelp()->openAssistant ();
     getHelp()->showPage(path);
}

void monitorVal::get(monitorVal* out)
{
    // qDebug("in get mon");
    out->items=mon.items;
}

void monitorVal::put(monitorVal &in)
{
     // qDebug("in put mon");
    mon.items.rgamma=in.items.rgamma;
    mon.items.ggamma=in.items.ggamma;
    mon.items.bgamma=in.items.bgamma;
    mon.items.rchecked=in.items.rchecked;
    mon.items.gchecked=in.items.gchecked;
    mon.items.bchecked=in.items.bchecked;
    mon.items.linked=in.items.linked;
    gammaOK = true;
   
}
