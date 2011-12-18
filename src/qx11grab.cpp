/**
* This file is part of the qx11grab project
*
* Copyright (C) Juergen Heinemann http://qx11grab.hjcms.de, (C) 2007-2012
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public License
* along with this library; see the file COPYING.LIB.  If not, write to
* the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301, USA.
**/

#include "qx11grab.h"
#include "settings.h"
#include "menubar.h"
#include "toolbar.h"
#include "systemtray.h"

#include "desktopinfo.h"
#include "rubberband.h"
#include "windowgrabber.h"
#include "grabberinfo.h"
#include "defaults.h"
#include "tableeditor.h"
#include "metadata.h"
#include "ffprocess.h"
#include "commandpreview.h"
#include "logviewer.h"
#include "exportdialog.h"
#include "bookmarkdialog.h"

/* QtCore */
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtCore/QTextStream>

/* QtGui */
#include <QtGui/QApplication>
#include <QtGui/QAction>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QIcon>
#include <QtGui/QKeySequence>
#include <QtGui/QMessageBox>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtGui/QRubberBand>
#include <QtGui/QToolBox>
#include <QtGui/QVBoxLayout>
#include <QtGui/QX11Info>

/* QtDBus */
#include <QtDBus/QDBusConnection>

QX11Grab::QX11Grab ( Settings *settings )
    : QMainWindow()
    , cfg ( settings )
    , m_FFProcess ( 0 )
{
  setObjectName ( QLatin1String ( "qx11grab" ) );
  setWindowTitle ( trUtf8 ( "QX11Grab" ) );
  setMinimumWidth ( 500 );
  setMinimumHeight ( 450 );

  QDBusConnection p_dbus = QDBusConnection::sessionBus();

  QIcon boxIcon = getThemeIcon ( "qx11grab" );
  setWindowIcon ( boxIcon );

  m_menuBar = new MenuBar ( this );
  setMenuBar ( m_menuBar );

  m_toolBar = new ToolBar ( this );
  addToolBar ( m_toolBar );

  statusBar()->show();

  m_FFProcess = new FFProcess ( this, cfg );

  QWidget* layerWidget = new QWidget ( this );

  QVBoxLayout* verticalLayout = new QVBoxLayout ( layerWidget );

  QToolBox* toolBox = new QToolBox ( layerWidget, Qt::Widget );
  toolBox->setObjectName ( QLatin1String ( "toolbox" ) );
  toolBox->setBackgroundRole ( QPalette::Window );
  verticalLayout->addWidget ( toolBox );

  m_grabberInfo = new GrabberInfo ( toolBox );
  toolBox->addItem ( m_grabberInfo, boxIcon, trUtf8 ( "Application" ) );

  m_defaults = new Defaults ( toolBox );
  toolBox->addItem ( m_defaults, boxIcon, trUtf8 ( "Defaults" ) );

  m_metaData = new MetaData ( toolBox );
  m_metaData->setToolTip ( QString::fromUtf8 ( "-metadata" ) );
  toolBox->addItem ( m_metaData, boxIcon, trUtf8 ( "Metadata" ) );

  m_videoEditor = new TableEditor ( toolBox );
  m_videoEditor->setToolTip ( QString::fromUtf8 ( "-vcodec" ) );
  toolBox->addItem ( m_videoEditor, boxIcon, trUtf8 ( "Video" ) );

  m_audioEditor = new TableEditor ( toolBox );
  m_audioEditor->setToolTip ( QString::fromUtf8 ( "-acodec" ) );
  toolBox->addItem ( m_audioEditor, boxIcon, trUtf8 ( "Audio" ) );

  m_commandPreview = new CommandPreview ( toolBox );
  toolBox->addItem ( m_commandPreview, boxIcon, trUtf8 ( "FFmpeg" ) );

  layerWidget->setLayout ( verticalLayout );
  setCentralWidget ( layerWidget );

  TimeOutMessages = 5000;

  /* init Actions */
  loadStats();
  createEnviroment();
  createSystemTrayIcon();

  /* Signals */
  connect ( m_FFProcess, SIGNAL ( message ( const QString & ) ),
            this, SLOT ( pushInfoMessage ( const QString & ) ) );

  connect ( m_FFProcess, SIGNAL ( errmessage ( const QString &, const QString & ) ),
            this, SLOT ( pushErrorMessage ( const QString &, const QString & ) ) );

  connect ( m_FFProcess, SIGNAL ( trigger ( const QString & ) ),
            this, SLOT ( pushToolTip ( const QString & ) ) );

  connect ( m_grabberInfo, SIGNAL ( screenDataChanged ( int ) ),
            this, SLOT ( toRubber ( int ) ) );

  connect ( m_grabberInfo, SIGNAL ( showRubber ( bool ) ),
            this, SLOT ( showRubber ( bool ) ) );

  connect ( m_grabberInfo, SIGNAL ( postUpdate () ),
            this, SLOT ( perparePreview () ) );

  connect ( m_videoEditor, SIGNAL ( postUpdate () ),
            this, SLOT ( perparePreview () ) );

  connect ( m_audioEditor, SIGNAL ( postUpdate () ),
            this, SLOT ( perparePreview () ) );

  connect ( this, SIGNAL ( stopRecording () ),
            m_FFProcess, SLOT ( stop () ) );

  connect ( this, SIGNAL ( killProcess () ),
            m_FFProcess, SLOT ( kill () ) );

  connect ( m_FFProcess, SIGNAL ( down () ),
            this, SLOT ( setActionsBack () ) );

  connect ( m_commandPreview, SIGNAL ( dataSaved ( const QStringList & ) ),
            this, SLOT ( updateCommandLine ( const QStringList & ) ) );
}

