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

#include "mainwindow.h"
#include "settings.h"
#include "menubar.h"
#include "toolbar.h"
#include "systemtray.h"

#include "desktopinfo.h"
#include "rubberband.h"
#include "windowgrabber.h"
#include "grabberinfo.h"
#include "tableeditor.h"
#include "metadata.h"
#include "ffprocess.h"
#include "listener.h"
#include "commandpreview.h"
#include "logviewer.h"
#include "exportdialog.h"
#include "bookmarkdialog.h"
#include "bookmark.h"
#include "configdialog.h"

/* QtCore */
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QTextStream>

/* QtGui */
#include <QtGui/QApplication>
#include <QtGui/QAction>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QIcon>
#include <QtGui/QInputDialog>
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
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>

MainWindow::MainWindow ( Settings * settings )
    : QMainWindow()
    , cfg ( settings )
    , m_FFProcess ( 0 )
    , m_listener ( 0 )
{
  setObjectName ( QLatin1String ( "qx11grab" ) );
  setWindowTitle ( QString::fromUtf8 ( "QX11Grab (%1)[*]" ).arg ( QX11GRAB_VERSION ) );
  setMinimumWidth ( 450 );
  setMinimumHeight ( 400 );
  setWindowFlags ( ( windowFlags() | Qt::WindowContextHelpButtonHint ) );
  setContentsMargins ( 0, 5, 0, 5 );

  QIcon boxIcon = getThemeIcon ( "qx11grab" );
  setWindowIcon ( boxIcon );

  m_menuBar = new MenuBar ( this );
  setMenuBar ( m_menuBar );

  m_toolBar = new ToolBar ( this );
  addToolBar ( m_toolBar );

  statusBar()->show();

  // Process Management
  m_FFProcess = new FFProcess ( this, cfg );
  m_listener = new Listener ( this );

  QWidget* layerWidget = new QWidget ( this );
  layerWidget->setContentsMargins ( 0, 0, 0, 0 );

  QVBoxLayout* verticalLayout = new QVBoxLayout ( layerWidget );
  verticalLayout->setContentsMargins ( 0, 0, 0, 0 );

  QToolBox* toolBox = new QToolBox ( layerWidget, Qt::Widget );
  toolBox->setObjectName ( QLatin1String ( "toolbox" ) );
  toolBox->setBackgroundRole ( QPalette::Window );
  toolBox->setContentsMargins ( 0, 5, 0, 10 );
  verticalLayout->addWidget ( toolBox );

  // Dimension {
  m_grabberInfo = new GrabberInfo ( toolBox );
  toolBox->addItem ( m_grabberInfo, boxIcon, trUtf8 ( "Dimension" ) );
  // } Dimension

  // MetaData {
  m_metaData = new MetaData ( toolBox );
  m_metaData->setToolTip ( QString::fromUtf8 ( "-metadata" ) );
  toolBox->addItem ( m_metaData, boxIcon, trUtf8 ( "Metadata" ) );
  // } MetaData

  // vCodec {
  m_videoEditor = new TableEditor ( toolBox );
  m_videoEditor->setToolTip ( QString::fromUtf8 ( "-vcodec" ) );
  toolBox->addItem ( m_videoEditor, boxIcon, trUtf8 ( "Video" ) );
  // } vCodec

  // aCodec {
  m_audioGroupBox = new QGroupBox ( toolBox );
  m_audioGroupBox->setFlat ( true );
  m_audioGroupBox->setCheckable ( true );
  m_audioGroupBox->setTitle ( trUtf8 ( "Audio Recording" ) );
  /*: WhatsThis */
  m_audioGroupBox->setWhatsThis ( trUtf8 ( "enable/disable audio recording in the captured video" ) );
  toolBox->addItem ( m_audioGroupBox, boxIcon, trUtf8 ( "Audio" ) );

  QVBoxLayout* audioBoxlayout = new QVBoxLayout ( m_audioGroupBox );
  m_audioEditor = new TableEditor ( m_audioGroupBox );
  m_audioEditor->setToolTip ( QString::fromUtf8 ( "-acodec" ) );
  audioBoxlayout->addWidget ( m_audioEditor );
  m_audioGroupBox->setLayout ( audioBoxlayout );
  // } aCodec

  // Preview {
  m_commandPreview = new CommandPreview ( toolBox );
  /*: ToolTip */
  m_commandPreview->setToolTip ( trUtf8 ( "command line preview" ) );
  toolBox->addItem ( m_commandPreview, boxIcon, trUtf8 ( "FFmpeg" ) );
  // } Preview

  layerWidget->setLayout ( verticalLayout );
  setCentralWidget ( layerWidget );

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

  connect ( m_listener, SIGNAL ( info ( const QString & ) ),
            this, SLOT ( statusBarMessage ( const QString & ) ) );

  connect ( m_listener, SIGNAL ( info ( const QString & ) ),
            m_systemTray, SLOT ( setCustomToolTip ( const QString & ) ) );

  connect ( m_grabberInfo, SIGNAL ( screenDataChanged ( bool ) ),
            this, SLOT ( toRubber ( bool ) ) );

  connect ( m_grabberInfo, SIGNAL ( showRubber ( bool ) ),
            this, SLOT ( showRubber ( bool ) ) );

  connect ( this, SIGNAL ( stopRecording () ),
            m_FFProcess, SLOT ( stop () ) );

  connect ( this, SIGNAL ( killProcess () ),
            m_FFProcess, SLOT ( kill () ) );

  connect ( m_FFProcess, SIGNAL ( down () ),
            this, SLOT ( setActionsBack () ) );

  connect ( m_commandPreview, SIGNAL ( dataSaved ( const QStringList & ) ),
            this, SLOT ( updateCommandLine ( const QStringList & ) ) );

  // Widget Updates
  connect ( m_grabberInfo, SIGNAL ( postUpdate () ),
            this, SLOT ( preparePreview () ) );

  connect ( m_metaData, SIGNAL ( postUpdate () ),
            this, SLOT ( preparePreview () ) );

  connect ( m_videoEditor, SIGNAL ( postUpdate () ),
            this, SLOT ( preparePreview () ) );

  connect ( m_audioGroupBox, SIGNAL ( toggled ( bool ) ),
            this, SLOT ( preparePreview ( bool ) ) );

  connect ( m_audioEditor, SIGNAL ( postUpdate () ),
            this, SLOT ( preparePreview () ) );

  connect ( m_commandPreview, SIGNAL ( restoreRequest () ),
            this, SLOT ( preparePreview () ) );
}

