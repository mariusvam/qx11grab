/***
* Author: Juergen Heinemann http://www.hjcms.de, (C) 2007-2008
* Copyright: See COPYING file that comes with this distribution
**/

#ifndef FFPROCESS_H
#define FFPROCESS_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QRect>
#include <QtGui/QX11Info>
#include <QtCore/QProcess>

class Settings;

class FFProcess : public QObject
{
    Q_OBJECT
    Q_CLASSINFO ( "Author", "Juergen Heinemann (Undefined)" )
    Q_CLASSINFO ( "URL", "http://qx11grab.hjcms.de" )

  private:
    Settings *m_Settings;
    const QX11Info xInfo;
    QString program, workdir;
    QStringList arguments;
    QProcess *m_QProcess;

    /**  Add Default X11Grab Command Line
     * @code
     *  -f x11grab -xerror -s (Width)x(Height) -i :(SCREEN).(qApp->SCREEN)+x,y
     * @endcode
     * @param r Dimension
     * @param o Options
     */
    void addVideoDevice ( const QRect &r, const QString &o );

    /**  Add Audio Command Line
     * @code
     *  -f oss -i /dev/dsp*
     * @endcode
     */
    void addAudioDevice();

    /** if not empty add -title,-author,-copyright,-comment,-genre */
    void addOptional();

    /** set Output filepath
     * @param p Directory
     * @param f Filename
     */
    const QString addOutput ( const QString &p, const QString &f );

  public:
    FFProcess ( QObject *parent = 0, Settings *settings = 0 );
    bool create ( const QRect & );
    ~FFProcess();

  public Q_SLOTS:
    bool start();
    bool stop();
    bool kill();
    bool isRunning();

  Q_SIGNALS:
    void running ();
    void down ();
    void message ( const QString & );
    void trigger ( const QString & );
    void errmessage ( const QString &, const QString & );

  private Q_SLOTS:
    void startCheck ();
    void errors ( QProcess::ProcessError );
    void exited ( int, QProcess::ExitStatus );

};

#endif