void QX11Grab::record()
{
  if ( ! m_FFProcess )
    return;

#ifdef QT_NO_EXCEPTIONS

  startRecord ();

#else

  try
  {
    startRecord ();
  }
  catch ( const char* mess )
  {
    qWarning ( "No XServer where found: (%s)", mess );
  }

#endif
}

void QX11Grab::stop()
{
  if ( ! m_FFProcess )
    return;

  if ( m_FFProcess->isRunning() )
    m_FFProcess->stop ();
}

void QX11Grab::createEnviroment()
{
  m_DesktopInfo = new DesktopInfo ( this );
  FrameMode desktop = m_DesktopInfo->grabScreenGeometry ( centralWidget() );
//  setDepthBox->setValue ( desktop.depth );

  m_RubberBand = new RubberBand ( qApp->desktop()->screen() );
  connect ( m_RubberBand, SIGNAL ( error ( const QString &, const QString & ) ),
            this, SLOT ( pushErrorMessage ( const QString &, const QString & ) ) );

  toRubber ( 1 );
  if ( m_grabberInfo->RubberbandIsVisible() )
    m_RubberBand->show();
  else
    m_RubberBand->hide();
}

void QX11Grab::createSystemTrayIcon()
{
  m_systemTray = new SystemTray ( this );
  connect ( m_systemTray, SIGNAL ( activated ( QSystemTrayIcon::ActivationReason ) ),
            this, SLOT ( systemTrayWatcher ( QSystemTrayIcon::ActivationReason ) ) );

  m_systemTray->show();
}

void QX11Grab::swapRubberBand ()
{
  if ( m_grabberInfo->RubberbandIsVisible() )
  {
    m_grabberInfo->setRubberbandCheckBox ( false );
    m_RubberBand->hide();
  }
  else
  {
    m_grabberInfo->setRubberbandCheckBox ( true );
    m_RubberBand->show();
  }
}

/**
* Ein/Ausblenden funktion für die Gummibandanzeige.
*/
void QX11Grab::showRubber ( bool b )
{
  if ( b )
  {
    m_grabberInfo->setRubberbandCheckBox ( true );
    m_RubberBand->show();
  }
  else
  {
    m_grabberInfo->setRubberbandCheckBox ( false );
    m_RubberBand->hide();
  }
}

/**
* Lese die Fenster Geometrien neu ein.
* @note Wird immer nur beim Start und show() und hide() aufgerufen!
*/
void QX11Grab::loadStats()
{
  if ( cfg->contains ( "window/position" ) )
    move ( cfg->value ( "window/position", pos() ).toPoint() );

  if ( cfg->contains ( "window/size" ) )
    resize ( cfg->value ( "window/size", size() ).toSize() );

  if ( cfg->contains ( "window/state" ) )
    restoreState ( cfg->value ( "window/state", QByteArray() ).toByteArray() );

  loadSettings();

  QFileInfo log ( qx11grabLogfile() );
  m_toolBar->setActionsEnabled ( log.exists() );
}

/**
* Fenster Verhältnisse Speichern
* @note Wird immer nur bei show() und hide() aufgerufen!
*/
void QX11Grab::saveStats()
{
  cfg->setValue ( "window/state", saveState ( 0 ) );
  cfg->setValue ( "window/position", pos() );
  cfg->setValue ( "window/size", size() );
}

/**
* Sende verschieben Info an Klasse @class RubberBand
*/
void QX11Grab::toRubber ( int i )
{
  if ( i < 1 )
    return;

  if ( ! m_RubberBand )
    return;

  QRect r = m_grabberInfo->getRect();
  m_RubberBand->resize ( r.width(), r.height() );
  m_RubberBand->move ( r.x(), r.y() );
  perparePreview();
}