void MainWindow::record()
{
  if ( ! m_FFProcess || m_FFProcess->isRunning() )
    return;

  startRecord ();
}

void MainWindow::stop()
{
  if ( ! m_FFProcess )
    return;

  if ( m_FFProcess->isRunning() )
    m_FFProcess->stop ();
}

/**
* Initialisiert die Desktop Umgebung
*/
void MainWindow::createEnviroment()
{
  // init desktop info
  m_DesktopInfo = new DesktopInfo ( this );
  m_DesktopInfo->grabScreenGeometry ( centralWidget() );

  m_RubberBand = new RubberBand ( m_DesktopInfo->screenWidget() );
  connect ( m_RubberBand, SIGNAL ( error ( const QString &, const QString & ) ),
            this, SLOT ( pushErrorMessage ( const QString &, const QString & ) ) );

  showRubber ( cfg->showRubberOnStart() );

  toRubber ( true );
}

void MainWindow::createSystemTrayIcon()
{
  m_systemTray = new SystemTray ( this );
  connect ( m_systemTray, SIGNAL ( activated ( QSystemTrayIcon::ActivationReason ) ),
            this, SLOT ( systemTrayWatcher ( QSystemTrayIcon::ActivationReason ) ) );

  m_systemTray->show();
}

/**
* Ein/Ausblenden funktion für die Gummibandanzeige.
*/
void MainWindow::showRubber ( bool b )
{
  if ( b )
    m_RubberBand->show();
  else
    m_RubberBand->hide();
}

void MainWindow::swapRubberBand ()
{
  showRubber ( ( ( m_RubberBand->isVisible() ) ? false : true ) );
}

/**
* Lese die Fenster Geometrien neu ein.
* @note Wird immer nur beim Start und show() und hide() aufgerufen!
*/
void MainWindow::loadStats()
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
void MainWindow::saveStats()
{
  cfg->setValue ( "window/state", saveState ( 0 ) );
  cfg->setValue ( "window/position", pos() );
  cfg->setValue ( "window/size", size() );
}

