#include "wwfloatspinbox.h"
#include <qvalidator.h>
#include <math.h>
#include <qlineedit.h>

#if QT_VERSION >= 0x030300
    #include "qlocale.h"
#endif


wwFloatSpinBox::wwFloatSpinBox(QWidget *parent, const char *name)
        : QSpinBox(parent, name) {
    m_precision = 1;
    QRegExp regExp("[0-9]*((,|.)[0-9]{1})?");
    setValidator(new QRegExpValidator(regExp, this));
    editor()->setAlignment(Qt::AlignRight);
    updateDisplay();
}



wwFloatSpinBox::~wwFloatSpinBox() {}

void wwFloatSpinBox::setValue( double value ) {
    double val = wwFloatSpinBox::value();
    QSpinBox::setValue((int)(pow(double(10),m_precision)*value));
    double val2=wwFloatSpinBox::value();
    if(val2!=val) emit valueChanged(val2);
}

void wwFloatSpinBox::setValue( int value ) {
    setValue((double)value/pow(double(10),m_precision));
}


double wwFloatSpinBox::minValue( ) const {
    double v = QSpinBox::minValue();
    return v/pow(double(10),m_precision);
}


double wwFloatSpinBox::maxValue( ) const {
    double v = QSpinBox::maxValue();
    return v/pow(double(10),m_precision);
}


void wwFloatSpinBox::setMinValue( double v) {
    QSpinBox::setMinValue((int)((v>=0 ? 1 : -1)*0.5+pow(double(10),m_precision)*v));
}


void wwFloatSpinBox::setMaxValue( double v) {
    QSpinBox::setMaxValue((int)((v>=0 ? 1 : -1)*0.5+pow(double(10),m_precision)*v));
}


double wwFloatSpinBox::value( ) const {
    double v = QSpinBox::value();
    return v/pow(double(10),m_precision);
}


QString wwFloatSpinBox::mapValueToText( int value ) {

    double v = value;
#if QT_VERSION >= 0x030300

    QLocale loc;
    return loc.toString(v/pow(double(10),m_precision), 'f', m_precision);
#else

    return QString::number(v/pow(double(10),m_precision), 'f', m_precision);
#endif
}


int wwFloatSpinBox::mapTextToValue( bool * ok ) {
#if QT_VERSION >= 0x030300
    QLocale loc;
    return (int)(0.5+pow(double(10),m_precision)*loc.toDouble(text(),ok));
#else
    return (int)(0.5+pow(double(10),m_precision)*text().toDouble(ok));
#endif

}


int wwFloatSpinBox::precision( ) const {
    return m_precision;
}

void wwFloatSpinBox::setPrecision( int p ) {
    if(p<0)
        p = 0;
    if(p>6)
        p = 6;
    double val = value();
    double ls = lineStep();
    setMaxValue((int)(maxValue()*pow(double(10),p-m_precision)));
    m_precision = p;
    setValue(val);
    setLineStep(ls);
    ((QRegExpValidator*)validator())->setRegExp("[0-9]*((,|.)[0-9]{,"+QString::number(m_precision)+"})?");
    updateDisplay();
}


double wwFloatSpinBox::lineStep( ) const {
    return ((double)QSpinBox::lineStep())/pow(double(10),m_precision);
}

void wwFloatSpinBox::setLineStep( double ls) {
    QSpinBox::setLineStep((int)((ls>=0 ? 1 : -1)*0.5+ls*pow(double(10),m_precision)));
}