/**
* Fenster Dimensionen abgreifen
*/
void QX11Grab::grabFromWindow()
{
  if ( ! m_RubberBand )
    return;

  WindowGrabber* grabber = new WindowGrabber ( this );
  QRect rect = grabber->grabWindowRect();

  if ( rect.isValid() )
  {
    m_grabberInfo->setScreenWidth ( rect.width() );
    m_grabberInfo->setScreenHeight ( rect.height() );
    m_grabberInfo->setScreenX ( rect.x() );
    m_grabberInfo->setScreenY ( rect.y() );
    m_grabberInfo->setScreenMode ( trUtf8 ( "grabbed Dimension" ) );
    toRubber ( 1 );
  }

  delete grabber;
}

/**
* Statusleisten Aktionen verarbeiten
*/
void QX11Grab::systemTrayWatcher ( QSystemTrayIcon::ActivationReason type )
{
  switch ( type )
  {
    case QSystemTrayIcon::Trigger:
    {
      if ( isVisible() )
        hide();
      else
        show();
    }
    break;

    default:
      return;
  }
}

/**
* Beim Maximieren die Fenster Geometrie laden
*/
void QX11Grab::showEvent ( QShowEvent * )
{
  loadStats();
}

/**
* Beim Minimieren die Fenster Geometrie speichern
*/
void QX11Grab::hideEvent ( QHideEvent * )
{
  saveStats();
}

/**
* Vor dem beenden einige prüfungen durchführen
*/
void QX11Grab::closeEvent ( QCloseEvent *ev )
{
  if ( m_FFProcess->isRunning() )
  {
    QMessageBox::warning ( this, trUtf8 ( "Warning" ), trUtf8 ( "Recorder is running." ) );
    ev->ignore();
    m_FFProcess->deleteLater ();
  }
  saveStats();
}

/**
* Informationen an die Statusleiste senden.
*/
void QX11Grab::pushInfoMessage ( const QString &txt )
{
  if ( m_systemTray )
    m_systemTray->showMessage ( trUtf8 ( "Info" ), txt,
                                QSystemTrayIcon::Information, TimeOutMessages );

  if ( ! m_FFProcess->isRunning() )
    m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
}

/**
* Fehler Meldungen an die Statusleiste senden.
*/
void QX11Grab::pushErrorMessage ( const QString &title, const QString &txt )
{
  if ( m_systemTray )
    m_systemTray->showMessage ( title, txt, QSystemTrayIcon::Critical, TimeOutMessages );

  if ( ! m_FFProcess->isRunning() )
    m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
}

/**
* Tips an die Statusleiste senden.
*/
void QX11Grab::pushToolTip ( const QString &txt )
{
  if ( m_systemTray )
    m_systemTray->setToolTip ( txt );
}

/**
* Die Daten wurden von @class CommandLineEdit modifiziert
* und müssen neu geschrieben werden.
**/
void QX11Grab::updateCommandLine ( const QStringList &cmd )
{
  if ( cmd.contains ( m_defaults->binary() ) )
    cfg->setValue ( QLatin1String ( "CurrentCommandLine" ), cmd );
}

/**
* Starte die Aufnahme und Sperre gleichzeitig
* einige Aktionen um doppel Klicks zu vermeiden.
*/
void QX11Grab::startRecord()
{
  if ( ! m_RubberBand->isScalability() )
    return;

  if ( m_FFProcess->create ( m_grabberInfo->getRect() ) )
  {
    m_systemTray->setActionsEnabled ( true );
    m_menuBar->setActionsEnabled ( true );
    showRubber ( false );
    QStringList cmd = cfg->value ( QLatin1String ( "CurrentCommandLine" ) ).toStringList();
    if ( m_FFProcess->start ( cmd ) )
    {
      m_systemTray->setIcon ( getThemeIcon ( "media-record" ) );
    }
  }
  else
    QMessageBox::critical ( this, trUtf8 ( "Error" ), trUtf8 ( "qx11grap not started" ) );

  m_toolBar->setActionsEnabled ( true );
}

/**
* Beim beenden einer Aufnahme alles in die Neutrale Stellung bringen.
*/
void QX11Grab::setActionsBack()
{
  m_systemTray->setActionsEnabled ( false );
  m_menuBar->setActionsEnabled ( false );
  m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
}

