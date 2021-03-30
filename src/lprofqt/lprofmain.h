// $Id:
//  Little cms - profiler construction set
//  Copyright (C) 1998-2001 Marti Maria
//  Copyright (C) 2005 Hal Engel
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

#ifndef LPROFMAIN_H
#define LPROFMAIN_H

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
#include <qtooltip.h>


#include "lprofmainbase.h"
#include "lcmsprf.h"

#include <qvariant.h>
#include <qpixmap.h>
#include <qmainwindow.h>
#include <qfiledialog.h>
#include <qcombobox.h>
#include <qtabwidget.h>
#include <qcanvas.h>
#include <qimage.h>
#include <qpainter.h>
#include <qmultilineedit.h>
#include <qtextstream.h>
#include <qapplication.h>
#include <qspinbox.h>
#include <qstatusbar.h>
#include <qmessagebox.h>
#include <qradiobutton.h>
#include <qassistantclient.h>
#include <qsettings.h>
#include "vigra/stdimage.hxx"

// static QAssistantClient* help;

QAssistantClient* getHelp();

class dialogSize
{
    public:
        void put (int d, int h, int w);
};
 
class passSys
{
public:
         void get( PROFILERDATA* out);
         void put(PROFILERDATA &in);
};
  

struct target_template
{
    QString targetFile, templateFile;
    int targPos, tempPos;
};

class targetTemplate
{
public:
    target_template items;
    void put(targetTemplate &in);
};


struct monval
{
    int wppos;
    int ppos;
    int temp;
    int linked;
    BOOL rchecked;
    BOOL gchecked;
    BOOL bchecked;
    double rgamma;
    double ggamma;
    double bgamma;
    BOOL valid_mon;
    
};

class monVal
{
public:
    monval items;
    void get(monVal* out);
    void put(monVal &in);
};
    
// measurement tool stuff

class QVBoxLayout;

// RTTI numbers 

#define imageRTTI 0xF0538
#define gridRTTI  0xF0539


// The back image

class ImageItem: public QCanvasRectangle {
	

public:

    ImageItem(QString ImageFile, 
			  const QString MonitorProfile, 
			  const QString ScannerProfile, 
			  QCanvas *canvas );

    int rtti () const { return imageRTTI; }
 
	void Reload(QString ImageFile, 
			  const QString MonitorProfile, 
			  const QString ScannerProfile, 
			  QCanvas *canvas );

    QImage image;
    QPixmap pixmap, proof;
	double sx, sy;
	BOOL lProof;
   // vigra::BasicImage image_data;  // from vigra

protected:



    void drawShape( QPainter & );	
	void TransformImage(const QString OutputProfile, 
		  const QString InputProfile, 
                  vigra::DRGBImage& p,
                    QImage& pout);

	
};


// Picker layout. Coords are in 0...0xffff units

#define LAYOUT_MAX	(65535.)

class GridLayout {

public:

	void ScaleTo(int w, int h, QRect& Scaled);
	int  Scale(int n, int max);
 
	PATCH   p;
	QRect   rect;	

};




class GridItem: public QCanvasRectangle {
	

public:

    GridItem( QCanvas *canvas, QString TemplateFile, int SafeFrame );
    int rtti () const { return gridRTTI; }


	void Pick(QString InputProfile, QImage& Image, double sx, double sy);
	
	QList<GridLayout> Layout;

	// double  UnShearFactor;
	double  SafeFrameFactor;


protected:

	int  ScaleX(int n) { return (int) ((double) (n * width())  / LAYOUT_MAX); }
	int  ScaleY(int n) { return (int) ((double) (n * height()) / LAYOUT_MAX); } 

	void ComputePatchRects(QRect& OurViewport, GridLayout* Patch, 
								 QRect& Frame, QRect& HotZone);

	void ComputeViewport(QRect& OurViewport, double sx, double sy);


    void drawShape(QPainter & );	
	
};


// Statistics

class Statistic {

public:
		
	Statistic() { clear(); }
 

	void clear(void) { n = x = x2 = 0; }


	void	Add(double v) { x += v; x2 += (v * v); n  += 1.0;} 
		