/**
* Ausgabepfad erstellen und zwischenspeichern!
* \note Der Dateiname ändert sich bei jedem aufruf!
*/
const QString MainWindow::generateOutputFile()
{
  QString dest = cfg->outputDirectory();
  dest.append ( "/" );

  QString f = cfg->outputTemplateName();
  QDateTime dt = QDateTime::currentDateTime();
  QString timeStamp = QString::number ( dt.date().dayOfYear() );
  timeStamp.append ( dt.toString ( "hhmm" ) );
  f.replace ( QRegExp ( "\\b(X{3,})\\b" ), timeStamp );
  dest.append ( f );

  QString outFile;
  QString codec = videoCodec();
  if ( codec.contains ( "theora", Qt::CaseInsensitive ) )
    outFile = QString ( "%1.ogg" ).arg ( dest );
  else if ( codec.contains ( "libvpx", Qt::CaseInsensitive ) )
    outFile = QString ( "%1.webm" ).arg ( dest );
  else if ( codec.contains ( "libx264", Qt::CaseInsensitive ) )
    outFile = QString ( "%1.mp4" ).arg ( dest );
  else if ( codec.contains ( "mpeg", Qt::CaseInsensitive ) )
    outFile = QString ( "%1.mpg" ).arg ( dest );
  else
    outFile = QString ( "%1.avi" ).arg ( dest );

  cfg->setOutputPath ( outFile );
  return outFile;
}

/**
* Sende verschieben Info an Klasse @class RubberBand
*/
void MainWindow::toRubber ( bool )
{
  if ( ! m_RubberBand )
    return;

  QRect r = m_grabberInfo->getRect();
  m_RubberBand->resize ( r.width(), r.height() );
  m_RubberBand->move ( r.x(), r.y() );
  preparePreview();
}

/**
* Fenster Dimensionen abgreifen
*/
void MainWindow::grabFromWindow()
{
  if ( ! m_RubberBand )
    return;

  WindowGrabber* grabber = new WindowGrabber ( m_DesktopInfo->screenWidget() );
  QRect rect = grabber->grabWindowRect();

  if ( rect.isValid() )
  {
    m_RubberBand->show();
    m_grabberInfo->setRect ( rect );
  }
  delete grabber;
}

/**
* Statusleisten Aktionen verarbeiten
*/
void MainWindow::systemTrayWatcher ( QSystemTrayIcon::ActivationReason type )
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
* Beim Minimieren die Fenster Geometrie speichern
*/
void MainWindow::hideEvent ( QHideEvent * ev )
{
  saveStats();
  QMainWindow::hideEvent ( ev );
}

/**
* Das beenden über den WindowManager CloseButton
* verhindern! Statt dessen die Einstellungen mit
* \ref hideEvent Speichern und Hauptfenster in das
* Systray minimieren!
*/
void MainWindow::closeEvent ( QCloseEvent * ev )
{
  if ( ev->type() == QEvent::Close )
  {
    ev->ignore();
    hide();
  }
}

/**
* Informationen an die Statusleiste senden.
*/
void MainWindow::pushInfoMessage ( const QString &txt )
{
  if ( m_systemTray )
    m_systemTray->sendMessage ( trUtf8 ( "Info" ), txt, QSystemTrayIcon::Information );

  if ( ! m_FFProcess->isRunning() )
    m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
}

/**
* Fehler Meldungen an die Statusleiste senden.
*/
void MainWindow::pushErrorMessage ( const QString &title, const QString &txt )
{
  if ( m_systemTray )
  {
    m_systemTray->sendMessage ( title, txt, QSystemTrayIcon::Critical );

    if ( ! m_FFProcess->isRunning() )
      m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
  }
}

/**
* Tips an die Statusleiste senden.
*/
void MainWindow::pushToolTip ( const QString &txt )
{
  if ( m_systemTray )
    m_systemTray->setToolTip ( txt );
}

/**
* Die Daten wurden von @class CommandLineEdit modifiziert
* und müssen neu geschrieben werden.
* Das hat nur einen Einfluss auf die gespeicherte Kommandozeile
* und wird bei einem @b preparePreview wieder Überschrieben!
**/
void MainWindow::updateCommandLine ( const QStringList &cmd )
{
  if ( cmd.contains ( cfg->binaryPath() ) )
  {
    cfg->setCommandLine ( cmd );
    setWindowModified ( false );
  }
}

