/**
* This file is part of the qx11grab project
*
* Copyright (C) Juergen Heinemann http://qx11grab.hjcms.de, (C) 2007-2010
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

#include "audiodevice.h"

/* QtCore */
#include <QtCore/QString>

/* QtGui */
#include <QtGui/QGridLayout>
#include <QtGui/QGroupBox>
#include <QtGui/QLabel>
#include <QtGui/QRadioButton>
#include <QtGui/QVBoxLayout>

AudioDevice::AudioDevice ( QWidget * parent )
    : QWidget ( parent )
{
  setObjectName ( QLatin1String ( "audiodevice" ) );
  setContentsMargins ( 0, 2, 0, 2 );

  QVBoxLayout* vLayout = new QVBoxLayout ( this );
  vLayout->setContentsMargins ( 0, 2, 0, 2 );

  QGroupBox* audioGroup = new QGroupBox ( trUtf8 ( "Audio Settings" ), this );
  audioGroup->setFlat ( true );
  QGridLayout* gridLayout = new QGridLayout ( audioGroup );

  m_swap_alsa = new QRadioButton ( trUtf8 ( "Advanced Linux Sound Architecture (Alsa)" ), audioGroup );
  gridLayout->addWidget ( m_swap_alsa, 0, 0, 1, 1 );

  m_swap_oss = new QRadioButton ( trUtf8 ( "Open Sound System (OSS)" ), audioGroup );
  gridLayout->addWidget ( m_swap_oss, 0, 1, 1, 1 );
  // Setze Alsa als Standard
  m_swap_alsa->setChecked ( true );

  QLabel* txt0 = new QLabel ( audioGroup );
  txt0->setText ( trUtf8 ( "Audio Intensifier:" ) );
  txt0->setAlignment ( ( Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter ) );
  gridLayout->addWidget ( txt0, 1, 0, 1, 1 );

  intensifier = new QSpinBox ( audioGroup );
  intensifier->setSingleStep ( 2 );
  intensifier->setRange ( 0, 512 );
  intensifier->setValue ( 256 );
  intensifier->setObjectName ( QLatin1String ( "audio_intensifier" ) );
  intensifier->setToolTip ( trUtf8 ( "Change Audio Volume (256=normal)" ) );
  intensifier->setWhatsThis ( trUtf8 ( "Change Audio Amplifier.\nDefault: 256=normal" ) );
  gridLayout->addWidget ( intensifier, 1, 1, 1, 1 );

  QLabel* txt1 = new QLabel ( audioGroup );
  txt1->setText ( trUtf8 ( "Audio Capture Device:" ) );
  txt1->setAlignment ( ( Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter ) );
  gridLayout->addWidget ( txt1, 2, 0, 1, 1 );

  device = new QLineEdit ( audioGroup );
  device->setObjectName ( QLatin1String ( "device" ) );
  device->setWhatsThis ( trUtf8 ( "Change Audio Capture Device\nOSS Default: /dev/dsp or Alsa Default: default" ) );

  gridLayout->addWidget ( device, 2, 1, 1, 1 );

  vLayout->addWidget ( audioGroup );
  vLayout->addStretch ( 1 );

  setLayout ( vLayout );
}

void AudioDevice::setVolume ( int i )
{
  intensifier->setValue ( i );
}

int AudioDevice::getVolume ()
{
  return intensifier->value();
}

void AudioDevice::setAudioEngine ( const QString &t )
{
  if ( t.contains ( QString::fromUtf8 ( "alsa" ) ) )
    m_swap_alsa->setChecked ( true );
  else
    m_swap_oss->setChecked ( true );
}

const QString AudioDevice::getAudioEngine ()
{
  return m_swap_alsa->isChecked() ? QLatin1String ( "alsa" ) : QLatin1String ( "oss" );
}

void AudioDevice::setAudioDevice ( const QString &d )
{
  device->setText ( d );
}

const QString AudioDevice::getAudioDevice ()
{
  return device->text();
}

const QStringList AudioDevice::data()
{
  QStringList cmd;
  if ( device->text().isEmpty() )
    return cmd;

  if ( m_swap_alsa->isChecked() )
    cmd << "-f" << "alsa" << "-i" << device->text();
  else
    cmd << "-f" << "oss" << "-i" << device->text();

  if ( intensifier->value() != 256 )
    cmd << "-vol" << QString::number ( intensifier->value() );

  return cmd;
}

AudioDevice::~AudioDevice()
{}
