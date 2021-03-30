#ifndef WWFLOATSPINBOX_H
#define WWFLOATSPINBOX_H

#include <qspinbox.h>

/**
 * @class wwFloatSpinBox
 * @brief Spin Box displaying real values
 *
 *
 */
class wwFloatSpinBox : public QSpinBox {
    Q_OBJECT
    Q_OVERRIDE( double maxValue READ maxValue  WRITE setMaxValue  )
    Q_OVERRIDE( double minValue READ minValue  WRITE setMinValue  )
    Q_OVERRIDE( double lineStep READ lineStep  WRITE setLineStep  )
    Q_OVERRIDE( double value    READ value     WRITE setValue     )
    Q_PROPERTY( int   precision READ precision WRITE setPrecision )
public:
    wwFloatSpinBox(QWidget *parent = 0, const char *name = 0);
    ~wwFloatSpinBox();
    double minValue() const;
    double maxValue() const;
    void  setMinValue( double );
    void  setMaxValue( double );
    double lineStep() const;                                              ///< Returns the line step.
    void  setLineStep( double );                                          ///< Sets the line step.
    double value() const;                                                 ///< Returns the value of the spin box.
    int precision() const;                                                ///< Returns the precision of the spin box.
signals:
    void valueChanged( double );                                          ///< Emitted when value changes
public slots:
    virtual void setValue( double value );                                ///< Sets the value of the spin box to value.
    virtual void setValue(int value);                                     ///< Sets the value of the spin box to value.
    virtual void setPrecision( int p);                                    ///< Sets the precision of the spin box to p.
protected:
    virtual QString	mapValueToText( int value );
    virtual int		mapTextToValue( bool* ok );
    int m_precision;

};

#endif
