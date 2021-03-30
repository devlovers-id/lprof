// $Id: setgamma.h,v 1.7 2006/03/12 20:42:23 hvengel Exp $
//  Lttle cms Profiler
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
#ifndef SETGAMMA_H
#define SETGAMMA_H


#include <qstringlist.h>
#include <qbuttongroup.h>
#include <qlayout.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qfiledialog.h>
#include <qmultilineedit.h>
#include <qtabwidget.h>
#include <qtextview.h>
#include <qprogressbar.h>
#include <qdir.h>
#include <qapplication.h>
#include <qtable.h>
#include <qvalidator.h>
#include <qimage.h>
#include <qtooltip.h>
#include "setgammabase.h"
#include "lcmsprf.h"

class  SetGamma : public SetGammaBase
{
  Q_OBJECT
    
public:
    
  SetGamma(QWidget *parent = 0 );
  ~SetGamma();
  
public slots:
    	virtual void slotChangeGamma();
	virtual void slotGammaSlider();
	virtual void slotGammaRadio();

  
      
protected:
     void SlidersToValues();
     void ValuesToLabels();
    void RenderGamma();
    void ProofGamma();
    void FillResultsGrid(void);
    void slotOK();    
    void slotChangeStrategy();
    void slotHelpButton();
    void Set_dwMask_Gamma ();
    void Gamma_loop (QImage in, QImage out);
    
    QPixmap* GammaPix;
    QImage GammaBitmap;
    QImage Screen;
    QPixmap ScreenPixmap;
    
    QPixmap* ChartPix;
    QImage ChartBitmap;
    QImage ChartScreen;
    QPixmap ChartScreenPixmap;
};

#endif