/**
* Starte die Aufnahme und Sperre gleichzeitig
* einige Aktionen um doppel Klicks zu vermeiden.
*/
void MainWindow::startRecord()
{
  if ( ! m_RubberBand->isScalability() || ! m_listener->setOutputFile ( outputFile() ) )
    return;

  if ( m_FFProcess->create ( m_grabberInfo->getRect() ) )
  {
    // Nehme die Editierte Zeile des Benutzers
    QStringList cmd = m_commandPreview->currentCommandLine();
    if ( cmd.size() < 1 )
    {
      statusBarMessage ( trUtf8 ( "Missing Input" ) );
      return;
    }

    showRubber ( false );
    if ( m_FFProcess->start ( cmd ) )
    {
      m_systemTray->setActionsEnabled ( true );
      m_menuBar->setActionsEnabled ( true );
      m_systemTray->setIcon ( getThemeIcon ( "media-record" ) );
      m_toolBar->setPlayerEnabled ( false );
    }
  }
  else
    QMessageBox::critical ( this, trUtf8 ( "Error" ), trUtf8 ( "qx11grap not started" ) );

  m_toolBar->setActionsEnabled ( true );
  QTimer::singleShot ( 6000, m_listener, SLOT ( start() ) );
  setWindowModified ( true );
}

/**
* Beim beenden einer Aufnahme alles in die Neutrale Stellung bringen.
*/
void MainWindow::setActionsBack()
{
  m_systemTray->setActionsEnabled ( false );
  m_menuBar->setActionsEnabled ( false );
  m_systemTray->setIcon ( getThemeIcon ( "qx11grab" ) );
  m_toolBar->setPlayerEnabled ( true );
}

/**
* Lade beim Start des Dialoges alle Einstellungen.
*/
void MainWindow::loadSettings()
{
  m_grabberInfo->load ( cfg );
  m_metaData->load ( cfg );
  m_audioGroupBox->setChecked ( cfg->value ( "SoundRecording", false ).toBool() );
  m_videoEditor->load ( QString::fromUtf8 ( "VideoOptions" ), cfg );
  m_audioEditor->load ( QString::fromUtf8 ( "AudioOptions" ), cfg );
  preparePreview();
  setWindowModified ( false );
}

/**
* Speichere alle Einstellungen.
*/
void MainWindow::saveSettings()
{
  cfg->setValue ( "SoundRecording", m_audioGroupBox->isChecked() );
  m_grabberInfo->save ( cfg );
  m_metaData->save ( cfg );
  m_videoEditor->save ( QString::fromUtf8 ( "VideoOptions" ), cfg );
  m_audioEditor->save ( QString::fromUtf8 ( "AudioOptions" ), cfg );
  setWindowModified ( false );
}

/**
* Log Dialog öffnen
*/
void MainWindow::openLogFileDialog()
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
*/
void MainWindow::preparePreview ( bool b )
{
  Q_UNUSED ( b );

  QStringList commandLine;
  commandLine << cfg->binaryPath();
  commandLine << "-xerror";
  commandLine << "-loglevel" << cfg->logLevel();
  commandLine << "-f" << "x11grab";
  commandLine << "-framerate" << QString::number ( m_grabberInfo->frameRate() );

  // Dimension
  QX11Info xInfo;
  QRect r = m_grabberInfo->getRect();
  QString geometry = QString ( "%1x%2" ).arg ( QString::number ( r.width() ), QString::number ( r.height() ) );
  /* WARNING ordered SIZE before POINT is implicit required
  * or else FFmpeg didnt correct scale the stream */
  commandLine << "-video_size" << geometry;
  commandLine << "-i" << QString ( ":%1.%2+%3,%4 " ) .arg (
      QString::number ( xInfo.screen() ),
      QString::number ( xInfo.appScreen() ),
      QString::number ( r.x() ),
      QString::number ( r.y() )
  );

  // Decoder
  commandLine << "-dcodec" << "copy";

  // Audio System
  if ( m_audioGroupBox->isChecked() )
    commandLine << cfg->getAudioDeviceCommand();

  // Experts
  if ( cfg->expertMode() )
  {
    QStringList expert = cfg->getExpertCommand();
    if ( expert.size() > 0 )
      commandLine << expert;
  }

  // Video Options
  commandLine << m_videoEditor->getCmd ();

  // Meta Daten
  if ( m_metaData->isChecked() )
    commandLine << m_metaData->getCmd ( videoCodec() );

  // Audio Aufnahme
  if ( m_audioGroupBox->isChecked() )
    commandLine << m_audioEditor->getCmd ();

  // Output Options
  QString outFile = generateOutputFile();
  commandLine << "-y" << outFile;

  m_commandPreview->setCommandLine ( commandLine );

  cfg->setValue ( QLatin1String ( "SoundRecording" ), m_audioGroupBox->isChecked() );
  cfg->setCommandLine ( commandLine );

  setWindowModified ( true );
}

