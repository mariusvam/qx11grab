/***
* Author: Juergen Heinemann http://www.hjcms.de, (C) 2007-2010
* Copyright: See COPYING file that comes with this distribution
**/

#include "settingsdialog.h"
#include "settings.h"

/* QtCore */
#include <QtCore/QDebug>

/* QtGui */
#include <QtGui/QPushButton>

SettingsDialog::SettingsDialog ( QWidget *parent, Settings *cfg )
    : QDialog ( parent )
    , m_Settings ( cfg )
{
  setupUi ( this );

  /* Save Button */
  btn_apply = new QPushButton ( trUtf8 ( "Apply" ) );
  buttonBox->addButton ( btn_apply, QDialogButtonBox::ActionRole );
  /* Cancel Button */
  btn_cancel = new QPushButton ( trUtf8 ( "Close" ) );
  buttonBox->addButton ( btn_cancel, QDialogButtonBox::RejectRole );

  if ( m_Settings->contains ( "qx11grab/options" ) )
    m_SettingsPageOne->setOptions ( m_Settings->value ( "qx11grab/options" ).toMap() );

  if ( m_Settings->contains ( "ffmpeg/options" ) )
    m_SettingsPageTwo->setOptions ( m_Settings->value ( "ffmpeg/options" ).toMap() );
  else
    m_SettingsPageTwo->setTableDefaults();

  connect ( listWidget,
            SIGNAL ( currentItemChanged ( QListWidgetItem *, QListWidgetItem * ) ),
            this, SLOT ( changeSettings ( QListWidgetItem *, QListWidgetItem* ) ) );

  connect ( btn_apply, SIGNAL ( clicked() ), this, SLOT ( savepages() ) );

  connect ( buttonBox, SIGNAL ( rejected() ), this, SLOT ( close() ) );
}

void SettingsDialog::savepages()
{
  m_SettingsPageOne->saveOptions( m_Settings );
  m_SettingsPageTwo->saveOptions( m_Settings );
}

void SettingsDialog::changeSettings ( QListWidgetItem *current, QListWidgetItem *previous )
{
  if ( !current )
    current = previous;

  stackedWidget->setCurrentIndex ( listWidget->row ( current ) );
}

void SettingsDialog::closeEvent ( QCloseEvent * )
{
  if ( ! m_Settings->contains ( "qx11grab/options" ) )
    savepages();
  else if ( ! m_Settings->contains ( "ffmpeg" ) )
    savepages();
}

SettingsDialog::~SettingsDialog()
{
}


