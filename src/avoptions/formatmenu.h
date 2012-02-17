/**
* This file is part of the qx11grab project
*
* Copyright (C) Juergen Heinemann (Undefined) http://qx11grab.hjcms.de, (C) 2007-2012
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

#ifndef FormatMenu_H
#define FormatMenu_H

/* QtCore */
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>

/* QtGui */
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtGui/QMenu>
#include <QtGui/QToolButton>
#include <QtGui/QWidget>

/* QX11Grab */
#include "avoptions.h"

namespace QX11Grab
{
  class FormatMenu : public QToolButton
  {
      Q_OBJECT
      Q_CLASSINFO ( "Author", "Jürgen Heinemann (Undefined)" )
      Q_CLASSINFO ( "URL", "http://qx11grab.hjcms.de" )

    private:
      const QIcon p_Icon;
      QMenu* m_menu;
      QList<QAction*> p_ActionsList;

      const QString findDefaultExtension ( const QString &name );

    private Q_SLOTS:
      void itemTriggered ( QAction * );

    Q_SIGNALS:
      void postUpdate();
      void extensionChanged ( const QString & );

    public Q_SLOTS:
      void updateMenu ( const QString &name, CodecID id );

    public:
      explicit FormatMenu ( QWidget * parent = 0 );
      void setEntryEnabled ( const QString & );
      virtual ~FormatMenu();
  };
}  /* eof namespace QX11Grab */

#endif
