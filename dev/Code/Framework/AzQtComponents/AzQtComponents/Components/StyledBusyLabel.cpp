/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzQtComponents/Components/StyledBusyLabel.h>

#include <QLabel>
AZ_PUSH_DISABLE_WARNING(4244 4251, "-Wunknown-warning-option") // 4251: 'QImageIOHandler::d_ptr': class 'QScopedPointer<QImageIOHandlerPrivate,QScopedPointerDeleter<T>>' needs to have dll-interface to be used by clients of class 'QImageIOHandler'
#include <QMovie>
AZ_POP_DISABLE_WARNING
#include <QSvgWidget>
#include <QSvgRenderer>
#include <QHBoxLayout>

namespace AzQtComponents
{
    StyledBusyLabel::StyledBusyLabel(QWidget* parent)
        : QWidget(parent)
        , m_busyIcon(new QSvgWidget(this))
        , m_oldBusyIcon(new QLabel(this))
        , m_text(new QLabel(this))
    {
        setLayout(new QHBoxLayout);
        layout()->setSpacing(6);
        layout()->addWidget(m_busyIcon);
        layout()->addWidget(m_oldBusyIcon);
        layout()->addWidget(m_text);
        m_oldBusyIcon->setMovie(new QMovie(this));
        m_oldBusyIcon->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        m_busyIcon->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        m_busyIcon->setVisible(false);
        m_text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);

        loadDefaultIcon();
    }

    bool StyledBusyLabel::unpolish(Style* style, QWidget* widget)
    {
        Q_UNUSED(style);
        auto busyLabel = qobject_cast<StyledBusyLabel*>(widget);
        if (busyLabel)
        {
            busyLabel->SetUseNewWidget(false);
        }
        return busyLabel;
    }

    bool StyledBusyLabel::polish(Style* style, QWidget* widget)
    {
        Q_UNUSED(style);
        auto busyLabel = qobject_cast<StyledBusyLabel*>(widget);
        if (busyLabel)
        {
            busyLabel->SetUseNewWidget(true);
        }
        return busyLabel;
    }

    void StyledBusyLabel::loadDefaultIcon()
    {
        SetBusyIcon(m_useNewWidget ? ":/stylesheet/img/loading.svg" : ":/stylesheet/img/in_progress.gif");
    }

    void StyledBusyLabel::SetUseNewWidget(bool usenew)
    {
        if (m_useNewWidget != usenew)
        {
            m_useNewWidget = usenew;
            loadDefaultIcon();
            m_oldBusyIcon->setVisible(!m_useNewWidget);
            m_busyIcon->setVisible(m_useNewWidget);
            if (m_useNewWidget)
            {
                m_oldBusyIcon->movie()->stop();
            }
            else
            {
                m_oldBusyIcon->movie()->start();
            }
            updateMovie();
        }
    }

    bool StyledBusyLabel::GetIsBusy() const
    {
        return m_useNewWidget ? m_busyIcon->isVisible() : m_oldBusyIcon->movie()->state() == QMovie::Running;
    }

    void StyledBusyLabel::SetIsBusy(bool busy)
    {
        if (m_isBusy != busy)
        {
            m_isBusy = busy;
            updateMovie();
        }
    }

    QString StyledBusyLabel::GetText() const
    {
        return m_text->text();
    }

    void StyledBusyLabel::SetText(const QString& text)
    {
        m_text->setText(text);
    }

    QString StyledBusyLabel::GetBusyIcon() const
    {
        return m_fileName;
    }

    void StyledBusyLabel::SetBusyIcon(const QString& iconSource)
    {
        m_fileName = iconSource;

        if (m_useNewWidget)
        {
            m_busyIcon->renderer()->load(iconSource);
        }
        else
        {
            m_oldBusyIcon->movie()->setFileName(iconSource);
            m_oldBusyIcon->setFixedWidth(32);
            m_oldBusyIcon->movie()->setScaledSize(QSize(height(), height()));
        }

        updateMovie();
    }

    int StyledBusyLabel::GetBusyIconSize() const
    {
        return m_busyIconSize;
    }

    void StyledBusyLabel::SetBusyIconSize(int iconSize)
    {
        if (m_busyIconSize != iconSize)
        {
            m_busyIconSize = iconSize;
            updateMovie();
        }
    }

    QSize StyledBusyLabel::sizeHint() const
    {
        return QSize(m_busyIconSize, m_busyIconSize);
    }

    void StyledBusyLabel::updateMovie()
    {
        if (m_isBusy)
        {
            if (!m_useNewWidget)
            {
                m_oldBusyIcon->movie()->start();
            }
            else
            {
                m_busyIcon->show();
            }
        }
        else
        {
            if (!m_useNewWidget)
            {
                m_oldBusyIcon->movie()->stop();
            }
            else
            {
                m_busyIcon->hide();
            }
        }
        if (!m_useNewWidget)
        {
            m_oldBusyIcon->setFixedWidth(m_busyIconSize);
            m_oldBusyIcon->movie()->setScaledSize(QSize(m_busyIconSize, m_busyIconSize));
        }
        else
        {
            m_busyIcon->setFixedSize(m_busyIconSize, m_busyIconSize);
        }
    }

#include <Components/StyledBusyLabel.moc>
} // namespace AzQtComponents