/**
* Lade beim Start des Dialoges alle Einstellungen.
*/
void QX11Grab::loadSettings()
{
  m_grabberInfo->load ( cfg );
  m_defaults->load ( cfg );
  m_metaData->load ( cfg );
  m_videoEditor->load ( QString::fromUtf8 ( "VideoOptions" ), cfg );
  m_audioEditor->load ( QString::fromUtf8 ( "AudioOptions" ), cfg );
  perparePreview();
}

/**
* Speichere alle Einstellungen.
*/
void QX11Grab::saveSettings()
{
  m_grabberInfo->save ( cfg );
  m_defaults->save ( cfg );
  m_metaData->save ( cfg );
  m_videoEditor->save ( QString::fromUtf8 ( "VideoOptions" ), cfg );
  m_audioEditor->save ( QString::fromUtf8 ( "AudioOptions" ), cfg );
}

/**
* Log Dialog öffnen
*/
void QX11Grab::openLogFileDialog()
{
  QFileInfo log ( qx11grabLogfile() );
  if ( log.isReadable() )
  {
    LogViewer dialog ( log, centralWidget() );
    dialog.exec();
  }
}

/**
* Kommando Zeile für Textausgabe Aufbereiten.
*  X11grab indev AVOptions:
*  -video_size    string A string describing frame size, such as 640x480 or hd720.
*  -framerate     string
*  -draw_mouse    int    Draw the mouse pointer.
*  -follow_mouse  int    Move the grabbing region when the mouse pointer reaches within specified amount of pixels to the edge of region.
*     centered           Keep the mouse pointer at the center of grabbing region when following.
*  -show_region   int    Show the grabbing region.
*/
void QX11Grab::perparePreview()
{
  QStringList commandLine;

  commandLine << m_defaults->binary ();
  commandLine << "-xerror" << "-f" << "x11grab";

  QRect r = m_grabberInfo->getRect();
  commandLine << "-framerate" << QString::number ( m_grabberInfo->frameRate() );
  commandLine << "-video_size" << QString ( "%1x%2" ).arg (
      QString::number ( r.width() ), QString::number ( r.height() )
  );
  commandLine << "-r" << QString::number ( m_grabberInfo->frameRate() );

  QX11Info xInfo;
  commandLine << "-i" << QString ( ":%1.%2+%3,%4 " ) .arg (
      QString::number ( xInfo.screen() ),
      QString::number ( xInfo.appScreen() ),
      QString::number ( r.x() ),
      QString::number ( r.y() )
  );

  // Audio System
  if ( m_grabberInfo->soundEnabled() )
    commandLine << m_defaults->audioDeviceData();

  // Video Options
  commandLine << m_videoEditor->getCmd ();

  // Meta Daten
  if ( m_grabberInfo->metaEnabled() )
    commandLine << m_metaData->getCmd ();

  // Audio Aufnahme
  if ( m_grabberInfo->soundEnabled() )
    commandLine << m_audioEditor->getCmd ();

  // Output Options
  commandLine << "-y" << m_defaults->output();

  m_commandPreview->setCommandLine ( commandLine );

  cfg->setValue ( QLatin1String ( "CurrentCommandLine" ), commandLine );
}

/**
* Liest die aktuelle Kommandozeile aus der Konfiguration
*/
const QString QX11Grab::currentCommandLine()
{
  QStringList cmd = cfg->value ( QLatin1String ( "CurrentCommandLine" ) ).toStringList();
  return cmd.join ( " " );
}

/**
* Aktuelle Kommando Zeile in Shell Script exportieren!
*/
void QX11Grab::exportCommand()
{
  ExportDialog* d = new ExportDialog ( currentCommandLine(), this );
  if ( d->exec() == QFileDialog::Accepted )
    statusBarMessage ( trUtf8 ( "commandline exported" ) );

  delete d;
}

/**
* Leszeichen Editor
*/
void QX11Grab::openBookmarkEditor()
{
  BookmarkDialog* d = new BookmarkDialog ( cfg, this );
  d->exec();
  delete d;
}

/**
* Zeichenketten aus der Konfiguration lesen
*/
const QString QX11Grab::getSettingsValue ( const QString &key )
{
  return cfg->value ( key, "" ).toString();
}

/**
* Schreibe Nachricht in das Meldungs Label
*/
void QX11Grab::statusBarMessage ( const QString &msg, int timeout )
{
  statusBar()->showMessage ( msg, timeout );
}

/** Wird für DBUS und ItemDelegation benötigt! */
const QString QX11Grab::audioCodec()
{
  return m_audioEditor->selectedCodec();
}

/** Wird für DBUS und ItemDelegation benötigt! */
const QString QX11Grab::videoCodec()
{
  return m_videoEditor->selectedCodec();
}

QX11Grab::~QX11Grab()
{}