	double	Std(void)	  { return sqrt((n * x2 - x * x) / (n*(n-1))); }
	double  Mean(void)	  { return x/n; }

	
protected:

	double n, x, x2;    
    
};


// end measurment tool stuff

struct checkerparms
{
    QString profile;
    QString msheet;
    QString ref_dir;
    double cur_ref;
    QString mon_profile_dir;
    QString mon_profile_file_name;
};

class checkerP
{
public:
    checkerparms items;
    void get(checkerP &out);
};

struct ref_file_parms
{
    QString temp_dir;
    QString ref_dir;
    QString home_dir;
};

class ref_file
{
public:
    ref_file_parms items;
    void get(ref_file &out);
};

class lprofMain;

class FigureEditor : public QCanvasView {

    Q_OBJECT

public:

	 FigureEditor(lprofMain &main_dialog, QCanvas& c, QWidget* parent=0, const char* name=0, WFlags f=0) : QCanvasView(&c,parent,name,f), maindialog(main_dialog) {}
    void clear();
    void resizeEvent( QResizeEvent* e );
    
protected:
    void contentsMousePressEvent(QMouseEvent*);
    // void contentsMouseMoveEvent(QMouseEvent*);

signals:
    void status(const QString&);

protected:
    FigureEditor& operator=( const FigureEditor& rhs );
	
    GridItem* moving;
	GridItem* sizing;
	GridItem* unshearing;
    QPoint moving_start;
    lprofMain &maindialog;	
};

class lprofMain : public lprofMainBase
{
    Q_OBJECT

public:
    lprofMain( QWidget* parent = 0);
    ~lprofMain();
 
    virtual void Reload();
    virtual void AddTemplate();
    virtual void slotLoad();
    virtual void slotPick();
    
    void updateTargetList(target_template &in);
    
    virtual void slotTargetRefChanged();
       
    FigureEditor*   FigEditor;
    // QAssistantClient* help;
        
private slots:
     virtual void ProfIDpushButton_clicked();
    virtual void ProfileParmsbutton();
    virtual void monitorvalues_clicked();
    virtual void ProfChecker_clicked();
    virtual void setProfileIDText();
    virtual void setProfileParmsText();
    virtual void slotProof();
    virtual void slotHelpButton();
   // virtual void setupHelp();
   
protected:
    
    void SaveIT8(const char *fileName);
	void LoadIT8(const char *fileName);
	void DumpResultsToIT8(QList<GridLayout>& Layout);
	void DoMonitorProfile();
        void init_mon();
        void saveProfileDetails();

	
	QCanvas*         Canvas;
	QVBoxLayout* CanvasFrameLayout;
	GridItem*         TheGrid;
	ImageItem*     TheImage; 
	// FigureEditor*   FigEditor;	

	QString	          CurrentTemplate;
	double             CurrentSafeFrame;

	QString           fn;
        QSettings* settings;
    	    
private:
    virtual void slotSelectOutputFile();
    virtual void slotIsAllReady();
    virtual void OutputFileNameChanged();
    virtual void Go_button_clicked();
    // void SetScanoutFile(const QString ScanoutFIle);
    void slotSelectScanoutIT8();
    void ValuesToLabels();
    void ValuesToControls();
    void SlidersToValues();
    void ControlsToValues();
    void slotChangeStrategy();
    // virtual void slotGO();
    void slotTargetChanged();
    BOOL LoadConfig();
    void SaveConfig();
    void DoProfile();
    void slotUpdateLabels(); 
    virtual void SetOutputFile( const QString OutputFile );
    void FillResultsGrid(void);
    void RenderGamma();
    void ProofGamma();
    void init_measurement_tool();
    void SetGoButton(QString Caption, BOOL lEnable);
    void SetGoButton_2(QString Caption, BOOL lEnable);
    void DoScannerProfile();
    void slotSelSheet();
    void init_targetlist();
    virtual void slotInstallReference();
    virtual void tabChanged();
    virtual void OutputFileNameChangedMon();
    virtual void getConfigParmsandID(QString profile);
};


// The dialog
#endif // LPROFMAIN_H