/**
* Liest die aktuelle Kommandozeile aus der Konfiguration
*/
const QString MainWindow::currentCommandLine()
{
  QStringList cmd = cfg->getCommandline();
  return cmd.join ( " " );
}

/**
* Aktuelle Kommando Zeile in Shell Script exportieren!
*/
void MainWindow::exportCommand()
{
  ExportDialog* d = new ExportDialog ( currentCommandLine(), this );
  if ( d->exec() == QFileDialog::Accepted )
    statusBarMessage ( trUtf8 ( "commandline exported" ) );

  delete d;
}

/**
* Leszeichen Editor erstellen
*/
void MainWindow::openCreateBookmark()
{
  BookmarkDialog* d = new BookmarkDialog ( this );
  d->setVCodecOptions ( m_videoEditor->selectedCodec(), m_videoEditor->getTableItems () );
  d->setACodecOptions ( m_audioEditor->selectedCodec(), m_audioEditor->getTableItems () );
  d->exec();
  delete d;
}

/**
* Leszeichen Editor entfernen
*/
void MainWindow::openRemoveBookmark()
{
  Bookmark doc;
  if ( doc.open() )
  {
    bool ok;
    QStringList selection ( trUtf8 ( "" ) );
    selection << doc.entries();
    QString br = QInputDialog::getItem ( this, trUtf8 ( "Remove Bookmark" ), trUtf8 ( "Bookmark" ), selection, 0, false, &ok );
    if ( ! br.isEmpty() &&  doc.removeEntryById ( br ) )
    {
      QDBusInterface iface ( "de.hjcms.qx11grab", "/BookmarkSelect", "de.hjcms.qx11grab.BookmarkSelecter" );
      iface.call ( "reload" );
    }
  }
}

/**
* Leszeichen Öffnen siehe Toolbar
*/
void MainWindow::openBookmark ( const QString &id )
{
  Bookmark doc;
  if ( doc.open() )
  {
    BookmarkEntry entry = doc.entry ( id );
    if ( ! entry.isValid() )
    {
      statusBarMessage ( trUtf8 ( "Open Bookmark %1 failed" ).arg ( id ), 5000 );
      return;
    }

    // Video
    m_videoEditor->setCodecByName ( entry.getCodecName ( BookmarkEntry::VCODEC ) );
    m_videoEditor->setCodecOptions ( entry.getCodecOptions ( BookmarkEntry::VCODEC ) );
    // Audio
    m_audioEditor->setCodecByName ( entry.getCodecName ( BookmarkEntry::ACODEC ) );
    m_audioEditor->setCodecOptions ( entry.getCodecOptions ( BookmarkEntry::ACODEC ) );

    statusBarMessage ( trUtf8 ( "Open Bookmark %1" ).arg ( id ), 5000 );
  }
}

/**
* Configurations Dialog Öffnen
*/
void MainWindow::openConfiguration()
{
  ConfigDialog* d = new ConfigDialog ( cfg, this );
  if ( d->exec() )
    preparePreview();

  delete d;
}

/**
* Schreibe Nachricht in das Meldungs Label
*/
void MainWindow::statusBarMessage ( const QString &msg, int timeout )
{
  statusBar()->showMessage ( msg, timeout );
}

/** Wird für DBUS und ItemDelegation benötigt! */
const QString MainWindow::audioCodec()
{
  return m_audioEditor->selectedCodec();
}

/** Wird für DBUS und ItemDelegation benötigt! */
const QString MainWindow::videoCodec()
{
  return m_videoEditor->selectedCodec();
}

/** Wird für DBUS benötigt! */
const QString MainWindow::outputFile()
{
  return cfg->absoluteOutputPath();
}

/**
* Teile der System Statusleiste mit das sie
* DBus Notification verwenden soll!
*/
void MainWindow::registerMessanger ( QDBusConnection* bus )
{
  if ( ! bus || ! m_systemTray )
    return;

  m_systemTray->setMessanger ( bus );
}

MainWindow::~MainWindow()
{
  if ( m_listener )
    delete m_listener;
}
