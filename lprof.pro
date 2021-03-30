TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on debug

LIBS	+= -llcms -ltiff -L/usr/lib -lqassistantclient -lvigraimpex

INCLUDEPATH	+= src/liblprof src/argyll/h src/argyll/numlib src/argyll/rspl src/libqtlcmswidgets src/wwfloatspinbox

HEADERS	+= src/liblprof/lcmsprf.h \
	src/lprofqt/lprofmain.h \
	src/monqt/monitorvalues.h \
	src/checkerqt/profilechecker.h \
	src/IDqt/profileid.h \
	src/parmsqt/profileparms.h \
	src/gammaqt/setgamma.h \
	src/reference_inst_qt/installreffile.h \
	src/argyll/h/sort.h \
	src/argyll/numlib/dhsx.h \
	src/argyll/numlib/dnsq.h \
	src/argyll/numlib/ludecomp.h \
	src/argyll/numlib/numlib.h \
	src/argyll/numlib/numsup.h \
	src/argyll/numlib/powell.h \
	src/argyll/numlib/rand.h \
	src/argyll/numlib/sobol.h \
	src/argyll/numlib/svd.h \
	src/argyll/numlib/zbrent.h \
	src/argyll/rspl/rev.h \
	src/argyll/rspl/rspl.h \
	src/argyll/rspl/rspl_imp.h \
	src/libqtlcmswidgets/qtlcmswidgets.h \
	src/wwfloatspinbox/wwfloatspinbox.h

SOURCES	+= src/liblprof/cmshull.c \
	src/liblprof/cmslm.c \
	src/liblprof/cmslnr.c \
	src/liblprof/cmsmatn.c \
	src/liblprof/cmsmkmsh.c \
	src/liblprof/cmsmntr.c \
	src/liblprof/cmsoutl.c \
	src/liblprof/cmspcoll.c \
	src/liblprof/cmsprf.c \
	src/liblprof/cmsreg.c \
	src/liblprof/cmsscn.c \
	src/liblprof/cmssheet.c \
	src/libqtlcmswidgets/qtlcmswidgets.cpp \
	src/lprofqt/lprofmain.cpp \
	src/lprofqt/main.cpp \
	src/monqt/monitorvalues.cpp \
	src/checkerqt/profilechecker.cpp \
	src/IDqt/profileid.cpp \
	src/parmsqt/profileparms.cpp \
	src/checkerqt/qtcietonge.cpp \
	src/checkerqt/qtdrawcurve.cpp \
	src/gammaqt/setgamma.cpp \
	src/reference_inst_qt/installreffile.cpp \
	src/argyll/numlib/dhsx.c \
	src/argyll/numlib/dnsq.c \
	src/argyll/numlib/ludecomp.c \
	src/argyll/numlib/numsup.c \
	src/argyll/numlib/powell.c \
	src/argyll/numlib/rand.c \
	src/argyll/numlib/sobol.c \
	src/argyll/numlib/svd.c \
	src/argyll/numlib/zbrent.c \
	src/argyll/rspl/opt.c \
	src/argyll/rspl/rev.c \
	src/argyll/rspl/rspl.c \
	src/argyll/rspl/scat.c \
	src/argyll/rspl/spline.c \
	src/wwfloatspinbox/wwfloatspinbox.cpp

FORMS	= src/lprofqt/lprofmainbase.ui \
	src/monqt/monitorvaluesbase.ui \
	src/checkerqt/profilecheckerbase.ui \
	src/IDqt/profileidbase.ui \
	src/parmsqt/profileparmsbase.ui \
	src/reference_inst_qt/installreffilebase.ui \
	src/gammaqt/setgammabase.ui

IMAGES	= src/images/Norman_Koren-small.png \
	src/images/lcms.png \
	src/images/blue-grad.png \
	src/images/green-grad.png \
	src/images/orange-grad.png \
	src/images/yellow-grad.png \
	src/images/Norman_Koren-big.png

unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}


